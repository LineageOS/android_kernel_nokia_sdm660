/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Version: v1.0.0
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-aw2015.h>

/* register address */
#define AW2015_REG_RESET				0x00
#define AW2015_REG_GCR					0x01
#define AW2015_REG_STATUS				0x02
#define AW2015_REG_IMAX					0x03
#define AW2015_REG_LCFG1				0x04
#define AW2015_REG_LCFG2				0x05
#define AW2015_REG_LCFG3				0x06
#define AW2015_REG_LEDEN				0x07
#define AW2015_REG_PAT_RUN				0x09
#define AW2015_REG_ILED1				0x10
#define AW2015_REG_ILED2				0x11
#define AW2015_REG_ILED3				0x12
#define AW2015_REG_PWM1					0x1C
#define AW2015_REG_PWM2					0x1D
#define AW2015_REG_PWM3					0x1E
#define AW2015_REG_PAT1_T1				0x30
#define AW2015_REG_PAT1_T2				0x31
#define AW2015_REG_PAT1_T3				0x32
#define AW2015_REG_PAT1_T4				0x33
#define AW2015_REG_PAT1_T5				0x34
#define AW2015_REG_PAT2_T1				0x35
#define AW2015_REG_PAT2_T2				0x36
#define AW2015_REG_PAT2_T3				0x37
#define AW2015_REG_PAT2_T4				0x38
#define AW2015_REG_PAT2_T5				0x39
#define AW2015_REG_PAT3_T1				0x3A
#define AW2015_REG_PAT3_T2				0x3B
#define AW2015_REG_PAT3_T3				0x3C
#define AW2015_REG_PAT3_T4				0x3D
#define AW2015_REG_PAT3_T5				0x3E

/* register bits */
#define AW2015_CHIPID					0x31
#define AW2015_LED_RESET_MASK			0x55
#define AW2015_LED_CHIP_DISABLE			0x00
#define AW2015_LED_CHIP_ENABLE_MASK		0x01
#define AW2015_LED_CHARGE_DISABLE_MASK	0x02
#define AW2015_LED_BREATHE_MODE_MASK	0x01
#define AW2015_LED_ON_MODE_MASK			0x00
#define AW2015_LED_BREATHE_PWM_MASK		0xFF
#define AW2015_LED_ON_PWM_MASK			0xFF
#define AW2015_LED_FADEIN_MODE_MASK		0x02
#define AW2015_LED_FADEOUT_MODE_MASK	0x04

#define MAX_RISE_TIME_MS				15
#define MAX_HOLD_TIME_MS				15
#define MAX_FALL_TIME_MS				15
#define MAX_OFF_TIME_MS					15

/* aw2015 register read/write access*/
#define REG_NONE_ACCESS					0
#define REG_RD_ACCESS					1 << 0
#define REG_WR_ACCESS					1 << 1
#define AW2015_REG_MAX					0x7F

enum aw2015_led_mode {
	FLASH_NONE = 0,
	FLASH_URGENT = 3,
	FLASH_ALERT,
	FLASH_SINGLE,
	FLASH_USUAL,
	FLASH_ALWAYSON,
};

static int aw2015_led_mode_val = -1;

const unsigned char aw2015_reg_access[AW2015_REG_MAX] = {
	[AW2015_REG_RESET]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_GCR]     = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_STATUS]  = REG_RD_ACCESS,
	[AW2015_REG_IMAX]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG1]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG2]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LCFG3]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_LEDEN]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT_RUN] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED1]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED2]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_ILED3]   = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM1]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM2]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PWM3]    = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT1_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT2_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T1] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T2] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T3] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T4] = REG_RD_ACCESS|REG_WR_ACCESS,
	[AW2015_REG_PAT3_T5] = REG_RD_ACCESS|REG_WR_ACCESS,
};

/* aw2015 led struct */
struct aw2015_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct aw2015_platform_data *pdata;
	struct work_struct brightness_work;
	struct mutex lock;
	int num_leds;
	int id;
};

struct aw2015_pattern {
	u8 reg;
	u8 val;
};

