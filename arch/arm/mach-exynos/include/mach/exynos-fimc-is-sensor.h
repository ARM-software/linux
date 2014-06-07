/*
 * /include/media/exynos_fimc_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_fimc_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_SENSOR_H
#define MEDIA_EXYNOS_SENSOR_H

#include <mach/exynos-fimc-is.h>

#define FIMC_IS_SENSOR_DEV_NAME "exynos-fimc-is-sensor"
#define FIMC_IS_PINNAME_LEN 32

enum exynos_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B = 1,
	CSI_ID_C = 2,
	CSI_ID_MAX
};

enum exynos_flite_id {
	FLITE_ID_A = 0,
	FLITE_ID_B = 1,
	FLITE_ID_C = 2,
	FLITE_ID_D = 3,
	FLITE_ID_END,
	FLITE_ID_NOTHING = 100,
};

enum exynos_sensor_channel {
	SENSOR_CONTROL_I2C0	 = 0,
	SENSOR_CONTROL_I2C1	 = 1,
	SENSOR_CONTROL_I2C2	 = 2
};

enum exynos_sensor_position {
	SENSOR_POSITION_REAR = 0,
	SENSOR_POSITION_FRONT
};

enum exynos_sensor_id {
	SENSOR_NAME_NOTHING		 = 0,
	SENSOR_NAME_S5K3H2		 = 1,
	SENSOR_NAME_S5K6A3		 = 2,
	SENSOR_NAME_S5K3H5		 = 3,
	SENSOR_NAME_S5K3H7		 = 4,
	SENSOR_NAME_S5K3H7_SUNNY	 = 5,
	SENSOR_NAME_S5K3H7_SUNNY_2M	 = 6,
	SENSOR_NAME_S5K6B2		 = 7,
	SENSOR_NAME_S5K3L2		 = 8,
	SENSOR_NAME_S5K4E5		 = 9,
	SENSOR_NAME_S5K2P2		 = 10,
	SENSOR_NAME_S5K8B1		 = 11,
	SENSOR_NAME_S5K1P2		 = 12,
	SENSOR_NAME_S5K4H5		 = 13,

	SENSOR_NAME_IMX135		 = 101, /* 101 ~ 200 Sony sensors */
	SENSOR_NAME_IMX175		 = 102,

	SENSOR_NAME_SR261		 = 201, /* 201 ~ 300 Other vendor sensors */

	SENSOR_NAME_END,
	SENSOR_NAME_CUSTOM		 = 301,
};

enum actuator_name {
	ACTUATOR_NAME_AD5823	= 1,
	ACTUATOR_NAME_DWXXXX	= 2,
	ACTUATOR_NAME_AK7343	= 3,
	ACTUATOR_NAME_HYBRIDVCA	= 4,
	ACTUATOR_NAME_LC898212	= 5,
	ACTUATOR_NAME_WV560	= 6,
	ACTUATOR_NAME_AK7345	= 7,
	ACTUATOR_NAME_DW9804    = 8,
	ACTUATOR_NAME_AK7348    = 9,
	ACTUATOR_NAME_END,
	ACTUATOR_NAME_NOTHING	= 100,
};

enum flash_drv_name {
	FLADRV_NAME_KTD267	= 1,	/* Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_AAT1290A	= 2,
	FLADRV_NAME_MAX77693	= 3,
	FLADRV_NAME_AS3643	= 4,
	FLADRV_NAME_KTD2692	= 5,
	FLADRV_NAME_LM3560	= 6,
	FLADRV_NAME_SKY81296	= 7,
	FLADRV_NAME_RT5033	= 8,
	FLADRV_NAME_END,
	FLADRV_NAME_NOTHING	= 100,
};

enum from_name {
	FROMDRV_NAME_W25Q80BW	= 1,	/* Spi type */
	FROMDRV_NAME_FM25M16A	= 2,	/* Spi type */
	FROMDRV_NAME_FM25M32A	= 3,
	FROMDRV_NAME_NOTHING	= 100,
};

enum companion_name {
	COMPANION_NAME_73C1	= 1,	/* SPI, I2C, FIMC Lite */
	COMPANION_NAME_END,
	COMPANION_NAME_NOTHING	= 100,
};

enum sensor_peri_type {
	SE_NULL		= 0,
	SE_I2C		= 1,
	SE_SPI		= 2,
	SE_GPIO		= 3,
	SE_MPWM		= 4,
	SE_ADC		= 5,
	SE_FIMC_LITE	= 6,
};

