/*
 * ST STWLC33 Wireless Charging driver
 *
 * Copyright (C) 2017 Foxconn (FIH), Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power/stwlc33_wl_charger.h>


#define STWLC33_WLC_DEV_NAME "stwlc33_wlc"

#define WLC_RXMODE_WORK_DELAY		10000

#define RX_VOUT_SET_MV(rx_vout_set_adc)    \
    (rx_vout_set_adc * 100 + 3500)

#define RX_VOUT_MV(rx_vout_adc)    \
    (rx_vout_adc)

#define RX_IOUT_MA(rx_iout_adc)    \
    (rx_iout_adc)

#define RX_VRECT_MV(rx_vrect_adc)  \
    (rx_vrect_adc)

#define RX_TEMP_CELSIUS(rx_temp_adc)  \
    (377 - (rx_temp_adc /2 ))


static int NVM[] = {
/* #s-00: */ 0x41, 0x3D, 0x25, 0x78, 0x0F, 0xA2, 0x09, 0x05, 0x0F, 0x2C, 0x89, 0x07, 0x26, 0x00, 0x00, 0x3D, 0xC0, 0x07, 0x81, 0x02, 0x14, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-01: */ 0x4B, 0x3D, 0x25, 0x66, 0x14, 0xA2, 0x0F, 0x0D, 0x37, 0x0F, 0x89, 0x07, 0x26, 0x00, 0x00, 0x3C, 0x72, 0x02, 0x85, 0x02, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-02: */ 0x2A, 0x1E, 0x1E, 0x38, 0x00, 0xFF, 0x00, 0x14, 0x00, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-03: */ 0x00, 0x16, 0x00, 0x11, 0x22, 0x33, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x0A, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-04: */ 0x44, 0x3D, 0x15, 0x78, 0x15, 0x90, 0x15, 0x05, 0x15, 0x2C, 0x89, 0x0F, 0x66, 0x19, 0x1E, 0x1E, 0x00, 0x00, 0x00, 0x01, 0x14, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-05: */ 0x38, 0x03, 0x00, 0x00, 0x00, 0x25, 0x8C, 0x07, 0x03, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-06: */ 0x00, 0xAA, 0x10, 0x00, 0x00, 0x00, 0x25, 0x50, 0x02, 0x63, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* #s-07: */ 0x9A, 0x01, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const struct i2c_device_id stwlc33_wlc_id[] = {
    {STWLC33_WLC_DEV_NAME, 0},
    {},
};

static struct of_device_id stwlc33_wlc_match_table[] = {
    { .compatible = "wl,stwlc33",},
    { },
};

static char *stwlc33_power_supplied_to[] = {
    "battery",
};

static enum power_supply_property stwlc33_power_props_wireless[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
};

struct stwlc33_wlc_chip *stwlc33_chip = NULL;

static bool stwlc33_is_dc_present(struct stwlc33_wlc_chip *chip)
{
    union power_supply_propval pval = {0, };

    if (!chip->dc_psy)
        chip->dc_psy = power_supply_get_by_name("dc");

    if (chip->dc_psy)
        power_supply_get_property(chip->dc_psy,
                POWER_SUPPLY_PROP_PRESENT, &pval);
    else
        return false;

    pr_debug("stwlc33: %s %d\n", __func__, pval.intval);
    return pval.intval != 0;
}

#if 0
static bool stwlc33_is_dc_online(struct stwlc33_wlc_chip *chip)
{
    union power_supply_propval pval = {0, };

    if (!chip->dc_psy)
        chip->dc_psy = power_supply_get_by_name("dc");

    if (chip->dc_psy)
        power_supply_get_property(chip->dc_psy,
                POWER_SUPPLY_PROP_ONLINE, &pval);
    else
        return false;

    pr_debug("stwlc33: %s %d\n", __func__, pval.intval);
    return pval.intval != 0;
}
#endif

static int stwlc33_i2c_read_single(struct i2c_client *client, char *addr_reg, char *readbuf)
{
    struct i2c_msg msgs[2];
    int ret = -1;

    msgs[0].flags = 0;
    msgs[0].addr  = client->addr;
    msgs[0].len   = 1;
    msgs[0].buf   = addr_reg;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = 1;
    msgs[1].buf   = readbuf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0) {
        stwlc33_chip->i2c_failed = true;
        pr_err("stwlc33: stwlc33_i2c_read error ret = %d\n", ret);
    } else {
        stwlc33_chip->i2c_failed = false;
    }

    pr_debug("stwlc33: address=0x%02x reg=0x%02x data=0x%02x\n", stwlc33_chip->client->addr, *addr_reg, *readbuf);

    return ret;
}