static struct aw2015_pattern urgent[] = {
	{0x00,0x55}, // write 0x55 to RSTR reset internal logic and register 

	{0x01,0x03}, //Disable auto charge indication function and eable the device 
	{0x03,0x01}, //IMAX 6.375mA

	{0x04,0x01},
	{0x05,0x01},//Pattern mode
	{0x06,0x01},

	{0x08,0x08},//RGB

	{0x10,0xFF},//The LED RED output current value is IMAX * ILED1_1 / 255
	{0x13,0x00},//The LED1 output current value is IMAX * ILED1_2 / 255
	{0x16,0x00},//The LED1 output current value is IMAX * ILED1_3 / 255
	{0x19,0x00},//The LED1 output current value is IMAX * ILED1_4 / 255

	{0x11,0xFF},//The LED GREEN output current value is IMAX * ILED2_1 / 255
	{0x14,0x00},//The LED2 output current value is IMAX * ILED2_2 / 255
	{0x17,0x00},//The LED2 output current value is IMAX * ILED2_3 / 255
	{0x1a,0x00},//The LED2 output current value is IMAX * ILED2_4 / 255

	{0x12,0x00},//The LED3 output current value is IMAX * ILED3_1 / 255
	{0x15,0x00},//The LED3 output current value is IMAX * ILED3_2 / 255
	{0x18,0x00},//The LED3 output current value is IMAX * ILED3_3 / 255
	{0x1b,0x00},//The LED3 output current value is IMAX * ILED3_4 / 255

	{0x1c,0xff},//PWM
	{0x1d,0xff},
	{0x1e,0xff},

	{0x30,0x02},//SET TRISE AND TON
	{0x35,0x01},
	{0x3a,0x00},

	{0x31,0x02},//SET TFALL AND TOFF
	{0x36,0x30},
	{0x3b,0x00},

	{0x32,0x00},//SET TSLOT AND TDELAY
	{0x37,0x00},
	{0x3c,0x00},

	{0x33,0xc1},// pattern switch ; color 1 
	{0x38,0xc1},//change color if need
	{0x3d,0xc2},

	{0x07,0x07},//LED all Enable 
	{0x09,0x07}//pattern run if independent mode
};

static struct aw2015_pattern alert[] = {
	{0x00,0x55}, // write 0x55 to RSTR reset internal logic and register 
	
	{0x01,0x03}, //Disable auto charge indication function and eable the device 
	{0x03,0x01}, //IMAX 6.375mA
	
	{0x04,0x01},
	{0x05,0x01},//Pattern mode
	{0x06,0x01},
	
	{0x08,0x08},//RGB
	
	{0x10,0xFF},//The LED RED output current value is IMAX * ILED1_1 / 255
	{0x13,0x00},//The LED1 output current value is IMAX * ILED1_2 / 255
	{0x16,0x00},//The LED1 output current value is IMAX * ILED1_3 / 255
	{0x19,0x80},//The LED1 output current value is IMAX * ILED1_4 / 255
	
	{0x11,0xFF},//The LED GREEN output current value is IMAX * ILED2_1 / 255
	{0x14,0x00},//The LED2 output current value is IMAX * ILED2_2 / 255
	{0x17,0x00},//The LED2 output current value is IMAX * ILED2_3 / 255
	{0x1a,0x80},//The LED2 output current value is IMAX * ILED2_4 / 255
	
	{0x12,0x00},//The LED3 output current value is IMAX * ILED3_1 / 255
	{0x15,0x00},//The LED3 output current value is IMAX * ILED3_2 / 255
	{0x18,0xff},//The LED3 output current value is IMAX * ILED3_3 / 255
	{0x1b,0x80},//The LED3 output current value is IMAX * ILED3_4 / 255
	
	{0x1c,0xff},//PWM
	{0x1d,0xff},
	{0x1e,0xff},
	
	{0x30,0x34},//SET TRISE AND TON
	{0x35,0x00},
	{0x3a,0x00},
	
