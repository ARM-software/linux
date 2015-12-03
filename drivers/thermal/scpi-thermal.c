#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scpi_protocol.h>
#include <linux/thermal.h>
#include <linux/topology.h>

#define SOC_SENSOR "SENSOR_TEMP_SOC"

enum cluster_type {
	CLUSTER_BIG = 0,
	CLUSTER_LITTLE,
	NUM_CLUSTERS
};

struct cluster_power_coefficients {
	int dyn_coeff;
	int static_coeff;
	int cache_static_coeff;
};

struct cluster_power_coefficients cluster_data[] = {
	[CLUSTER_BIG] = {
		.dyn_coeff = 530,
		.static_coeff = 103,		/* 75 mW @ 85C/0.9V */
		.cache_static_coeff = 88,	/* 64 mW @ 85C/0.9V */
	},
	[CLUSTER_LITTLE] = {
		.dyn_coeff = 140,
		.static_coeff = 36,		/* 26 mW @ 85C/0.9V */
		.cache_static_coeff = 73,	/* 53 mW @ 85C/0.9V */
	},
};

struct scpi_sensor {
	u16 sensor_id;
	unsigned long prev_temp;
	u32 alpha;
	struct thermal_zone_device *tzd;
	struct thermal_zone_device *gpu_tzd;
	struct cpumask cluster[NUM_CLUSTERS];
	struct thermal_cooling_device *cdevs[NUM_CLUSTERS];
};
static struct scpi_sensor scpi_temp_sensor;

static struct scpi_ops *scpi_ops;

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

/**
 * mul_frac() - multiply two fixed-point numbers
 * @x:	first multiplicand
 * @y:	second multiplicand
 *
 * Return: the result of multiplying two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

static unsigned long get_temperature_scale(unsigned long temp)
{
	int i, t_exp = 1, t_scale = 0;
	int coeff[] = { 32000, 4700, -80, 2 }; // * 1E6

	for (i = 0; i < 4; i++) {
		t_scale += coeff[i] * t_exp;
		t_exp *= temp;
	}

	return t_scale / 1000; // the value returned needs to be /1E3
}

static unsigned long get_voltage_scale(unsigned long u_volt)
{
	unsigned long m_volt = u_volt / 1000;
	unsigned long v_scale;

	v_scale = m_volt * m_volt * m_volt; // = (m_V^3) / (900 ^ 3) =

	return v_scale / 1000000; // the value returned needs to be /(1E3)
}

/* voltage in uV and temperature in mC */
static int get_static_power(cpumask_t *cpumask, int interval,
		unsigned long u_volt, u32 *power)
{
	struct thermal_zone_device *tzd = scpi_temp_sensor.tzd;
	unsigned long t_scale, v_scale, milli_temp = 0;
	u32 cpu_coeff;
	int nr_cpus = cpumask_weight(cpumask);
	enum cluster_type cluster =
		topology_physical_package_id(cpumask_any(cpumask));

	cpu_coeff = cluster_data[cluster].static_coeff;

	if (tzd)
		milli_temp = tzd->temperature;

	t_scale = get_temperature_scale(milli_temp / 1000);
	v_scale = get_voltage_scale(u_volt);

	*power = nr_cpus * (cpu_coeff * t_scale * v_scale) / 1000000;

	if (nr_cpus) {
		u32 cache_coeff = cluster_data[cluster].cache_static_coeff;
		*power += (cache_coeff * v_scale * t_scale) / 1000000; /* cache leakage */
	}

	return 0;
}

static int get_temp_value(void *data, long *temp)
{
	struct scpi_sensor *sensor = (struct scpi_sensor *)data;
	u32 val;
	int ret;
	unsigned long est_temp;

	ret = scpi_ops->sensor_get_value(sensor->sensor_id, &val);
	if (ret)
		return ret;

	if (!sensor->prev_temp)
		sensor->prev_temp = val;

	est_temp = mul_frac(sensor->alpha, val) +
		mul_frac((int_to_frac(1) - sensor->alpha), sensor->prev_temp);

	sensor->prev_temp = est_temp;
	*temp = est_temp;

	return 0;
}

static int get_temp_for_gpu(struct thermal_zone_device *tz, unsigned long *temp)
{
	struct scpi_sensor *sensor = (struct scpi_sensor *)tz->devdata;

	*temp = sensor->tzd->temperature;

	return 0;
}

static void update_debugfs(struct scpi_sensor *sensor_data)
{
	struct dentry *dentry_f, *filter_d;

	filter_d = debugfs_create_dir("thermal_lpf_filter", NULL);
	if (IS_ERR_OR_NULL(filter_d)) {
		pr_warning("unable to create debugfs directory for the LPF filter\n");
		return;
	}

	dentry_f = debugfs_create_u32("alpha", S_IWUSR | S_IRUGO, filter_d,
				      &sensor_data->alpha);
	if (IS_ERR_OR_NULL(dentry_f)) {
		pr_warn("Unable to create debugfsfile: alpha\n");
		return;
	}
}