static int stwlc33_read_reg_single(unsigned char readaddr, unsigned char *readdata)
{
    int ret = 0;

    if (stwlc33_is_dc_present(stwlc33_chip) == true) {
        ret = stwlc33_i2c_read_single(stwlc33_chip->client, &readaddr, readdata);
        if (ret < 0) {
            return ret;
        }
    } else {
        pr_debug("stwlc33: Cann't read reg. dc not present or online\n");
        return -1;
    }

    return 0;
}

static int stwlc33_read_reg(unsigned char readaddr, unsigned char readsize, unsigned char *readdata)
{
    int ret = 0;
    int i;

    for (i=0; i<readsize; i++)
    {
        ret = stwlc33_read_reg_single(readaddr + i, readdata + i);
        if (ret < 0) {
            pr_debug("stwlc33: read address=0x%02x reg=0x%02x read failed\n", stwlc33_chip->client->addr, readaddr + i);
            return ret;
        }
    }

    return 0;
}

static int stwlc33_i2c_write_single(struct i2c_client *client, uint8_t *writedata)
{
    struct i2c_msg msgs[1];
    int ret = -1;

    msgs[0].flags = 0;
    msgs[0].addr  = client->addr;
    msgs[0].len   = 2;
    msgs[0].buf   = writedata;

    ret = i2c_transfer(client->adapter, msgs, 1);
    if (ret < 0) {
        stwlc33_chip->i2c_failed = true;
        pr_err("stwlc33: stwlc33_i2c_read error ret = %d\n", ret);
    } else {
        stwlc33_chip->i2c_failed = false;
    }

    //pr_debug("stwlc33: write address=0x%02x reg=0x%02x data=0x%02x\n", stwlc33_chip->client->addr, writedata[0], writedata[1]);

    return ret;
}

static int stwlc33_write_reg_single(unsigned char writeaddr, unsigned char *writedata)
{
    int ret = 0;

    if (stwlc33_is_dc_present(stwlc33_chip) == true) {
        uint8_t data[2];
        data[0] = writeaddr;
        data[1] = *writedata;
        ret = stwlc33_i2c_write_single(stwlc33_chip->client, data);
        if (ret < 0) {
            return ret;
        }
    } else {
        pr_debug("stwlc33: Cann't write reg. dc not present or online\n");
        return -1;
    }

    return 0;
}

#if 0
static int stwlc33_write_reg(unsigned char writeaddr, unsigned char writesize, unsigned char *writedata)
{
	int ret = 0;
    int i = 0;

	for (i=0; i<writesize; i++)
	{
		ret = stwlc33_write_reg_single(writeaddr + i, writedata + i);
		if (ret < 0) {
			pr_debug("stwlc33: address=0x%02x reg=0x%02x read failed\n", stwlc33_chip->client->addr, writeaddr + i);
			return ret;
		}
	}

    return 0;
}
#endif

#define STWLC33_RX_FUNC(func_name, len, rx_reg)                                     \
static u16 stwlc33_## func_name(void)                                               \
{                                                                                   \
    int ret = 0;                                                                    \
    unsigned char func_name[2] = {0x00, 0x00};                                      \
    u16 *p_## func_name = (u16 *)func_name;                                         \
                                                                                    \
    ret = stwlc33_read_reg(rx_reg, len, func_name);                                 \
    if (ret < 0) {                                                                  \
        pr_err("stwlc33: Fail read %s\n", __func__);                                \
        return 0;                                                                   \
    }                                                                               \
                                                                                    \
    return *p_## func_name;                                                         \
}

STWLC33_RX_FUNC(chip_id       , 1, STWLC33_REG_CHIP_ID);
STWLC33_RX_FUNC(chip_rev      , 1, STWLC33_REG_CHIP_REV);
STWLC33_RX_FUNC(fw_majver     , 1, STWLC33_REG_FW_MAJVER);
STWLC33_RX_FUNC(fw_minver     , 1, STWLC33_REG_FW_MINVER);
STWLC33_RX_FUNC(rx_status     , 1, STWLC33_REG_RX_STATUS);
STWLC33_RX_FUNC(rx_int        , 1, STWLC33_REG_RX_INT);
STWLC33_RX_FUNC(rx_int_enable , 1, STWLC33_REG_RX_INT_ENABLE);
STWLC33_RX_FUNC(rx_int_clear  , 1, STWLC33_REG_RX_INT_CLEAR);
STWLC33_RX_FUNC(rx_ilim_set   , 1, STWLC33_REG_RX_ILIM_SET);
STWLC33_RX_FUNC(rx_op_mode    , 1, STWLC33_REG_RX_SYS_OP_MODE);
STWLC33_RX_FUNC(rx_com        , 1, STWLC33_REG_RX_COM);
STWLC33_RX_FUNC(rx_ovp_set    , 1, STWLC33_REG_RX_OVP_SET);
STWLC33_RX_FUNC(rx_pma_adv    , 2, STWLC33_REG_RX_PMA_ADV);
STWLC33_RX_FUNC(rx_prmc_id    , 2, STWLC33_REG_RX_PRMC_ID);
STWLC33_RX_FUNC(rx_vout_set   , 1, STWLC33_REG_RX_VOUT_SET);
STWLC33_RX_FUNC(rx_vout	      , 2, STWLC33_REG_RX_VOUT);
STWLC33_RX_FUNC(rx_vrect      , 2, STWLC33_REG_RX_VRECT);
STWLC33_RX_FUNC(rx_iout       , 2, STWLC33_REG_RX_IOUT);
STWLC33_RX_FUNC(rx_die_temp   , 2, STWLC33_REG_RX_DIE_TEMP);
STWLC33_RX_FUNC(rx_op_freq    , 2, STWLC33_REG_RX_OP_FREQ);
STWLC33_RX_FUNC(rx_ping_freq  , 2, STWLC33_REG_RX_PING_FREQ);