	{0x31,0x35},//SET TFALL AND TOFF
	{0x36,0x00},
	{0x3b,0x00},
	
	{0x32,0x00},//SET TSLOT AND TDELAY
	{0x37,0x00},
	{0x3c,0x00},
	
	{0x33,0x01},// pattern switch ; color 1 
	{0x38,0x00},//change color if need
	{0x3d,0x00},
	
	{0x07,0x07},//LED all Enable 
	{0x09,0x07}//pattern run if independent mode

};

static struct aw2015_pattern single_time[] = {
	{0x00,0x55}, // write 0x55 to RSTR reset internal logic and register

	{0x01,0x03}, //Disable auto charge indication function and eable the device 
	{0x03,0x01}, //IMAX 6.375mA
	
	{0x04,0x01},
	{0x05,0x01},//Pattern mode
	{0x06,0x01},
	
	{0x08,0x08},//RGB
	
	{0x10,0xFF},//The LED RED output current value is IMAX * ILED1_1 / 255
	{0x13,0x00},//The LED1 output current value is IMAX * ILED1_2 / 255
	{0x16,0x00},//The LED1 output current value is IMAX * ILED1_3 / 255
	{0x19,0x80},//The LED1 output current value is IMAX * ILED1_4 / 255
	
	{0x11,0xFF},//The LED GREEN output current value is IMAX * ILED2_1 / 255
	{0x14,0x00},//The LED2 output current value is IMAX * ILED2_2 / 255
	{0x17,0x00},//The LED2 output current value is IMAX * ILED2_3 / 255
	{0x1a,0x80},//The LED2 output current value is IMAX * ILED2_4 / 255
	
	{0x12,0x00},//The LED3 output current value is IMAX * ILED3_1 / 255
	{0x15,0x00},//The LED3 output current value is IMAX * ILED3_2 / 255
	{0x18,0xff},//The LED3 output current value is IMAX * ILED3_3 / 255
	{0x1b,0x80},//The LED3 output current value is IMAX * ILED3_4 / 255
	
	{0x1c,0xff},//PWM
	{0x1d,0xff},
	{0x1e,0xff},
	
	{0x30,0x02},//SET TRISE AND TON
	{0x35,0x00},
	{0x3a,0x00},
	
	{0x31,0x33},//SET TFALL AND TOFF
	{0x36,0x00},
	{0x3b,0x00},
	
	{0x32,0x00},//SET TSLOT AND TDELAY
	{0x37,0x00},
	{0x3c,0x00},
	
	{0x33,0x81},// pattern switch ; color 1 
	{0x38,0x00},//change color if need
	{0x3d,0x00},
	
	{0x07,0x07},//LED all Enable 
	{0x09,0x07}//pattern run if independent mode
	
};

static struct aw2015_pattern usual[] = {
	{0x00,0x55}, // write 0x55 to RSTR reset internal logic and register 
	
	{0x01,0x03}, //Disable auto charge indication function and eable the device 
	{0x03,0x01}, //IMAX 6.375mA
	
	{0x04,0x01},
	{0x05,0x01},//Pattern mode
	{0x06,0x01},
	
	{0x08,0x08},//RGB
	
	{0x10,0xFF},//The LED RED output current value is IMAX * ILED1_1 / 255
	{0x13,0x00},//The LED1 output current value is IMAX * ILED1_2 / 255
	{0x16,0x00},//The LED1 output current value is IMAX * ILED1_3 / 255
	{0x19,0x00},//The LED1 output current value is IMAX * ILED1_4 / 255
	
	{0x11,0xFF},//The LED GREEN output current value is IMAX * ILED2_1 / 255
	{0x14,0x00},//The LED2 output current value is IMAX * ILED2_2 / 255
	{0x17,0x00},//The LED2 output current value is IMAX * ILED2_3 / 255
	{0x1a,0x00},//The LED2 output current value is IMAX * ILED2_4 / 255
	
