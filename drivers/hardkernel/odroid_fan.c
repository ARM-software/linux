//[*]--------------------------------------------------------------------------------------------------[*]
//
//  ODROID Board : ODROID FAN driver
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/pwm.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#define	DEBUG_PM_MSG
#include	<linux/platform_data/odroid_fan.h>

extern unsigned long exynos_thermal_get_value(void);

#define TEMP_LEVEL_0	57
#define TEMP_LEVEL_1	63
#define TEMP_LEVEL_2	68

//duty percent
#define FAN_SPEED_0		1
#define FAN_SPEED_1		51
#define FAN_SPEED_2		71
#define FAN_SPEED_3		91

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*
 * driver private data
 */
struct odroid_fan {
	struct odroid_fan_platform_data *pdata;
	struct delayed_work		work;
	struct pwm_device 		*pwm;

	struct mutex		mutex;
	int	pwm_status;
	int	fan_mode;

	int period;
	int duty;
	int pwm_id;

	int tmu_level_0;
	int tmu_level_1;
	int tmu_level_2;

	int fan_speed_0;
	int fan_speed_1;
	int fan_speed_2;
	int fan_speed_3;
};

//[*]------------------------------------------------------------------------------------------------------------------
//
// driver sysfs attribute define
//
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_pwm_enable	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	ssize_t show_pwm_status	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(pwm_enable, S_IRWXUGO, show_pwm_status, set_pwm_enable);

static	ssize_t set_fan_mode	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	ssize_t show_fan_mode	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(fan_mode, S_IRWXUGO, show_fan_mode, set_fan_mode);

static	ssize_t set_pwm_duty	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	ssize_t show_pwm_duty	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(pwm_duty, S_IRWXUGO, show_pwm_duty, set_pwm_duty);

static	ssize_t set_temp_levels		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	ssize_t show_temp_levels	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(temp_levels, S_IRWXUGO, show_temp_levels, set_temp_levels);

static	ssize_t set_fan_speeds	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	ssize_t show_fan_speeds	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(fan_speeds, S_IRWXUGO, show_fan_speeds, set_fan_speeds);

static struct attribute *odroid_fan_sysfs_entries[] = {
	&dev_attr_pwm_enable.attr,
	&dev_attr_fan_mode.attr,
	&dev_attr_pwm_duty.attr,
	&dev_attr_temp_levels.attr,
	&dev_attr_fan_speeds.attr,
	NULL
};

static struct attribute_group odroid_fan_attr_group = {
	.name   = NULL,
	.attrs  = odroid_fan_sysfs_entries,
};


//[*]------------------------------------------------------------------------------------------------------------------
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_pwm_enable	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;
	printk("PWM_0 : %s [%d] \n",__FUNCTION__,val);

	mutex_lock(&fan->mutex);
    if(val) {
    	fan->pwm_status = 1;
		pwm_disable(fan->pwm);
		pwm_config(fan->pwm, fan->duty * fan->period / 255, fan->period);
    	pwm_enable(fan->pwm);
    }
    else {
    	pwm_disable(fan->pwm);
		pwm_config(fan->pwm, 0, fan->period);
    	fan->pwm_status = 0;
    }
	mutex_unlock(&fan->mutex);

	return count;
}

static	ssize_t show_pwm_status	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);

	if(fan->pwm_status)	return	sprintf(buf, "PWM_0 : %s\n", "on");
	else					return	sprintf(buf, "PWM_0 : %s\n", "off");
}

//[*]------------------------------------------------------------------------------------------------------------------
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_fan_mode	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;
	printk("PWM_0 : %s [%d] \n",__FUNCTION__,val);

	mutex_lock(&fan->mutex);
    if(val) fan->fan_mode = 1;
    else {
    	fan->duty = 255;
    	pwm_disable(fan->pwm);
		pwm_config(fan->pwm, fan->duty * fan->period / 255, fan->period);
    	pwm_enable(fan->pwm);
    	fan->fan_mode = 0;
    }
	mutex_unlock(&fan->mutex);

	if(fan->fan_mode) {
		schedule_delayed_work(&fan->work, msecs_to_jiffies(3500));
	}

	return count;
}

static	ssize_t show_fan_mode	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);

	if(fan->fan_mode)	return	sprintf(buf, "fan_mode %s\n", "auto");
	else				return	sprintf(buf, "fan_mode %s\n", "manual");
}

//[*]------------------------------------------------------------------------------------------------------------------
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_pwm_duty	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

	if((val > 256)||(val < 0)){
		printk("PWM_0 : Invalid param. Duty cycle range is 0 to 255 \n");
		return count;
	}

	printk("PWM_0 : %s [%d] \n",__FUNCTION__,val);

	mutex_lock(&fan->mutex);
	fan->duty = val;

    if(fan->pwm_status){
		pwm_disable(fan->pwm);
		pwm_config(fan->pwm, fan->duty * fan->period / 255, fan->period);
    	pwm_enable(fan->pwm);
    }
    else {
		pwm_disable(fan->pwm);
		pwm_config(fan->pwm, 0, fan->period);
    }
	mutex_unlock(&fan->mutex);
	
	return count;
}