static int stwlc33_power_get_property_wireless(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    unsigned int reg_rx_vout = 0x0, reg_rx_iout = 0x0;
    unsigned int rx_vout = 0, rx_iout = 0;

    switch (psp) {
    case POWER_SUPPLY_PROP_PRESENT:
        reg_rx_vout = stwlc33_rx_vout();
        if (reg_rx_vout != 0)
        {
            rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout) * 1000;
            val->intval = (rx_vout > 0) ? 1 : 0;
        } else {
            val->intval = 0;
        }
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        reg_rx_iout = stwlc33_rx_iout();
        rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout) * 1000;
        val->intval = (rx_iout > 0) ? 1 : 0;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        reg_rx_vout = stwlc33_rx_vout();
        if (reg_rx_vout != 0)
        {
            rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout) * 1000;
            val->intval = rx_vout;
        } else {
            val->intval = 0;
        }
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        reg_rx_iout = stwlc33_rx_iout();
        rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout) * 1000;
        val->intval = rx_iout;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static void stwlc33_rxmode_dump_info(struct stwlc33_wlc_chip *chip)
{
    unsigned int reg_rx_status = 0x0, reg_rx_vout = 0x0, reg_rx_iout = 0x0, reg_rx_vrect = 0x0, reg_rx_die_temp = 0x0;
    unsigned int rx_vout = 0, rx_iout = 0, rx_vrect = 0, rx_temp = 0;

    /* Read wireless charger chip status */
    reg_rx_status = stwlc33_rx_status();
    msleep(5);

    /* Read wireless charger rx vout */
    reg_rx_vout = stwlc33_rx_vout();
    msleep(5);

    /* Read wireless charger rx iout */
    reg_rx_iout = stwlc33_rx_iout();
    msleep(5);

    /* Read wireless charger rx vrect */
    reg_rx_vrect = stwlc33_rx_vrect();
    msleep(5);

    /* Read wireless charger chip temperature */
    reg_rx_die_temp = stwlc33_rx_die_temp();
    msleep(5);

    rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout);
    rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout);
    rx_vrect = (unsigned int)RX_VRECT_MV(reg_rx_vrect);
    rx_temp = (unsigned int)RX_TEMP_CELSIUS(reg_rx_die_temp);

    pr_info("stwlc33: rx_status:0x%02x, rx_vout:(0x%04x)%d mV, rx_iout:(0x%04x)%d mA, rx_vrect:(0x%04x)%d mV, rx_temp=(0x%x)%d C\n",
        reg_rx_status,
        reg_rx_vout, rx_vout,
        reg_rx_iout, rx_iout,
        reg_rx_vrect, rx_vrect,
        reg_rx_die_temp, rx_temp);
}

void stwlc33_wlc_start_rxmode_monitor(struct stwlc33_wlc_chip *chip)
{
    chip->i2c_failed = false;
    schedule_delayed_work(&chip->wlc_rxmode_monitor_work, msecs_to_jiffies(WLC_RXMODE_WORK_DELAY));
}

void stwlc33_wlc_stop_rxmode_monitor(struct stwlc33_wlc_chip *chip)
{
    cancel_delayed_work(&chip->wlc_rxmode_monitor_work);
}