	{0x12,0x00},//The LED3 output current value is IMAX * ILED3_1 / 255
	{0x15,0x00},//The LED3 output current value is IMAX * ILED3_2 / 255
	{0x18,0x00},//The LED3 output current value is IMAX * ILED3_3 / 255
	{0x1b,0x00},//The LED3 output current value is IMAX * ILED3_4 / 255
	
	{0x1c,0xff},//PWM
	{0x1d,0xff},
	{0x1e,0xff},
	
	{0x30,0x46},//SET TRISE AND TON
	{0x35,0x00},
	{0x3a,0x00},
	
	{0x31,0x48},//SET TFALL AND TOFF
	{0x36,0x00},
	{0x3b,0x00},
	
	{0x32,0x00},//SET TSLOT AND TDELAY
	{0x37,0x00},
	{0x3c,0x00},
	
	{0x33,0x01},// pattern switch ; color 1 
	{0x38,0x00},//change color if need
	{0x3d,0x00},
	
	{0x07,0x07},//LED all Enable 
	{0x09,0x07}//pattern run if independent mode
	
};

static struct aw2015_pattern always_on[] = {
	{0x00,0x55}, // write 0x55 to RSTR reset internal logic and register 
	
	{0x01,0x03}, //Disable auto charge indication function and eable the device 
	{0x03,0x01}, //IMAX 6.375mA
	
	{0x04,0x00},
	
	{0x08,0x08},//RGB
	
	{0x10,0xFF},//The LED RED output current value is IMAX * ILED1_1 / 255
	{0x13,0x00},//The LED1 output current value is IMAX * ILED1_2 / 255
	{0x16,0x00},//The LED1 output current value is IMAX * ILED1_3 / 255
	{0x19,0x80},//The LED1 output current value is IMAX * ILED1_4 / 255
	
	{0x11,0xFF},//The LED GREEN output current value is IMAX * ILED2_1 / 255
	{0x14,0x00},//The LED2 output current value is IMAX * ILED2_2 / 255
	{0x17,0x00},//The LED2 output current value is IMAX * ILED2_3 / 255
	{0x1a,0x80},//The LED2 output current value is IMAX * ILED2_4 / 255
	
	{0x12,0x00},//The LED3 output current value is IMAX * ILED3_1 / 255
	{0x15,0x00},//The LED3 output current value is IMAX * ILED3_2 / 255
	{0x18,0xff},//The LED3 output current value is IMAX * ILED3_3 / 255
	{0x1b,0x80},//The LED3 output current value is IMAX * ILED3_4 / 255
	
	{0x1c,0x80},//PWM
	{0x1d,0x80},
	{0x1e,0x80},
	
	{0x07,0x07},//LED all Enable 
	{0x09,0x07}//pattern run if independent mode

};

static int aw2015_write(struct aw2015_led *led, u8 reg, u8 val)
{
	int ret = -EINVAL, retry_times = 0;

	do {
		ret = i2c_smbus_write_byte_data(led->client, reg, val);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);
	
	return ret;	
}

static int aw2015_read(struct aw2015_led *led, u8 reg, u8 *val)
{
	int ret = -EINVAL, retry_times = 0;

	do{
		ret = i2c_smbus_read_byte_data(led->client, reg);
		retry_times ++;
		if(retry_times == 5)
			break;
	}while (ret < 0);

	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}


