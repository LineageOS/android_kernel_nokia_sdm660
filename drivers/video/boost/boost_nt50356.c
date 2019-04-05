/*
 * NT50356 Boost Driver
 *
 * Copyright (C) 2015 FIH Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/platform_data/boost_nt50356.h>
#include "../../fih/fih_swid.h"

/* Black Box */
#define BBOX_BACKLIGHT_I2C_FAIL do {printk("BBox;%s: I2C fail\n", __func__); printk("BBox::UEC;1::1\n");} while (0);
//#define BBOX_BACKLIGHT_GPIO_FAIL do {printk("BBox;%s: GPIO fail\n", __func__); printk("BBox::UEC;1::1\n");} while (0);

struct nt50356_chip_data
{
	struct device *dev;
	struct nt50356_platform_data *pdata;
	struct regmap *regmap;
};

struct i2c_client *nt50356_i2c_client;

static int reg_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	int ret;
	int retry = 0;

	do {
		ret = regmap_read(map, reg, val);
		if (ret < 0) {
			pr_err("%s: fail, reg=%d\n", __func__, reg);
			retry++;
			msleep(10);
		}
	} while ((ret < 0) && (retry <= 3));

	if (ret < 0) {
		BBOX_BACKLIGHT_I2C_FAIL
	}

	return ret;
}

static int reg_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	int ret;
	int retry = 0;

	do {
		ret = regmap_write(map, reg, val);
		if (ret < 0) {
			pr_err("%s: fail, reg=%d, val=0x%X\n", __func__, reg, val);
			retry++;
			msleep(10);
		}
	} while ((ret < 0) && (retry <= 3));

	if (ret < 0) {
		BBOX_BACKLIGHT_I2C_FAIL
	}

	return ret;
}

//init register, NOT USED
static int nt50356_init_reg(void)
{
	struct	i2c_client *client = nt50356_i2c_client;
	struct	nt50356_chip_data *pchip;
	struct	nt50356_platform_data *pdata;
	unsigned int reg_val;
	int ret = 0;

	if (!client) {
		pr_err("%s: client is NULL\n", __func__);
		return -1;
	}

	pchip = i2c_get_clientdata(client);
	if (!pchip) {
		pr_err("%s: pchip is NULL\n", __func__);
		return -1;
	}

	pdata = pchip->pdata;
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -1;
	}

#if 0 //NT50356
	reg_read(pchip->regmap, NT50356_REVISION, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_REVISION reg_val = 0x%x ***\n\n", __func__, reg_val);
	reg_read(pchip->regmap, NT50356_BACKLIGHT_CONF_1, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_CONF_1 reg_val = 0x%x ***\n\n", __func__, reg_val);
	reg_read(pchip->regmap, NT50356_BACKLIGHT_ENABLE, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_ENABLE reg_val = 0x%x ***\n\n", __func__, reg_val);

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_ENABLE, 0x1F);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, NT50356_BACKLIGHT_ENABLE, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_ENABLE reg_val = 0x%x ***\n\n", __func__, reg_val);

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_1, 0x89); //0x88 Turn on Backlight w/o PWM
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, NT50356_BACKLIGHT_CONF_1, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_CONF_1 reg_val = 0x%x ***\n\n", __func__, reg_val);
#else //LM36274
	reg_read(pchip->regmap, NT50356_REVISION, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_REVISION reg_val = 0x%x ***\n\n", __func__, reg_val);
	reg_read(pchip->regmap, NT50356_BACKLIGHT_CONF_1, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_CONF_1 reg_val = 0x%x ***\n\n", __func__, reg_val);
	reg_read(pchip->regmap, NT50356_BACKLIGHT_ENABLE, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_ENABLE reg_val = 0x%x ***\n\n", __func__, reg_val);

//test
	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_1, 0x28); //0x88 Turn on Backlight w/o PWM
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, NT50356_BACKLIGHT_CONF_1, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_CONF_1 reg_val = 0x%x ***\n\n", __func__, reg_val);
//test

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_ENABLE, 0x13);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, NT50356_BACKLIGHT_ENABLE, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_ENABLE reg_val = 0x%x ***\n\n", __func__, reg_val);

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_1, 0x28); //0x88 Turn on Backlight w/o PWM
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, NT50356_BACKLIGHT_CONF_1, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: NT50356_BACKLIGHT_CONF_1 reg_val = 0x%x ***\n\n", __func__, reg_val);

//test
	ret = reg_write(pchip->regmap, 0x9, 0x0);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, 0x9, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: 0x9 reg_val = 0x%x ***\n\n", __func__, reg_val);
//test

	/* BIAS settings */
	ret = reg_write(pchip->regmap, NT50356_BSTO_VOLTAGE_SETTING, 0x28);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, 0xD, 0x24);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, 0xD, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: 0xD reg_val = 0x%x ***\n\n", __func__, reg_val);

	ret = reg_write(pchip->regmap, 0xE, 0x24);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, 0xE, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: 0xE reg_val = 0x%x ***\n\n", __func__, reg_val);

	ret = reg_write(pchip->regmap, 0x9, 0x9E);
	if (ret < 0)
		goto out;

	reg_read(pchip->regmap, 0x9, &reg_val);
	pr_err("\n\n*** [LCM BOOST] %s: 0x9 reg_val = 0x%x ***\n\n", __func__, reg_val);