static int stwlc33_nvm_upgrade(struct stwlc33_wlc_chip *chip)
{
    int x, y;
    int r, retry_limit = 10;
    int verify_pass = 1;
    unsigned char write_addr = 0x00;
    unsigned char write_data = 0x00;
    unsigned char read_addr = 0x00;
    unsigned char read_data = 0x00;

    mutex_lock(&chip->nvm_lock);
    pr_info("stwlc33: NVM upgrade begin\n");

    // TODO: check chip id

    for (r=0; r<retry_limit; r++)
    {
        verify_pass = 1;

        // write NVM
        for (y=0; y<8; y++)
        {
            pr_info("stwlc33: #s-0%d: ", y);
            for (x=0; x<0x20; x++)
            {
                write_addr = 0x90 + x;
                write_data = NVM[0x20 * y + x];
                pr_info("stwlc33: WRITE [0x%02x] = 0x%02x\n", write_addr, write_data);
                stwlc33_write_reg_single(write_addr, &write_data);
            }
            write_addr = 0x8F;
            write_data = 0x20 | (0x0F & y);
            pr_info("stwlc33: WRITE [0x%02x] = 0x%02x\n\n", write_addr, write_data);
            stwlc33_write_reg_single(write_addr, &write_data);
            msleep(100);
        }

        // read NVM
        for (y=0; y<8; y++)
        {
            pr_info("stwlc33: VERIFY #s-0%d\n", y);
            write_addr = 0x8F;
            write_data = 0x40 | (0x0F & y);
            stwlc33_write_reg_single(write_addr, &write_data);
            msleep(100);
            for (x=0; x<0x20; x++)
            {
                read_addr = 0x90 + x;
                stwlc33_read_reg_single(read_addr, &read_data);
                if (read_data != NVM[0x20 * y + x])
                {
                    pr_err("stwlc33: READ VERIFY FAIL x=%d y=%d expect=0x%02x read_value=0x%02x\n", x, y, NVM[0x20 * y + x], read_data);
                    verify_pass = 0;
                    break;
                }
            }
        }

        if (verify_pass)
            break;
        else
        {
            pr_err("stwlc33: VERIFIED FAILED.  WAIT 1 SECOND AND UPGRADE NVM AGAIN\n");
            msleep(1000);
        }
    }

    if (verify_pass)
        pr_info("stwlc33: NVM upgrade finish\n");
    else
        pr_info("stwlc33: NVM upgrade error\n");

    mutex_unlock(&chip->nvm_lock);

    return verify_pass;
}

static void stwlc33_wlc_rxmode_monitor_work(struct work_struct *work)
{
    struct stwlc33_wlc_chip *chip = container_of(work,
                struct stwlc33_wlc_chip,
                wlc_rxmode_monitor_work.work);

    pr_debug("stwlc33: %s Enter\n", __func__);

    /* check chip match */
    if (chip->chip_match == CHIP_NOT_CONFIRM)
    {
        /* have a chance to confirm wireless chip id */
        unsigned char id = 0x00;
        int ret = 0;
        ret = stwlc33_read_reg_single(0x00, &id);
        if (ret >= 0)
        {
            if (id == STWLC33_CHIP_ID)
            {
                pr_info("stwlc33: STWLC33 CHIP MATCH\n");
                chip->chip_match = CHIP_CONFIRM_MATCH;
            }
            else
            {
                pr_err("stwlc33: STWLC33 CHIP DISMATCH.  Read 0x%02x but expected 0x%02x\n", id, STWLC33_CHIP_ID);
                chip->chip_match = CHIP_CONFIRM_DISMATCH;
            }
        }
    }

    if (chip->chip_match == CHIP_CONFIRM_MATCH)
    {
        if (!chip->i2c_failed) {
            /* Debugging */
            stwlc33_rxmode_dump_info(chip);
        }

        if (stwlc33_is_dc_present(chip) == true)
        {
            schedule_delayed_work(&chip->wlc_rxmode_monitor_work, msecs_to_jiffies(WLC_RXMODE_WORK_DELAY));
        }
    }

    pr_debug("stwlc33: %s End\n", __func__);
}

static ssize_t stwlc33_nvm_upgrade_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct stwlc33_wlc_chip *chip = stwlc33_chip;
    ssize_t len = 0;

    pr_debug("stwlc33: %s Enter\n", __func__);

    if (stwlc33_nvm_upgrade(chip))
        len += sprintf(buf + len, "DONE\n");
    else
        len += sprintf(buf + len, "ERROR\n");

    pr_debug("stwlc33: %s End\n", __func__);
    return len;
}

#define SHOW_OPS(ops_name)                                      \
static ssize_t stwlc33_## ops_name ##_show(struct device *dev,  \
        struct device_attribute *attr, char *buf)               \
{                                                               \
    ssize_t len = 0;                                            \
    int ops_name = 0x0;                                         \
                                                                \
    ops_name = stwlc33_## ops_name();                           \
    len += sprintf(buf + len, "0x%x\n", ops_name);              \
                                                                \
    return len;                                                 \
}                                                               \
                                                                \
