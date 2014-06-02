#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scpi_protocol.h>
#include <linux/thermal.h>
#include <linux/topology.h>

#define SOC_SENSOR "SENSOR_TEMP_SOC"

#define NUM_CLUSTERS 2

struct scpi_sensor {
	u16 sensor_id;
	struct thermal_zone_device *tzd;
	struct cpumask cluster[NUM_CLUSTERS];
	struct thermal_cooling_device *cdevs[NUM_CLUSTERS];
};
static struct scpi_sensor scpi_temp_sensor;

static struct scpi_ops *scpi_ops;

static int get_temp_value(void *data, long *temp)
{
	struct scpi_sensor *sensor = (struct scpi_sensor *)data;
	u32 val;
	int ret;

	ret = scpi_ops->sensor_get_value(sensor->sensor_id, &val);

	if (!ret)
		*temp = (unsigned long)val;

	return ret;
}

static struct thermal_zone_of_device_ops scpi_of_ops = {
	.get_temp = get_temp_value,
};

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
			of_cpufreq_cooling_register(np,
						&sensor_data->cluster[i]);

		if (IS_ERR(sensor_data->cdevs[i])) {
			dev_warn(&pdev->dev,
				"Error registering cooling device: %s\n", node);
			continue;
		}
		j++;
	}

	if ((sensor = scpi_ops->sensor_get_id(SOC_SENSOR)) < 0) {
		dev_warn(&pdev->dev, "%s not found. ret=%d\n", SOC_SENSOR, sensor);
		goto error;
	}

	sensor_data->sensor_id = (u16)sensor;
	dev_info(&pdev->dev, "Probed %s sensor. Id=%hu\n", SOC_SENSOR, sensor_data->sensor_id);

	sensor_data->tzd = thermal_zone_of_sensor_register(&pdev->dev,
							sensor_data->sensor_id,
							sensor_data,
							&scpi_of_ops);

	if (IS_ERR(sensor_data->tzd)) {
		dev_warn(&pdev->dev, "Error registering sensor: %ld\n", PTR_ERR(sensor_data->tzd));
		return PTR_ERR(sensor_data->tzd);
	}

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
