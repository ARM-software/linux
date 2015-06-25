#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scpi_protocol.h>
#include <linux/thermal.h>
#include <linux/topology.h>

#define SOC_SENSOR "SENSOR_TEMP_SOC"

struct scpi_sensor {
	u16 sensor_id;
	struct thermal_zone_device *tzd;
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
	int sensor;

	scpi_ops = get_scpi_ops();
	if (!scpi_ops)
		return -EPROBE_DEFER;

	platform_set_drvdata(pdev, sensor_data);

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

	thermal_zone_device_update(sensor_data->tzd);

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