static DEVICE_ATTR(ops_name, S_IRUGO | S_IWUSR | S_IWGRP,       \
            stwlc33_## ops_name ##_show, NULL);

static DEVICE_ATTR(nvm_upgrade, S_IRUGO | S_IWUSR | S_IWGRP,
            stwlc33_nvm_upgrade_show, NULL);

SHOW_OPS(chip_id);
SHOW_OPS(chip_rev);
SHOW_OPS(fw_majver);
SHOW_OPS(fw_minver);
SHOW_OPS(rx_status);
SHOW_OPS(rx_int);
SHOW_OPS(rx_int_enable);
SHOW_OPS(rx_int_clear);
SHOW_OPS(rx_ilim_set);
SHOW_OPS(rx_op_mode);
SHOW_OPS(rx_com);
SHOW_OPS(rx_ovp_set);
SHOW_OPS(rx_pma_adv);
SHOW_OPS(rx_prmc_id);
SHOW_OPS(rx_vout_set);
SHOW_OPS(rx_vout);
SHOW_OPS(rx_vrect);
SHOW_OPS(rx_iout);
SHOW_OPS(rx_die_temp);
SHOW_OPS(rx_op_freq);
SHOW_OPS(rx_ping_freq);

static struct attribute *stwlc33_attrs[] = {
    &dev_attr_chip_id.attr,
    &dev_attr_chip_rev.attr,
    &dev_attr_fw_majver.attr,
    &dev_attr_fw_minver.attr,
    &dev_attr_rx_status.attr,
    &dev_attr_rx_int.attr,
    &dev_attr_rx_int_enable.attr,
    &dev_attr_rx_int_clear.attr,
    &dev_attr_rx_ilim_set.attr,
    &dev_attr_rx_op_mode.attr,
    &dev_attr_rx_com.attr,
    &dev_attr_rx_ovp_set.attr,
    &dev_attr_rx_pma_adv.attr,
    &dev_attr_rx_prmc_id.attr,
    &dev_attr_rx_vout_set.attr,
    &dev_attr_rx_vout.attr,
    &dev_attr_rx_vrect.attr,
    &dev_attr_rx_iout.attr,
    &dev_attr_rx_die_temp.attr,
    &dev_attr_rx_op_freq.attr,
    &dev_attr_rx_ping_freq.attr,
    &dev_attr_nvm_upgrade.attr,
    NULL,
};

static struct attribute_group stwlc33_attrs_group = {
    .attrs = stwlc33_attrs,
};

static int stwlc33_wlc_suspend(struct device *dev)
{
    pr_debug("stwlc33: %s Enter\n", __func__);

    return 0;
}

static int stwlc33_wlc_resume(struct device *dev)
{
    pr_debug("stwlc33: %s Enter\n", __func__);

    return 0;
}

static const struct dev_pm_ops stwlc33_wlc_pm_ops = {
    .suspend = stwlc33_wlc_suspend,
    .resume = stwlc33_wlc_resume,
};

static int stwlc33_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
    struct power_supply *psy = v;
    struct stwlc33_wlc_chip *chip = container_of(nb, struct stwlc33_wlc_chip, nb);

    if (chip->chip_match == CHIP_CONFIRM_DISMATCH)
        return NOTIFY_OK;

    if (!strcmp(psy->desc->name, "dc")) {
        if (!chip->dc_psy) {
            chip->dc_psy = psy;
        }

        if (ev == PSY_EVENT_PROP_CHANGED) {
            if (stwlc33_is_dc_present(chip) == true) {
                if (chip->chip_match == CHIP_CONFIRM_MATCH || chip->chip_match == CHIP_NOT_CONFIRM)
                {
                    pr_info("stwlc33: wireless charger plugin\n");
                    stwlc33_wlc_start_rxmode_monitor(chip);
                }
            } else {
                if (chip->chip_match == CHIP_CONFIRM_MATCH)
                {
                    pr_info("stwlc33: wireless charger unplug\n");
                    stwlc33_wlc_stop_rxmode_monitor(chip);
                }
            }
        }
    }

    return NOTIFY_OK;
}

static int STWLC33_register_notifier(struct stwlc33_wlc_chip *chip)
{
    int rc;

    chip->nb.notifier_call = stwlc33_notifier_call;
    rc = power_supply_reg_notifier(&chip->nb);
    if (rc < 0) {
        pr_err("stwlc33: Couldn't register psy notifier rc = %d\n", rc);
        return rc;
    }

    return 0;
}