static	ssize_t show_pwm_duty	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);

	return	sprintf(buf, "%d\n", fan->duty);
}

//[*]------------------------------------------------------------------------------------------------------------------
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_temp_levels	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	unsigned int	level_0, level_1, level_2;

	if(sscanf(buf, "%u %u %u\n", &level_0, &level_1, &level_2) != 3)	return	-EINVAL;

	if(!(level_0 < level_1 && level_1 < level_2)){
		printk("temperature levels must be increasing in value\n");
		return count;
	}

	printk("temp_levels : %s [%d %d %d] \n",__FUNCTION__, level_0, level_1, level_2);

	mutex_lock(&fan->mutex);
	fan->tmu_level_0 = level_0;
	fan->tmu_level_1 = level_1;
	fan->tmu_level_2 = level_2;
	mutex_unlock(&fan->mutex);

	return count;
}

static	ssize_t show_temp_levels	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	ssize_t result;

	mutex_lock(&fan->mutex);
	result = sprintf(buf, "%d %d %d\n", fan->tmu_level_0, fan->tmu_level_1, fan->tmu_level_2);
	mutex_unlock(&fan->mutex);
	return	result;
}

//[*]------------------------------------------------------------------------------------------------------------------
//[*]------------------------------------------------------------------------------------------------------------------
static	ssize_t set_fan_speeds	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	unsigned int	speed_0, speed_1, speed_2, speed_3;

	if(sscanf(buf, "%u %u %u %u\n", &speed_0, &speed_1, &speed_2, &speed_3) != 4)	return	-EINVAL;

	if(!(speed_0 < speed_1 && speed_1 < speed_2 && speed_2 < speed_3)){
		printk("fan speeds must be increasing in value\n");
		return count;
	}

	printk("fan_speeds : %s [%d %d %d %d] \n",__FUNCTION__, speed_0, speed_1, speed_2, speed_3);

	mutex_lock(&fan->mutex);
	fan->fan_speed_0 = speed_0;
	fan->fan_speed_1 = speed_1;
	fan->fan_speed_2 = speed_2;
	fan->fan_speed_3 = speed_3;
	mutex_unlock(&fan->mutex);

	return count;
}