static void aw2015_brightness_work(struct work_struct *work)
{
	struct aw2015_led *led = container_of(work, struct aw2015_led,
					brightness_work);
	u8 val = 0;

	mutex_lock(&led->pdata->led->lock);

	/* enable aw2015 if disabled */
	aw2015_read(led, AW2015_REG_GCR, &val);
	if (!(val&AW2015_LED_CHIP_ENABLE_MASK)) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_ENABLE_MASK | AW2015_LED_CHARGE_DISABLE_MASK);
	}
	
    if (led->cdev.brightness > 0) {
		if (led->cdev.brightness > led->cdev.max_brightness)
			led->cdev.brightness = led->cdev.max_brightness;
		aw2015_write(led, AW2015_REG_LCFG1 + led->id, AW2015_LED_ON_MODE_MASK);
		aw2015_write(led, AW2015_REG_IMAX , led->pdata->imax);
		aw2015_write(led, AW2015_REG_ILED1 + led->id, led->cdev.brightness);
		aw2015_write(led, AW2015_REG_PWM1 + led->id, AW2015_LED_ON_PWM_MASK);
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val | (1 << led->id));
	} else {
        aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
		aw2015_led_mode_val = FLASH_NONE;
	}

	/*
	 * If value in AW2015_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2015_read(led, AW2015_REG_LEDEN, &val);
	if (val == 0) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_DISABLE  | AW2015_LED_CHARGE_DISABLE_MASK);
		mutex_unlock(&led->pdata->led->lock);
		return;
	}

	mutex_unlock(&led->pdata->led->lock);
}

static void aw2015_led_blink_set(struct aw2015_led *led, unsigned long blinking)
{
	u8 val = 0;

	/* enable regulators if they are disabled */
	/* enable aw2015 if disabled */
	aw2015_read(led, AW2015_REG_GCR, &val);
	if (!(val&AW2015_LED_CHIP_ENABLE_MASK)) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_ENABLE_MASK | AW2015_LED_CHARGE_DISABLE_MASK);
	}

	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;

	if (blinking > 0) {
		aw2015_write(led, AW2015_REG_LCFG1 + led->id, AW2015_LED_BREATHE_MODE_MASK);
		aw2015_write(led, AW2015_REG_IMAX , led->pdata->imax);
		aw2015_write(led, AW2015_REG_ILED1 + led->id, led->pdata->led_current);
		aw2015_write(led, AW2015_REG_PWM1 + led->id, AW2015_LED_BREATHE_PWM_MASK);
		aw2015_write(led, AW2015_REG_PAT1_T1 + led->id*5, 
					(led->pdata->rise_time_ms << 4 | led->pdata->hold_time_ms));
		aw2015_write(led, AW2015_REG_PAT1_T2 + led->id*5, 
					(led->pdata->fall_time_ms << 4 | led->pdata->off_time_ms));

		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val | (1 << led->id));

		aw2015_write(led, AW2015_REG_PAT_RUN, (1 << led->id));
	} else {
		aw2015_read(led, AW2015_REG_LEDEN, &val);
		aw2015_write(led, AW2015_REG_LEDEN, val & (~(1 << led->id)));
		aw2015_led_mode_val = FLASH_NONE;
	}

	/*
	 * If value in AW2015_REG_LEDEN is 0, it means the RGB leds are
	 * all off. So we need to power it off.
	 */
	aw2015_read(led, AW2015_REG_LEDEN, &val);
	if (val == 0) {
		aw2015_write(led, AW2015_REG_GCR, AW2015_LED_CHIP_DISABLE  | AW2015_LED_CHARGE_DISABLE_MASK);
	}
}

static void aw2015_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct aw2015_led *led = container_of(cdev, struct aw2015_led, cdev);

	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}

static ssize_t aw2015_store_blink(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long blinking = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2015_led_blink_set(led, blinking);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2015_led_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			led->pdata->rise_time_ms, led->pdata->hold_time_ms,
			led->pdata->fall_time_ms, led->pdata->off_time_ms);
}

static ssize_t aw2015_led_time_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int rc = 0, rise_time_ms = 0, hold_time_ms = 0, fall_time_ms = 0, off_time_ms = 0;

	rc = sscanf(buf, "%d %d %d %d",
			&rise_time_ms, &hold_time_ms,
			&fall_time_ms, &off_time_ms);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->rise_time_ms = (rise_time_ms > MAX_RISE_TIME_MS) ?
				MAX_RISE_TIME_MS : rise_time_ms;
	led->pdata->hold_time_ms = (hold_time_ms > MAX_HOLD_TIME_MS) ?
				MAX_HOLD_TIME_MS : hold_time_ms;
	led->pdata->fall_time_ms = (fall_time_ms > MAX_FALL_TIME_MS) ?
				MAX_FALL_TIME_MS : fall_time_ms;
	led->pdata->off_time_ms = (off_time_ms > MAX_OFF_TIME_MS) ?
				MAX_OFF_TIME_MS : off_time_ms;
	aw2015_led_blink_set(led, 1);
	mutex_unlock(&led->pdata->led->lock);
	return len;
}