#if 0
static int stwlc33_wl_request_named_gpio(struct device *dev,
		const char *label, int *gpio)
{
    int rc = 0;
    struct device_node *np = dev->of_node;

    if (dev == NULL) {
        pr_err("stwlc33: dev is null\n");
        return -EFAULT;
    }

    if (np == NULL) {
        pr_err("stwlc33: np is null\n");
        return -EFAULT;
    }

    if (gpio == NULL) {
        pr_err("stwlc33: gpio is null\n");
        return -EFAULT;
    }

    rc = of_get_named_gpio(np, label, 0);
    if (rc < 0) {
        pr_err("stwlc33: failed to get '%s'\n", label);
        return rc;
    }
    *gpio = rc;

    rc = devm_gpio_request(dev, *gpio, label);
    if (rc) {
        pr_err("stwlc33: failed to request gpio %d\n", *gpio);
        return rc;
    }

    return rc;
}
#endif

#if 0
static int stwlc33_wlc_parse_dt(struct stwlc33_wlc_chip *chip)
{
    int rc = 0;
    struct device *dev = chip->dev;

    pr_info("stwlc33: %s Enter\n", __func__);

    /* Setup irq pin */
    rc = stwlc33_wl_request_named_gpio(dev, "wl,irq-gpio", &chip->irq_gpio);
    if (rc) {
        pr_err("stwlc33: Unable to get irq_gpio \n");
        return rc;
    }

    /* Setup enable pin */
    rc = stwlc33_wl_request_named_gpio(dev, "wl,enable-gpio", &chip->enable_gpio);
    if (rc) {
        pr_err("stwlc33: Unable to get enable_gpio \n");
        return rc;
    }

    /* Setup rx on pin */
    rc = stwlc33_wl_request_named_gpio(dev, "wl,rx-on-gpio", &chip->rx_on_gpio);
    if (rc)	{
        pr_err("stwlc33: Unable to get rx_on_gpio \n");
        return rc;
    }

    /* Setup tx on pin */
    rc = stwlc33_wl_request_named_gpio(dev, "wl,tx-on-gpio", &chip->tx_on_gpio);
    if (rc) {
        pr_err("stwlc33: Unable to get tx_on_gpio \n");
        return rc;
    }

    pr_info("stwlc33: %s End\n", __func__);
    return 0;
}

static int stwlc33_wlc_pinctrl_init(struct stwlc33_wlc_chip *chip)
{
    int rc = 0;
    struct device *dev = chip->dev;

    pr_info("stwlc33: %s Enter\n", __func__);

    if (dev == NULL) {
        pr_err("stwlc33: dev is null\n");
        return -EFAULT;
    }

    chip->wl_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR_OR_NULL(chip->wl_pinctrl)) {
        pr_err("stwlc33: Target does not use pinctrl\n");
        rc = PTR_ERR(chip->wl_pinctrl);
        goto err;
    }


    /* Setup pinctrl state of interrupt */
    chip->wlc_intr_active =
    pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_intr_active");
    if (IS_ERR_OR_NULL(chip->wlc_intr_active)) {
        pr_err("stwlc33: Cannot get pmx_wlc_intr_active pinstate\n");
        rc = PTR_ERR(chip->wlc_intr_active);
        goto err;
    }


    /* Setup pinctrl state of rx path */
    chip->wlc_rxpath_active =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_rx_path_active");
    if (IS_ERR_OR_NULL(chip->wlc_rxpath_active)) {
        pr_err("stwlc33: Cannot get pmx_wlc_rx_path_active pinstate\n");
        rc = PTR_ERR(chip->wlc_rxpath_active);
        goto err;
    }

    chip->wlc_rxpath_sleep =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_rx_path_sleep");
    if (IS_ERR_OR_NULL(chip->wlc_rxpath_sleep)) {
        pr_err("stwlc33: Cannot get pmx_wlc_rx_path_sleep pinstate\n");
        rc = PTR_ERR(chip->wlc_rxpath_sleep);
        goto err;
    }


    /* Setup pinctrl state of tx path */
    chip->wlc_txpath_active =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_tx_path_active");
    if (IS_ERR_OR_NULL(chip->wlc_txpath_active)) {
        pr_err("stwlc33: Cannot get pmx_wlc_tx_path_active pinstate\n");
        rc = PTR_ERR(chip->wlc_txpath_active);
        goto err;
    }

    chip->wlc_txpath_sleep =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_tx_path_sleep");
    if (IS_ERR_OR_NULL(chip->wlc_txpath_sleep)) {
        pr_err("stwlc33: Cannot get pmx_wlc_tx_path_sleep pinstate\n");
        rc = PTR_ERR(chip->wlc_txpath_sleep);
        goto err;
    }


    /* Setup pinctrl state of sleep */
    chip->wlc_sleep =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_sleep");
    if (IS_ERR_OR_NULL(chip->wlc_sleep)) {
        pr_err("stwlc33: Cannot get pmx_wlc_sleep pinstate\n");
        rc = PTR_ERR(chip->wlc_sleep);
        goto err;
    }

    pr_info("stwlc33: %s End\n", __func__);
    return 0;