#endif
/*
	ret = reg_write(pchip->regmap, NT50356_CONFIG, nt50356_init_data[0]);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_TIMING, nt50356_init_data[1]);
	if (ret < 0)
		goto out;
*/
	pr_err("\n\n******************** [LCM BOOST] %s, --- **********************\n\n", __func__);

	return ret;

out:
	pr_err("\n\n******************** [LCM BOOST] %s ---, i2c failed to access register return ret = %d **********************\n\n", __func__, ret);

	BBOX_BACKLIGHT_I2C_FAIL
	return ret;

}

int nt50356_suspend(void)
{
	struct	i2c_client *client = nt50356_i2c_client;
	struct	nt50356_chip_data *pchip;
	struct	nt50356_platform_data *pdata;
	int ret = 0;

	pr_debug("\n\n*** [LCM BOOST] %s, +++, ***\n\n", __func__);

	if (!client) {
		pr_err("%s: client is NULL\n", __func__);
		return -1;
	}

	pchip = i2c_get_clientdata(client);
	if (!pchip) {
		pr_err("%s: pchip is NULL\n", __func__);
		return -1;
	}

	pdata = pchip->pdata;
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -1;
	}

	/* Enable BL_EN_GPIO(HWEN) */
	gpio_direction_output(pdata->bl_en_gpio, 0);

	pr_debug("\n\n*** [LCM BOOST] %s, --- ***n\n", __func__);
	return ret;
}
EXPORT_SYMBOL(nt50356_suspend);

int nt50356_resume(void)
{
	struct	i2c_client *client = nt50356_i2c_client;
	struct	nt50356_chip_data *pchip;
	struct	nt50356_platform_data *pdata;
	int ret = 0;
//	int i = 0;
//	unsigned int reg_val;
	struct st_swid_table tb;
	fih_swid_read(&tb);

	pr_debug("\n\n*** [LCM BOOST] %s, +++, ***\n\n", __func__);

	if (!client) {
		pr_err("%s: client is NULLi\n", __func__);
		return -1;
	}

	pchip = i2c_get_clientdata(client);
	if (!pchip) {
		pr_err("%s: pchip is NULL\n", __func__);
		return -1;
	}

	pdata = pchip->pdata;
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -1;
	}

	/* Enable BL_EN_GPIO(HWEN) */
	gpio_direction_output(pdata->bl_en_gpio, 1);
	udelay(5000);

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_ENABLE, 0x00);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_LSB, 0x07);
	if (ret < 0)
		goto out;

	pr_debug("\n\n*** [LCM BOOST] %s, GPIO_62=%d, ***\n\n", __func__, gpio_get_value(pdata->id_gpio));
	if ( gpio_get_value(pdata->id_gpio) ) { /* 0:TI, 1:Novatek */
		if(strstr(saved_command_line, "mdss_mdp.panel=1:dsi:0:fih,mdss_dsi_nt36672_fhd_video"))
			ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_MSB, 0x7F); // 2nd source lcm, backlight 10mA for per sink
		else
			ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_MSB, 0xFF); // Main source lcm, backlight 20mA for per sink
		if (ret < 0)
			goto out;

		ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_1, 0xEB);
		if (ret < 0)
			goto out;

		/* Change PWM sample rate to 24MHz: 0x03 Bits[1:0]=0 */
		ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_2, 0x4F);
		if (ret < 0)
		goto out;
	} else {
		if(strstr(saved_command_line, "mdss_mdp.panel=1:dsi:0:fih,mdss_dsi_nt36672_fhd_video")){
			// 2nd source lcm, backlight 10mA for per sink
			ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_LSB, 0x02);
			if (ret < 0)
				goto out;
			ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_MSB, 0x55);
			if (ret < 0)
				goto out;
		} else {
			// Main source lcm, backlight 20mA for per sink
			ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_BRIGHTNESS_MSB, 0xA0);
			if (ret < 0)
				goto out;
		}

		ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_1, 0xE9);
		if (ret < 0)
			goto out;

		/* Change PWM sample rate to 24MHz: 0x03 Bits[1:0]=0 */
		ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_CONF_2, 0x0C);
		if (ret < 0)
		goto out;
	}

	/* Change PWM sample rate to 24MHz: 0x10 Bits[0]=1 */
	ret = reg_write(pchip->regmap, NT50356_OPTION_1, 0x07);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_BACKLIGHT_ENABLE, 0x1F);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_BSTO_VOLTAGE_SETTING, 0x28);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_SET_AVDD_VOLTAGE, pdata->avdd_voltage);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_SET_AVEE_VOLTAGE, pdata->avee_voltage);
	if (ret < 0)
		goto out;

	ret = reg_write(pchip->regmap, NT50356_BIAS_CONF_1, 0x9C);
	if (ret < 0)
		goto out;
	mdelay(10);

	ret = reg_write(pchip->regmap, NT50356_BIAS_CONF_1, 0x9E);
	if (ret < 0)
		goto out;
	udelay(2 * 1000);

	pr_debug("\n\n*** [LCM BOOST] %s, --- ***\n\n", __func__);
	return ret;