static ssize_t aw2015_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	unsigned char i = 0, reg_val = 0;
	ssize_t len = 0;

	for(i=0; i<AW2015_REG_MAX; i++) {
		if(!(aw2015_reg_access[i]&REG_RD_ACCESS))
		continue;
		aw2015_read(led, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}
	
	return len;
}

static ssize_t aw2015_reg_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);

	unsigned int databuf[2] = {0};

	if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1]))
	{
		aw2015_write(led, (unsigned char)databuf[0], (unsigned char)databuf[1]);
	}

	return len;
}

static int parse_string(const char *in_buf, char *out_buf)
{
	int i;

	if (snprintf(out_buf, 20, "%s", in_buf)
		> 20)
		return -EINVAL;

	for (i = 0; i < strlen(out_buf); i++) {
		if (out_buf[i] == ' ' || out_buf[i] == '\n' ||
			out_buf[i] == '\t') {
			out_buf[i] = '\0';
			break;
		}
	}

	return 0;
}


static ssize_t aw2015_pattern_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
	int size = -1;
	int i;
    int rc = 0;
    char str[20+1];

	dev_info(&led->client->dev,"%s,the pattern is %s",__func__,buf);

	rc = parse_string(buf, str);
	if (rc < 0){
        dev_info(&led->client->dev,"%s,the pattern is too long",__func__);
        return rc;
    }

    if (0 == strcmp(str,"urgent")) {
		aw2015_led_mode_val = FLASH_URGENT;
		size = sizeof(urgent)/sizeof(struct aw2015_pattern);
		for (i = 0; i < size; i++) {
			aw2015_write(led, urgent[i].reg, urgent[i].val);
		}
	}else if (0 == strcmp(str,"alert")) {
		aw2015_led_mode_val = FLASH_ALERT;
		size = sizeof(alert)/sizeof(struct aw2015_pattern);
		for (i = 0; i < size; i++) {
			aw2015_write(led, alert[i].reg, alert[i].val);
		}
	}else if (0 == strcmp(str,"singletime")) {
		aw2015_led_mode_val = FLASH_SINGLE;
		size = sizeof(single_time)/sizeof(struct aw2015_pattern);
		for (i = 0; i < size; i++) {
			aw2015_write(led, single_time[i].reg, single_time[i].val);
		}
	}else if (0 == strcmp(str,"usual")) {
		aw2015_led_mode_val = FLASH_USUAL;
		size = sizeof(usual)/sizeof(struct aw2015_pattern);
		for (i = 0; i < size; i++) {
			aw2015_write(led, usual[i].reg, usual[i].val);
		}
	}else if (0 == strcmp(str,"alwayson")) {
		aw2015_led_mode_val = FLASH_ALWAYSON;
		size = sizeof(always_on)/sizeof(struct aw2015_pattern);
		for (i = 0; i < size; i++) {
			aw2015_write(led, always_on[i].reg, always_on[i].val);
		}
	}else {
		dev_err(&led->client->dev,"aw2015 does not support this pattern");
	}
	return len;
}