static	ssize_t show_fan_speeds	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct odroid_fan *fan = dev_get_drvdata(dev);
	ssize_t result;

	mutex_lock(&fan->mutex);
	result = sprintf(buf, "%d %d %d %d\n", fan->fan_speed_0, fan->fan_speed_1, fan->fan_speed_2, fan->fan_speed_3);
	mutex_unlock(&fan->mutex);
	return	result;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static void odroid_fan_work(struct work_struct *work)
{
	struct odroid_fan *fan = container_of(work, struct odroid_fan, work.work);
	unsigned long temp=0;
	unsigned int duty_percent=0;

	if(!fan->fan_mode) return;

	temp = exynos_thermal_get_value();

	mutex_lock(&fan->mutex);
	if(temp<fan->tmu_level_0)		duty_percent=fan->fan_speed_0;
	else if(temp<fan->tmu_level_1)	duty_percent=fan->fan_speed_1;
	else if(temp<fan->tmu_level_2)	duty_percent=fan->fan_speed_2;
	else							duty_percent=fan->fan_speed_3;

	fan->duty = (255 * duty_percent)/100;

    if(fan->pwm_status){
		pwm_disable(fan->pwm);
		pwm_config(fan->pwm, fan->duty * fan->period / 255, fan->period);
    	pwm_enable(fan->pwm);
    }
    else {
		pwm_disable(fan->pwm);
		pwm_config(fan->pwm, 0, fan->period);
    }
	mutex_unlock(&fan->mutex);

	if(fan->fan_mode) {
		if(duty_percent>fan->fan_speed_0)
			schedule_delayed_work(&fan->work, msecs_to_jiffies(10000));
		else
			schedule_delayed_work(&fan->work, msecs_to_jiffies(2000));
	}
	return;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static int	odroid_fan_resume(struct platform_device *dev)
{
	#if	defined(DEBUG_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	odroid_fan_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct odroid_fan_platform_data *odroid_fan_parse_dt(struct platform_device *pdev, struct odroid_fan *fan)
{
	struct device 	*dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct odroid_fan_platform_data *pdata;
    unsigned int    rdata;

	if (!np)
		return NULL;

	pdata = kzalloc(sizeof(struct odroid_fan_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return NULL;
	}
	dev->platform_data = pdata;

	if(of_property_read_u32(np, "pwm_id", &rdata))	return NULL;
	fan->pwm_id = rdata;
	if(of_property_read_u32(np, "pwm_periode_ns", &rdata))	return NULL;
	fan->period = rdata;
	if(of_property_read_u32(np, "pwm_duty", &rdata))	return NULL;
	fan->duty = rdata;

	if(of_property_read_u32(np, "fan_mode", &rdata))	return NULL;
	fan->fan_mode = rdata;

	if(of_property_read_u32(np, "tmu_level_0", &rdata))	return NULL;
	fan->tmu_level_0 = rdata;
	if(of_property_read_u32(np, "tmu_level_1", &rdata))	return NULL;
	fan->tmu_level_1 = rdata;
	if(of_property_read_u32(np, "tmu_level_2", &rdata))	return NULL;
	fan->tmu_level_2 = rdata;

	if(of_property_read_u32(np, "fan_speed_0", &rdata))	return NULL;
	fan->fan_speed_0 = rdata;
	if(of_property_read_u32(np, "fan_speed_1", &rdata))	return NULL;
	fan->fan_speed_1 = rdata;
	if(of_property_read_u32(np, "fan_speed_2", &rdata))	return NULL;
	fan->fan_speed_2 = rdata;
	if(of_property_read_u32(np, "fan_speed_3", &rdata))	return NULL;
	fan->fan_speed_3 = rdata;

	return pdata;
}
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
static	int		odroid_fan_probe		(struct platform_device *pdev)	
{
	struct odroid_fan *fan;
	struct device 	*dev = &pdev->dev;
	int ret=0;

	fan = devm_kzalloc(&pdev->dev, sizeof(*fan), GFP_KERNEL);
	if (!fan) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (pdev->dev.of_node) {
		fan->pdata = odroid_fan_parse_dt(pdev, fan);
	}else {
		fan->pdata = pdev->dev.platform_data;
		//pwm port pin_func init
		if (gpio_is_valid(fan->pdata->pwm_gpio)) {
			ret = gpio_request(fan->pdata->pwm_gpio, "pwm_gpio");
			if (ret)
				printk(KERN_ERR "failed to get GPIO for PWM0\n");
			s3c_gpio_cfgpin(fan->pdata->pwm_gpio, fan->pdata->pwm_func);
			s5p_gpio_set_drvstr(fan->pdata->pwm_gpio, S5P_GPIO_DRVSTR_LV4);
			gpio_free(fan->pdata->pwm_gpio);
	    }
		fan->fan_mode = 1;
	
		fan->pwm_id = fan->pdata->pwm_id;
		fan->period = fan->pdata->pwm_periode_ns;
		fan->duty = fan->pdata->pwm_duty;

		fan->tmu_level_0 = TEMP_LEVEL_0;
		fan->tmu_level_1 = TEMP_LEVEL_1;
		fan->tmu_level_2 = TEMP_LEVEL_2;

		fan->fan_speed_0 = FAN_SPEED_0;
		fan->fan_speed_1 = FAN_SPEED_1;
		fan->fan_speed_2 = FAN_SPEED_2;
		fan->fan_speed_3 = FAN_SPEED_3;
	}

	fan->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(fan->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM, trying legacy API\n");

		fan->pwm = pwm_request(fan->pwm_id, "pwm-fan");
		if (IS_ERR(fan->pwm)) {
			dev_err(&pdev->dev, "unable to request legacy PWM\n");
			ret = PTR_ERR(fan->pwm);
			goto err_alloc;
		}
	}

	pwm_config(fan->pwm, fan->duty * fan->period / 255, fan->period);
	pwm_enable(fan->pwm);
	fan->pwm_status = 1;

	mutex_init(&fan->mutex);
	INIT_DELAYED_WORK(&fan->work, odroid_fan_work);
	schedule_delayed_work(&fan->work, msecs_to_jiffies(25000));

	dev_set_drvdata(dev, fan);

	ret =sysfs_create_group(&dev->kobj, &odroid_fan_attr_group);
	if(ret < 0)	{
		dev_err(&pdev->dev, "failed to create sysfs group !!\n");
	}

	return 0;
err_alloc:
	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	int		odroid_fan_remove		(struct platform_device *pdev)	
{
    return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(CONFIG_OF)
static const struct of_device_id odroid_fan_dt[] = {
	{ .compatible = "odroid-fan" },
	{ },
};
MODULE_DEVICE_TABLE(of, odroid_fan_dt);
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_driver odroid_fan_driver = {
	.driver = {
		.name = "odroid-fan",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(odroid_fan_dt),
#endif
	},
	.probe 		= odroid_fan_probe,
	.remove 	= odroid_fan_remove,
	.suspend	= odroid_fan_suspend,
	.resume		= odroid_fan_resume,
};

//[*]--------------------------------------------------------------------------------------------------[*]
static int __init odroid_fan_init(void)
{
    return platform_driver_register(&odroid_fan_driver);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit odroid_fan_exit(void)
{
    platform_driver_unregister(&odroid_fan_driver);
}

//[*]--------------------------------------------------------------------------------------------------[*]
module_init(odroid_fan_init);
module_exit(odroid_fan_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_DESCRIPTION("FAN driver for odroid-Dev board");
MODULE_AUTHOR("Hard-Kernel");
MODULE_LICENSE("GPL");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