static struct thermal_zone_of_device_ops scpi_of_ops = {
	.get_temp = get_temp_value,
};

static struct thermal_zone_device_ops gpu_dummy_tz_ops = {
	.get_temp = get_temp_for_gpu,
};

static struct thermal_zone_params gpu_dummy_tzp = {
	.governor_name = "user_space",
};

int sensor_get_id(const char* name)
{
	u16 sensors, sensor_id;
	int ret;

	ret = scpi_ops->sensor_get_capability(&sensors);
	if (ret)
		return ret;

	for (sensor_id = 0; sensor_id < sensors; sensor_id++) {

		struct scpi_sensor_info info;
		ret = scpi_ops->sensor_get_info(sensor_id, &info);
		if (ret)
			return ret;

		if (strcmp(name, info.name) == 0)
			return sensor_id;
	}

	return -ENODEV;
}

static int scpi_thermal_probe(struct platform_device *pdev)
{
	struct scpi_sensor *sensor_data = &scpi_temp_sensor;
	struct device_node *np;
	int sensor, cpu;
	int i, j;

	scpi_ops = get_scpi_ops();
	if (!scpi_ops)
		return -EPROBE_DEFER;

	if (!cpufreq_frequency_get_table(0))
		return -EPROBE_DEFER;

	platform_set_drvdata(pdev, sensor_data);

	for_each_possible_cpu(cpu) {
		int cluster_id = topology_physical_package_id(cpu);
		if (cluster_id > NUM_CLUSTERS) {
			dev_warn(&pdev->dev, "Cluster id: %d > %d\n",
				 cluster_id, NUM_CLUSTERS);
			goto error;
		}

		cpumask_set_cpu(cpu, &sensor_data->cluster[cluster_id]);
	}

	for (i = 0, j = 0; i < NUM_CLUSTERS; i++) {
		char node[16];

		snprintf(node, 16, "cluster%d", i);
		np = of_find_node_by_name(NULL, node);

		if (!np) {
			dev_info(&pdev->dev, "Node not found: %s\n", node);
			continue;
		}

		sensor_data->cdevs[j] =
			of_cpufreq_power_cooling_register(np,
							  &sensor_data->cluster[i],
							  cluster_data[i].dyn_coeff,
							  get_static_power);

		if (IS_ERR(sensor_data->cdevs[i])) {
			dev_warn(&pdev->dev,
				"Error registering cooling device: %s\n", node);
			continue;
		}
		j++;
	}

	if ((sensor = sensor_get_id(SOC_SENSOR)) < 0) {
		dev_warn(&pdev->dev, "%s not found. ret=%d\n", SOC_SENSOR, sensor);
		goto error;
	}

	sensor_data->sensor_id = (u16)sensor;
	dev_info(&pdev->dev, "Probed %s sensor. Id=%hu\n", SOC_SENSOR, sensor_data->sensor_id);

	/*
	 * alpha ~= 2 / (N + 1) with N the window of a rolling mean We
	 * use 8-bit fixed point arithmetic.  For a rolling average of
	 * window 20, alpha = 2 / (20 + 1) ~= 0.09523809523809523 .
	 * In 8-bit fixed point arigthmetic, 0.09523809523809523 * 256
	 * ~= 24
	 */
	sensor_data->alpha = 24;

	sensor_data->tzd = thermal_zone_of_sensor_register(&pdev->dev,
							sensor_data->sensor_id,
							sensor_data,
							&scpi_of_ops);

	if (IS_ERR(sensor_data->tzd)) {
		dev_warn(&pdev->dev, "Error registering sensor: %ld\n", PTR_ERR(sensor_data->tzd));
		return PTR_ERR(sensor_data->tzd);
	}

	sensor_data->gpu_tzd = thermal_zone_device_register("gpu", 0, 0,
							    sensor_data,
							    &gpu_dummy_tz_ops,
							    &gpu_dummy_tzp,
							    0, 0);
	if (IS_ERR(sensor_data->gpu_tzd)) {
		int ret = PTR_ERR(sensor_data->gpu_tzd);

		dev_warn(&pdev->dev, "Error register gpu thermal zone: %d\n",
			 ret);
		return ret;
	}

	update_debugfs(sensor_data);

	return 0;

error:
	return -ENODEV;
}

static int scpi_thermal_remove(struct platform_device *pdev)
{
	struct scpi_sensor *sensor = platform_get_drvdata(pdev);

	thermal_zone_device_unregister(sensor->tzd);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct of_device_id scpi_thermal_of_match[] = {
	{ .compatible = "arm,scpi-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, scpi_thermal_of_match);

static struct platform_driver scpi_thermal_platdrv = {
	.driver = {
		.name		= "scpi-thermal",
		.owner		= THIS_MODULE,
		.of_match_table = scpi_thermal_of_match,
	},
	.probe	= scpi_thermal_probe,
	.remove	= scpi_thermal_remove,
};
module_platform_driver(scpi_thermal_platdrv);

MODULE_LICENSE("GPL");