static ssize_t aw2015_enviroment_brightest_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2015_led *led =
			container_of(led_cdev, struct aw2015_led, cdev);
    int rc = 0;
    char str[20+1];
	int env_val = 2;

	rc = parse_string(buf, str);
	if (rc < 0){
        dev_info(&led->client->dev,"%s,the enviroment is too long",__func__);
        return rc;
    }

	dev_info(&led->client->dev,"%s,the enviroment %s",__func__,str);

	if (0 == strcmp(str,"dark")) {
		env_val = 1;
	}else if (0 == strcmp(str,"outdoor")) {
		env_val = 3;
	}
	else
	{
		env_val = 2;
	}
	dev_info(&led->client->dev,"%s,the enviroment val %d,mode=%d",__func__,env_val,aw2015_led_mode_val);

	switch (aw2015_led_mode_val) {
	case FLASH_URGENT:
	case FLASH_ALERT:
	case FLASH_USUAL:
	case FLASH_SINGLE:
		if (1 == env_val) {
			aw2015_write(led, AW2015_REG_PWM1, 0x80);
		}else if (3 == env_val) {
			aw2015_write(led, AW2015_REG_PWM1, 0xff);
		}
		else
		{
			aw2015_write(led, AW2015_REG_PWM1, 0xcc);
		}
		break;
	case FLASH_ALWAYSON:
		if (1 == env_val) {
			aw2015_write(led, AW2015_REG_PWM1, 0x66);
			aw2015_write(led, AW2015_REG_PWM1, 0x4d);
		}else if (3 == env_val) {
			aw2015_write(led, AW2015_REG_PWM1, 0xcc);
		}
		else
		{
			aw2015_write(led, AW2015_REG_PWM1, 0x80);
		}
		break;
	default:
        dev_info(&led->client->dev,"%s,do not support this led mode",__func__);
		break;
	}
	return len;
}

static DEVICE_ATTR(blink, 0664, NULL, aw2015_store_blink);
static DEVICE_ATTR(led_time, 0664, aw2015_led_time_show, aw2015_led_time_store);
static DEVICE_ATTR(reg, 0664, aw2015_reg_show, aw2015_reg_store);
static DEVICE_ATTR(pattern, 0664, NULL, aw2015_pattern_store);
static DEVICE_ATTR(enviroment, 0664, NULL, aw2015_enviroment_brightest_store);


static struct attribute *aw2015_led_attributes[] = {
	&dev_attr_blink.attr,
	&dev_attr_led_time.attr,
	&dev_attr_reg.attr,
	&dev_attr_pattern.attr,
	&dev_attr_enviroment.attr,
	NULL,
};

static struct attribute_group aw2015_led_attr_group = {
	.attrs = aw2015_led_attributes
};
static int aw2015_check_chipid(struct aw2015_led *led)
{
	u8 val = 0;
	u8 cnt = 0;
	
	for(cnt = 5; cnt > 0; cnt --)
	{
		aw2015_read(led, AW2015_REG_RESET, &val);
		dev_info(&led->client->dev,"AW2015 chip id %0x",val);
		if (val == AW2015_CHIPID)
			return 0;
	}
	
	return -EINVAL;
}

static int aw2015_led_err_handle(struct aw2015_led *led_array,
				int parsed_leds)
{
	int i = 0;
	/*
	 * If probe fails, cannot free resource of all LEDs, only free
	 * resources of LEDs which have allocated these resource really.
	 */
	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2015_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&led_array->client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	return i;
}

static int aw2015_led_parse_child_node(struct aw2015_led *led_array,
				struct device_node *node)
{
	struct aw2015_led *led;
	struct device_node *temp;
	struct aw2015_platform_data *pdata;
	int rc = 0, parsed_leds = 0;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->client = led_array->client;

		pdata = devm_kzalloc(&led->client->dev,
				sizeof(struct aw2015_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&led->client->dev,
				"Failed to allocate memory\n");
			goto free_err;
		}
		pdata->led = led_array;
		led->pdata = pdata;

		rc = of_property_read_string(temp, "aw2015,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,id",
			&led->id);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,imax",
			&led->pdata->imax);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading imax, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,led-current",
			&led->pdata->led_current);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led-current, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,max-brightness",
			&led->cdev.max_brightness);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-brightness, rc = %d\n",
				rc);
			goto free_pdata;
		}
		
		rc = of_property_read_u32(temp, "aw2015,rise-time-ms",
			&led->pdata->rise_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading rise-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,hold-time-ms",
			&led->pdata->hold_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading hold-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,fall-time-ms",
			&led->pdata->fall_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading fall-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2015,off-time-ms",
			&led->pdata->off_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading off-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		INIT_WORK(&led->brightness_work, aw2015_brightness_work);

		led->cdev.brightness_set = aw2015_set_brightness;

		rc = led_classdev_register(&led->client->dev, &led->cdev);
		if (rc) {
			dev_err(&led->client->dev,
				"unable to register led %d,rc=%d\n",
				led->id, rc);
			goto free_pdata;
		}

		rc = sysfs_create_group(&led->cdev.dev->kobj,
				&aw2015_led_attr_group);
		if (rc) {
			dev_err(&led->client->dev, "led sysfs rc: %d\n", rc);
			goto free_class;
		}
		parsed_leds++;
	}

	return 0;