out:
	pr_debug("\n\n*** [LCM BOOST] %s ---, i2c failed to access register return ret = %d ***\n\n", __func__, ret);
	BBOX_BACKLIGHT_I2C_FAIL
	return ret;
}
EXPORT_SYMBOL(nt50356_resume);

static int nt50356_gpio_config(struct nt50356_platform_data *pdata)
{	
	int ret = 0;

	// Configure BL ENABLE GPIO (Output)
	ret =  gpio_request(pdata->bl_en_gpio, "nt50356-bl-en");
	if (ret < 0) {
		gpio_free(pdata->bl_en_gpio);
		pr_err("nt50356-bl-en pin request gpio failed, ret = %d\n", ret);
	}
	else
		gpio_direction_output(pdata->bl_en_gpio, 1);

	// Configure ENP ENABLE GPIO (Output)
	ret =  gpio_request(pdata->enp_gpio, "nt50356-enp");
	if (ret < 0) {
		gpio_free(pdata->enp_gpio);
		pr_err("nt50356-enp pin request gpio failed, ret = %d\n", ret);
	}
	else
		gpio_direction_output(pdata->enp_gpio, 0);//Don't need to pull high enp

	// Configure ENN ENABLE GPIO (Output)
	ret =  gpio_request(pdata->enn_gpio, "nt50356-enn");
	if (ret < 0) {
		gpio_free(pdata->enn_gpio);
		pr_err("nt50356-enn pin request gpio failed, ret = %d\n", ret);
	}
	else
		gpio_direction_output(pdata->enn_gpio, 0);//Don't need to pull high enn

	// Configure ID GPIO (Output)
	ret =  gpio_request(pdata->id_gpio, "nt50356-id");
	if (ret < 0) {
		gpio_free(pdata->id_gpio);
		pr_err("nt50356-id pin request gpio failed, ret = %d\n", ret);
	}
	else
		gpio_direction_input(pdata->id_gpio);

	return ret;
}