err:
    chip->wl_pinctrl = NULL;
    chip->wlc_intr_active = NULL;
    chip->wlc_rxpath_active = NULL;
    chip->wlc_rxpath_sleep = NULL;
    chip->wlc_txpath_active = NULL;
    chip->wlc_txpath_sleep = NULL;
    chip->wlc_sleep = NULL;
    return rc;
}
#endif

static irqreturn_t stwlc33_wlc_handler(int irq, void *data)
{
    pr_info("stwlc33: %s\n", __func__);

    return IRQ_HANDLED;
}

static int stwlc33_wlc_hw_init(struct stwlc33_wlc_chip *chip)
{
    int rc = 0;

    pr_info("stwlc33: %s Enter\n", __func__);

    ///* Turn off TX path */
    ///* Turn on RX path */
    ///* set wireless charger chipset enable */
    //if (IS_ERR_OR_NULL(chip->wlc_txpath_sleep)) {
    //    pr_err("stwlc33: not a valid wlc_txpath_sleep pinstate\n");
    //    return -EINVAL;
    //}
    //
    //rc = pinctrl_select_state(chip->wl_pinctrl, chip->wlc_txpath_sleep);
    //if (rc) {
    //    pr_err("stwlc33: Cannot set wlc_txpath_sleep pinstate\n");
    //    return rc;
    //}
    //
    //if (IS_ERR_OR_NULL(chip->wlc_rxpath_active)) {
    //    pr_err("stwlc33: not a valid wlc_rxpath_active pinstate\n");
    //    return -EINVAL;
    //}
    //
    //rc = pinctrl_select_state(chip->wl_pinctrl, chip->wlc_rxpath_active);
    //if (rc) {
    //    pr_err("stwlc33: Cannot set wlc_rxpath_active pinstate\n");
    //    return rc;
    //}

    chip->chipset_enabled = CHIPSET_ENABLE;

    /* Setup irq */
    if (!gpio_is_valid(chip->irq_gpio)) {
        pr_err("stwlc33: irq_gpio is invalid.\n");
        return -EINVAL;
    }

    rc = gpio_direction_input(chip->irq_gpio);
    if (rc) {
        pr_err("stwlc33: gpio_direction_input (irq) failed.\n");
        return rc;
    }
    chip->irq_num = gpio_to_irq(chip->irq_gpio);

    rc = request_irq(chip->irq_num, stwlc33_wlc_handler,
            IRQF_TRIGGER_FALLING,
            "stwlc33_wireless_charger", chip);
    if (rc < 0) {
        pr_err("stwlc33: stwlc33_wireless_charger request irq failed\n");
        return rc;
    }

    disable_irq(chip->irq_num);

    pr_info("stwlc33: %s End\n", __func__);
    return 0;
}

static int stwlc33_wlc_hw_deinit(struct stwlc33_wlc_chip *chip)
{
    struct device *dev = chip->dev;

    pr_info("stwlc33: %s Enter\n", __func__);

    if (chip->irq_gpio > 0)
        devm_gpio_free(dev, chip->irq_gpio);

    free_irq(chip->irq_num, chip);

    pr_info("stwlc33: %s End\n", __func__);
    return 0;
}

#if 0
static void stwlc33_wlc_interrupt_work(struct work_struct *work)
{
    struct stwlc33_wlc_chip *chip = container_of(work,
        struct stwlc33_wlc_chip,
        wlc_interrupt_work);

    WLC_DBG_INFO("stwlc33_wlc_interrupt_work\n");
    pr_err("stwlc33: stwlc33_wlc_interrupt_work\n");
}
#endif

static int stwlc33_wlc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct stwlc33_wlc_chip *chip;
    struct power_supply_config wireless_psy_cfg = {};
    int rc = 0;

    pr_info("stwlc33: %s Enter\n", __func__);

    if (client->dev.of_node == NULL) {
        pr_err("stwlc33: client->dev.of_node null\n");
        rc = -EFAULT;
        goto err;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("stwlc33: i2c_check_functionality error\n");
        rc = -EIO;
        goto err;
    }

    chip = devm_kzalloc(&client->dev,
        sizeof(struct stwlc33_wlc_chip), GFP_KERNEL);
    if (!chip) {
        pr_err("stwlc33: Failed to allocate memory\n");
        rc = -ENOMEM;
        goto err;
    }

    chip->client = client;
    chip->dev = &client->dev;