struct i2c_type {
	u32 channel;
	u32 slave_address;
	u32 speed;
};

struct spi_type {
	u32 channel;
};

struct gpio_type {
	u32 first_gpio_port_no;
	u32 second_gpio_port_no;
};

struct fimc_lite_type {
	u32 channel;
};

union sensor_peri_format {
	struct i2c_type i2c;
	struct spi_type spi;
	struct gpio_type gpio;
	struct fimc_lite_type fimc_lite;
};

struct sensor_protocol {
	u32 product_name;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
	u32 reserved[4];
};

struct sensor_peri_info {
	bool valid;
	enum sensor_peri_type peri_type;
	union sensor_peri_format peri_setting;
};

struct sensor_companion {
	u32 product_name;	/* enum companion_name */
	struct sensor_peri_info peri_info0;
	struct sensor_peri_info peri_info1;
	struct sensor_peri_info peri_info2;
	struct sensor_peri_info reserved[2];
};

struct sensor_open_extended {
	struct sensor_protocol sensor_con;
	struct sensor_protocol actuator_con;
	struct sensor_protocol flash_con;
	struct sensor_protocol from_con;
	struct sensor_companion companion_con;

	u32 mclk;
	u32 mipi_lane_num;
	u32 mipi_settle_line;
	u32 mipi_speed;
	/* Skip setfile loading when fast_open_sensor is not 0 */
	u32 fast_open_sensor;
	/* Activatiing sensor self calibration mode (6A3) */
	u32 self_calibration_mode;
	/* This field is to adjust I2c clock based on ACLK200 */
	/* This value is varied in case of rev 0.2 */
	u32 I2CSclk;
};

#define GPIO_SCENARIO_ON		0
#define GPIO_SCENARIO_OFF		1
#define GPIO_SCENARIO_MAX		2
#define GPIO_CTRL_MAX			32

#define SENSOR_SCENARIO_NORMAL		0
#define SENSOR_SCENARIO_VISION		1
#define SENSOR_SCENARIO_MAX 		10

enum pin_act {
	PIN_PULL_NONE = 0,
	PIN_OUTPUT_HIGH,
	PIN_OUTPUT_LOW,
	PIN_INPUT,
	PIN_RESET,
	PIN_FUNCTION,
	PIN_REGULATOR_ON,
	PIN_REGULATOR_OFF,
	PIN_END
};

struct exynos_sensor_pin {
	int pin;
	u32 value;
	char *name;
	enum pin_act act;
};

#define SET_PIN(d, s, c, i, p, v, n, a)		\
	(d)->pin_ctrls[s][c][i].pin	= p;	\
	(d)->pin_ctrls[s][c][i].value	= v;	\
	(d)->pin_ctrls[s][c][i].name	= n;	\
	(d)->pin_ctrls[s][c][i].act	= a;

/*
 * struct exynos_platform_fimc_is_sensor - platform data for exynos_sensor driver
 * @irq: GPIO getting the irq pin of exynos_sensor
 * @gpio_rst: GPIO driving the reset pin of exynos_sensor
 * @enable_rst: the pin state when reset pin is enabled
 * @clk_on/off: sensor clock on/off
 * @set_power: an additional callback to a board setup code
 *			to be called after enabling and before disabling
 *			the exynos_sensor device supply regulators
 */
struct exynos_platform_fimc_is_sensor {
	int (*gpio_cfg)(struct platform_device *pdev, u32 scenario, u32 enable);
	int (*iclk_cfg)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*iclk_on)(struct platform_device *pdev,u32 scenario, u32 channel);
	int (*iclk_off)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*mclk_on)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*mclk_off)(struct platform_device *pdev, u32 scenario, u32 channel);
	struct exynos_sensor_pin pin_ctrls[SENSOR_SCENARIO_MAX][GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	u32 scenario;
	u32 mclk_ch;
	u32 csi_ch;
	u32 flite_ch;
	u32 i2c_ch;
	u32 i2c_addr;
	u32 is_bns;
	u32 flash_first_gpio;
	u32 flash_second_gpio;
};

extern int exynos_fimc_is_sensor_pins_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 enable);
extern int exynos_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel);

#endif /* MEDIA_EXYNOS_SENSOR_H */