free_class:
	aw2015_led_err_handle(led_array, parsed_leds);
	led_classdev_unregister(&led_array[parsed_leds].cdev);
	cancel_work_sync(&led_array[parsed_leds].brightness_work);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	led_array[parsed_leds].pdata = NULL;
	return rc;

free_pdata:
	aw2015_led_err_handle(led_array, parsed_leds);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	return rc;

free_err:
	aw2015_led_err_handle(led_array, parsed_leds);
	return rc;
}

extern char *saved_command_line;
static void aw2015_led_alwayson(struct aw2015_led *led)
{
    int size = -1;
    int i = -1;
    size = sizeof(always_on)/sizeof(struct aw2015_pattern);
    for (i = 0; i < size; i++) {
        aw2015_write(led, always_on[i].reg, always_on[i].val);
    }
}
static int aw2015_led_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct aw2015_led *led_array;
	struct device_node *node;
	int ret = -EINVAL, num_leds = 0;
	node = client->dev.of_node;
	if (node == NULL)
		return -EINVAL;

	num_leds = of_get_child_count(node);

	if (!num_leds)
		return -EINVAL;

	led_array = devm_kzalloc(&client->dev,
			(sizeof(struct aw2015_led) * num_leds), GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	led_array->client = client;
	led_array->num_leds = num_leds;

	mutex_init(&led_array->lock);

	ret = aw2015_led_parse_child_node(led_array, node);
	if (ret) {
		dev_err(&client->dev, "parsed node error\n");
		goto free_led_arry;
	}

	i2c_set_clientdata(client, led_array);
	
	ret = aw2015_check_chipid(led_array);
	if (ret) {
		dev_err(&client->dev, "Check chip id error\n");
		goto fail_parsed_node;
	}

    if (strstr(saved_command_line,"androidboot.mode=charger") != NULL) {
        aw2015_led_alwayson(led_array); //poweroff charging
    }



	return 0;

fail_parsed_node:
	aw2015_led_err_handle(led_array, num_leds);
free_led_arry:
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return ret;
}

static int aw2015_led_remove(struct i2c_client *client)
{
	struct aw2015_led *led_array = i2c_get_clientdata(client);
	int i = 0, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2015_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return 0;
}

static void aw2015_led_shutdown(struct i2c_client *client)
{
	struct aw2015_led *led = i2c_get_clientdata(client);
	
    aw2015_write(led, AW2015_REG_RESET, AW2015_LED_RESET_MASK);
    
}

static const struct i2c_device_id aw2015_led_id[] = {
	{"aw2015_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2015_led_id);

static struct of_device_id aw2015_match_table[] = {
	{ .compatible = "awinic,aw2015_led",},
	{ },
};

static struct i2c_driver aw2015_led_driver = {
	.probe = aw2015_led_probe,
	.remove = aw2015_led_remove,
	.shutdown = aw2015_led_shutdown,
	.driver = {
		.name = "aw2015_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2015_match_table),
	},
	.id_table = aw2015_led_id,
};

static int __init aw2015_led_init(void)
{
	return i2c_add_driver(&aw2015_led_driver);
}
module_init(aw2015_led_init);

static void __exit aw2015_led_exit(void)
{
	i2c_del_driver(&aw2015_led_driver);
}
module_exit(aw2015_led_exit);

MODULE_AUTHOR("<liweilei@awinic.com.cn>");
MODULE_DESCRIPTION("AWINIC AW2015 LED driver");
MODULE_LICENSE("GPL v2");