#if 0
    rc = stwlc33_wlc_parse_dt(chip);
    if (rc) {
        pr_err("stwlc33: DT parsing failed\n");
        goto free_alloc_mem;
    }

    rc = stwlc33_wlc_pinctrl_init(chip);
    if (rc) {
        pr_err("stwlc33: pinctrl init failed\n");
        goto free_alloc_mem;
    }
#endif

    chip->chip_match = CHIP_NOT_CONFIRM;
    chip->irq_gpio = 21;
    chip->chipset_enabled = CHIPSET_UNINIT;
    chip->i2c_failed = false;

    //INIT_WORK(&chip->wlc_interrupt_work, stwlc33_wlc_interrupt_work);
    INIT_DELAYED_WORK(&chip->wlc_rxmode_monitor_work, stwlc33_wlc_rxmode_monitor_work);
    mutex_init(&chip->lock);
	mutex_init(&chip->nvm_lock);

    /* register power supply notifier */
    STWLC33_register_notifier(chip);

    rc = stwlc33_wlc_hw_init(chip);
    if (rc) {
        pr_err("stwlc33: couldn't init hardware rc = %d\n", rc);
        goto free_alloc_mem;
    }

    i2c_set_clientdata(client, chip);
    pr_info("stwlc33: i2c address: %x\n", client->addr);

    /* PowerSupply */
    chip->wireless_psy_d.name = "stwlc33";
    chip->wireless_psy_d.type = POWER_SUPPLY_TYPE_WIRELESS;
    chip->wireless_psy_d.properties = stwlc33_power_props_wireless;
    chip->wireless_psy_d.num_properties = ARRAY_SIZE(stwlc33_power_props_wireless);
    chip->wireless_psy_d.get_property = stwlc33_power_get_property_wireless;

    wireless_psy_cfg.drv_data = chip;
    wireless_psy_cfg.supplied_to = stwlc33_power_supplied_to;
    wireless_psy_cfg.num_supplicants = ARRAY_SIZE(stwlc33_power_supplied_to);
    wireless_psy_cfg.of_node = chip->dev->of_node;

    chip->wireless_psy = devm_power_supply_register(chip->dev,
        &chip->wireless_psy_d, &wireless_psy_cfg);
    if (IS_ERR(chip->wireless_psy)) {
        pr_err("stwlc33: Unable to register wireless_psy rc = %ld\n", PTR_ERR(chip->wireless_psy));
        rc = PTR_ERR(chip->wireless_psy);
        goto free_client_data;
    }

    /* Register the sysfs nodes */
    if ((rc = sysfs_create_group(&chip->dev->kobj, &stwlc33_attrs_group))) {
        pr_err("stwlc33: Unable to create sysfs group rc = %d\n", rc);
        goto free_client_data;
    }

    stwlc33_chip = chip;

    pr_info("stwlc33: %s End\n", __func__);
    return 0;

free_client_data:
    i2c_set_clientdata(client, NULL);

free_alloc_mem:
    stwlc33_wlc_hw_deinit(chip);

err:
    return rc;
}

static int stwlc33_wlc_remove(struct i2c_client *client)
{
    struct stwlc33_wlc_chip *chip = i2c_get_clientdata(client);

    pr_info("stwlc33: %s Enter\n", __func__);

    cancel_delayed_work_sync(&chip->wlc_rxmode_monitor_work);
    sysfs_remove_group(&chip->dev->kobj, &stwlc33_attrs_group);
    power_supply_unregister(chip->wireless_psy);
    stwlc33_chip = NULL;
    stwlc33_wlc_hw_deinit(chip);
    devm_kfree(&client->dev, chip);

    pr_info("stwlc33: %s End\n", __func__);
    return 0;
}

static struct i2c_driver stwlc33_wlc_driver = {
    .probe = stwlc33_wlc_probe,
    .remove = stwlc33_wlc_remove,
    .driver = {
    .name = STWLC33_WLC_DEV_NAME,
    .owner = THIS_MODULE,
    .of_match_table = stwlc33_wlc_match_table,
#ifdef CONFIG_PM
    .pm = &stwlc33_wlc_pm_ops,
#endif
    },
    .id_table = stwlc33_wlc_id,
};

static int __init stwlc33_wlc_init(void)
{
    pr_info("stwlc33: %s Enter\n", __func__);

    return i2c_add_driver(&stwlc33_wlc_driver);
}

static void __exit stwlc33_wlc_exit(void)
{
    pr_info("stwlc33: %s Enter\n", __func__);

    i2c_del_driver(&stwlc33_wlc_driver);
}

late_initcall(stwlc33_wlc_init);
module_exit(stwlc33_wlc_exit);

MODULE_AUTHOR("AlanWHTsai <alanwhtsai@fih-foxconn.com>");
MODULE_DESCRIPTION("ST STWLC33 Wireless Charger Driver");
MODULE_LICENSE("GPL v2");
