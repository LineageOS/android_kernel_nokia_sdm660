/*
 * Copyright (C) 2015 FIH Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef LINUX_BOOST_NT50356_H
#define LINUX_BOOST_NT50356_H

#define NT50356_DRV_VER 		"1.0.0"
#define NT50356_DEV_NAME 	"nt50356"

enum
{
	NT50356_REVISION = 0x01,
	NT50356_BACKLIGHT_CONF_1 = 0x02,
	NT50356_BACKLIGHT_CONF_2 = 0x03,
	NT50356_BACKLIGHT_BRIGHTNESS_LSB = 0x04,
	NT50356_BACKLIGHT_BRIGHTNESS_MSB = 0x05,
	NT50356_BACKLIGHT_ENABLE = 0x08,
	NT50356_BIAS_CONF_1 = 0x09,
	NT50356_BSTO_VOLTAGE_SETTING = 0x0C,
	NT50356_SET_AVDD_VOLTAGE = 0x0D,
	NT50356_SET_AVEE_VOLTAGE = 0x0E,
	NT50356_OPTION_1 = 0x10,
	NT50356_MAX_REG,
};

enum
{
	NT50356_4_0_V,
	NT50356_4_0_5_V,
	NT50356_4_1_V,
	NT50356_4_1_5_V,
	NT50356_4_2_V,
	NT50356_4_2_5_V,
	NT50356_4_3_V,
	NT50356_4_3_5_V,
	NT50356_4_4_V,
	NT50356_4_4_5_V,
	NT50356_4_5_V,
	NT50356_4_5_5_V,
	NT50356_4_6_V,
	NT50356_4_6_5_V,
	NT50356_4_7_V,
	NT50356_4_7_5_V,
	NT50356_4_8_V,
	NT50356_4_8_5_V,
	NT50356_4_9_V,
	NT50356_4_9_5_V,
	NT50356_5_0_V,
	NT50356_5_0_5_V,
	NT50356_5_1_V,
	NT50356_5_1_5_V,
	NT50356_5_2_V,
	NT50356_5_2_5_V,
	NT50356_5_3_V,
	NT50356_5_3_5_V,
	NT50356_5_4_V,
	NT50356_5_4_5_V,
	NT50356_5_5_V,
	NT50356_5_5_5_V,
	NT50356_5_6_V,
	NT50356_5_6_5_V,
	NT50356_5_7_V,
	NT50356_5_7_5_V,
	NT50356_5_8_V,
	NT50356_5_8_5_V,
	NT50356_5_9_V,
	NT50356_5_9_5_V,
	NT50356_6_0_V,
};

struct nt50356_platform_data
{
	struct mutex io_lock;
	u32 avdd_voltage;
	u32 avee_voltage;
	int    bl_en_gpio;
	int    enp_gpio;
	int    enn_gpio;
	int    id_gpio;
};

extern int nt50356_set_avdd_voltage(void);
extern int nt50356_set_avee_voltage(void);
extern int nt50356_resume(void);
extern int nt50356_suspend(void);
#endif