static int nt50356_parse_dt(struct nt50356_platform_data *pdata, struct device *dev)
{
	#ifdef CONFIG_OF
	u32 tmp = 0;
	int rc= 0;
	struct device_node *np = dev->of_node;

	rc = of_property_read_u32(np, "novatek,avdd-voltage", &tmp);
	pdata->avdd_voltage = (!rc ? tmp : NT50356_5_8_V);
	rc = of_property_read_u32(np, "novatek,avee-voltage", &tmp);
	pdata->avee_voltage = (!rc ? tmp : NT50356_5_8_V);

	pr_info("[LCM BOOST] %s  pdata->avdd_voltage = 0x%x \n", __func__, pdata->avdd_voltage);
	pr_info("[LCM BOOST] %s  pdata->avee_voltage = 0x%x \n", __func__, pdata->avee_voltage);

	pdata->bl_en_gpio = of_get_named_gpio(np, "novatek,bl-en-gpio", 0);
	pr_info("[LCM BOOST] %s: NT50356_BACKLIGHT_ENABLE gpio = %d \n", __func__, pdata->bl_en_gpio);
	pdata->enp_gpio = of_get_named_gpio(np, "novatek,enp-gpio", 0);
	pr_info("[LCM BOOST] %s: NT50356 ENP gpio = %d \n", __func__, pdata->enp_gpio);
	pdata->enn_gpio = of_get_named_gpio(np, "novatek,enn-gpio", 0);
	pr_info("[LCM BOOST] %s: NT50356 ENN gpio = %d \n", __func__, pdata->enn_gpio);
	pdata->id_gpio = of_get_named_gpio(np, "novatek,id-gpio", 0);
	pr_info("[LCM BOOST] %s: Boost ID gpio = %d \n", __func__, pdata->id_gpio);

	#endif
	return 0;
}

static const struct regmap_config nt50356_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = NT50356_MAX_REG,
};

static int nt50356_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct nt50356_chip_data *pchip;
	struct nt50356_platform_data *pdata;
	int ret = 0;

	pr_debug("\n\n******************** [LCM BOOST] %s, +++ **********************\n\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "[LCM BOOST] fail : i2c functionality check...\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct nt50356_chip_data),
			     GFP_KERNEL);
	if (!pchip)
	{
		dev_err(&client->dev, "[LCM BOOST] %s: Failed to alloc mem for nt50356_chip_data\n", __func__);
		return -ENOMEM;
	}

	pdata = devm_kzalloc(&client->dev, sizeof(struct nt50356_platform_data),
			     GFP_KERNEL);
	if (!pdata)
	{
		dev_err(&client->dev, "[LCM BOOST] %s: Failed to alloc mem for nt50356_platform_data\n", __func__);
		return -ENOMEM;
	}

	nt50356_parse_dt(pdata, &client->dev);
	nt50356_gpio_config(pdata);

	//chip data initialize
	pchip->pdata = pdata;
	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &nt50356_regmap);
	if (IS_ERR(pchip->regmap)) {
		ret = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "[LCM BOOST] fail : allocate register map: %d\n",
			ret);
		return ret;
	}
	i2c_set_clientdata(client, pchip);

	nt50356_i2c_client = client;

	//start mutex
	mutex_init(&pdata->io_lock);

	//init register map
	if (0)
		nt50356_init_reg();

	dev_info(&client->dev, "[LCM BOOST] NT50356 backlight probe OK.\n");

	pr_debug("\n\n******************** [LCM BOOST] %s, --- **********************\n\n", __func__);

	return 0;
}

static int nt50356_remove(struct i2c_client *client)
{
	struct nt50356_chip_data *pchip = i2c_get_clientdata(client);
	struct	nt50356_platform_data *pdata;

	pdata = pchip->pdata;
	if (!pdata) {
		pr_err("[LCM BOOST] %s: pdata is NULL\n", __func__);
		return -1;
	}

	if(pdata)
	{
		mutex_destroy(&pdata->io_lock);
	}

	return 0;
}

//device tree id table
static struct of_device_id nt50356_match_table[] = {
	{ .compatible = "novatek,nt50356", },
	{},
};
MODULE_DEVICE_TABLE(of, nt50356_match_table);

static const struct i2c_device_id nt50356_id[] = {
	{ NT50356_DEV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nt50356_id);

static struct i2c_driver nt50356_driver =
{
	.driver =
	{
		.owner = THIS_MODULE,
		.name = NT50356_DEV_NAME,
		.of_match_table = nt50356_match_table,//device tree
	},
	.probe = nt50356_probe,
	.remove = nt50356_remove,
	.id_table = nt50356_id,
};


static int __init nt50356_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&nt50356_driver);
	if( ret != 0)
	{
		pr_err("\n\n******* [LCM BOOST] %s, driver init failed **********\n\n", __func__);
	}
	return ret;
}

static void __exit nt50356_exit(void)
{
	i2c_del_driver(&nt50356_driver);
}
module_init(nt50356_init);
module_exit(nt50356_exit);

MODULE_DESCRIPTION("NovaTek Power Boost Converter Driver for NT50356");
MODULE_AUTHOR("FIH Corp.");
MODULE_VERSION(NT50356_DRV_VER);
MODULE_LICENSE("GPL");

