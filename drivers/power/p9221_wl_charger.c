/*
 * IDT P9221 Wireless Charging driver
 *
 * Copyright (C) 2017 Foxconn (FIH), Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/power/p9221_wl_charger.h>
#include "supply/qcom/smb-reg.h"

#define P9221_WLC_DEV_NAME "p9221_wlc"

#define WLC_RXMODE_WORK_DELAY                10000
#define WLC_RXMODE_WORK_DELAY_FROM_RESUME	 100

#define WLC_POWER_PROFILE_SETUP_WORK_DELAY   100
#define WLC_POWER_PROFILE_CHECK_MAX_TIMES    10

#define WLC_UDPATE_FOD_WORK_DELAY            (WLC_POWER_PROFILE_SETUP_WORK_DELAY * WLC_POWER_PROFILE_CHECK_MAX_TIMES + 1000)

#define WLC_CURRENT_LIMIT_WORK_DELAY         7000
#define WLC_CURRENT_LIMIT_RUNTIME_WORK_DELAY 5000
#define WLC_CURRENT_LIMIT_BOOTUP_WORK_DELAY  20000

#define WLC_POWEROFF_CURRENT_LIMIT_RUNTIME_WORK_DELAY 5000
#define WLC_POWEROFF_CURRENT_LIMIT_BOOTUP_WORK_DELAY 10000

#define MAX_CYCLE_PLUGINOUT_COUNT  2
#define FAKE_CHARGING_TIME_SEC     3

#define RX_VOUT_MV(rx_vout_adc)    \
    ((rx_vout_adc * 6 * 2100) / 4095)

#define RX_IOUT_MA(rx_iout_adc)    \
    ((rx_iout_adc * 2 * 2100) / 4095)

#define RX_VRECT_MV(rx_vrect_adc)  \
    ((rx_vrect_adc * 10 * 2100) / 4095)

#define RX_TEMP_CELSIUS(rx_temp_adc)  \
    ((((rx_temp_adc - 1350) * 83) / 444) - 273)


static const struct i2c_device_id p9221_wlc_id[] = {
    {P9221_WLC_DEV_NAME, 0},
    {},
};

static struct of_device_id p9221_wlc_match_table[] = {
    { .compatible = "wl,p9221",},
    { },
};

static char *p9221_power_supplied_to[] = {
    "battery",
};

static enum power_supply_property p9221_power_props_wireless[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
};

struct p9221_wlc_chip *fih_chip = NULL;

static bool p9221_is_dc_present(void)
{
    union power_supply_propval pval = {0, };
    struct power_supply *dc_psy = NULL;

    dc_psy = power_supply_get_by_name("dc");
    if (!dc_psy) {
        pr_err("wlc: %s Fail got dc_psy handle\n", __func__);
        return false;
    }

    power_supply_get_property(dc_psy,
            POWER_SUPPLY_PROP_PRESENT, &pval);
    power_supply_put(dc_psy);

    pr_debug("wlc: %s %d\n", __func__, pval.intval);
    return pval.intval != 0;
}

static bool p9221_is_dc_online(void)
{
    union power_supply_propval pval = {0, };
    struct power_supply *dc_psy = NULL;

    dc_psy = power_supply_get_by_name("dc");
    if (!dc_psy) {
        pr_err("wlc: %s Fail got dc_psy handle\n", __func__);
        return false;
    }

    power_supply_get_property(dc_psy,
            POWER_SUPPLY_PROP_ONLINE, &pval);
    power_supply_put(dc_psy);

    pr_debug("wlc: %s %d\n", __func__, pval.intval);
    return pval.intval != 0;
}

static int p9221_get_dc_current_max(void)
{
    union power_supply_propval pval = {0, };
    struct power_supply *dc_psy = NULL;

    dc_psy = power_supply_get_by_name("dc");
    if (!dc_psy) {
        pr_err("wlc: %s Fail got dc_psy handle\n", __func__);
        return -1;
    }

    power_supply_get_property(dc_psy,
            POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
    power_supply_put(dc_psy);

    pr_debug("wlc: %s %d\n", __func__, pval.intval);
    return pval.intval;
}

static int p9221_get_wipwr_range_status(void)
{
    union power_supply_propval pval = {0, };
    struct power_supply *dc_psy = NULL;

    dc_psy = power_supply_get_by_name("dc");
    if (!dc_psy) {
        pr_err("wlc: %s Fail got dc_psy handle\n", __func__);
        return -1;
    }

    power_supply_get_property(dc_psy,
            POWER_SUPPLY_PROP_WIPWR_RANGE_STATUS, &pval);
    power_supply_put(dc_psy);

    pr_debug("wlc: %s %d\n", __func__, pval.intval);
    return pval.intval;
}

static int p9221_set_dc_current_max(int current_ua)
{
    union power_supply_propval pval = {0, };
    struct power_supply *dc_psy = NULL;
    int rc;

    dc_psy = power_supply_get_by_name("dc");
    if (!dc_psy) {
        pr_err("wlc: %s Fail got dc_psy handle\n", __func__);
        return -1;
    }

    pval.intval = current_ua;
    rc = power_supply_set_property(dc_psy,
            POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
    power_supply_put(dc_psy);

    if (rc < 0) {
        pr_err("wlc: %s Fail set current max ret = %d\n", __func__, rc);
        return -1;
    }

    pr_debug("wlc: %s %d\n", __func__, pval.intval);
    return 0;
}

static int p9221_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
    struct i2c_msg msgs[2];
    int ret = -1;

    msgs[0].flags = 0;
    msgs[0].addr  = client->addr;
    msgs[0].len   = writelen;
    msgs[0].buf   = writebuf;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = readlen;
    msgs[1].buf   = readbuf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0) {
        fih_chip->i2c_failed = true;
        pr_err("wlc: p9221_i2c_read error ret = %d\n", ret);
    } else {
        fih_chip->i2c_failed = false;
    }

    return ret;
}

static int p9221_i2c_write(struct i2c_client *client, uint8_t *writedata, int writesize)
{
    struct i2c_msg msg;
    int ret = -1;

    msg.flags = !I2C_M_RD;
    msg.addr  = client->addr;
    msg.len   = writesize;
    msg.buf   = writedata;

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0) {
        pr_err("p9221_i2c_write error ret = %d\n", ret);
    }

    return ret;
}

static int p9221_read_reg(struct p9221_wlc_chip *chip, unsigned char readaddr, unsigned char readsize, unsigned char *readdata)
{
    int ret = 0;
    unsigned char addr[P9221_REG_LEN] = {0x0, readaddr};

    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true) &&
        (chip->device_suspend == false)) {
        ret = p9221_i2c_read(chip->client, addr, readsize, readdata, readsize);
        if (ret < 0) {
            return ret;
        }
    } else {
        pr_err("wlc: Cann't read reg. dc not present or online device_suspend = %d\n", chip->device_suspend);
        return -1;
    }

    return 0;
}

static int p9221_write_reg(struct p9221_wlc_chip *chip, unsigned char *writedata, unsigned char writesize)
{
    int ret = 0;

    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true) &&
        (chip->device_suspend == false)) {
        ret = p9221_i2c_write(chip->client, writedata, writesize);
        if (ret < 0) {
            return ret;
        }
    } else {
        pr_err("wlc: Cann't write reg. dc not present or online device_suspend = %d\n", chip->device_suspend);
        return -1;
    }

    return 0;
}

static u16 p9221_chipid_force_read(struct p9221_wlc_chip *chip)
{
    int ret = 0;
    unsigned char chipid[P9221_REG_LEN] = {0x0};
    unsigned char addr[P9221_REG_LEN] = {0x0, P9221_REG_CHIPID};

    u16 *p_chipid = (u16 *)chipid;

    ret = p9221_i2c_read(chip->client, addr, P9221_REG_LEN, chipid, P9221_REG_LEN);
    if (ret < 0) {
        return ret;
    }

    pr_info("wlc: %s[1][0]: 0x%x 0x%x\n", __func__, chipid[1], chipid[0]);

    return (*p_chipid);
}

static void p9221_end_power_transfer_trigger(struct p9221_wlc_chip *chip)
{
    int ret = 0;
    unsigned char end_power_transfer_write[P9221_REG_LEN + 1] = {0x0, P9221_REG_RX_END_POWER_TRANSFER, 0x02};
    unsigned char command_reg_read[P9221_REG_LEN] = {0x0};
    unsigned char addr_command_reg[P9221_REG_LEN] = {0x0, P9221_REG_RX_COMMMAND};
    unsigned char command_reg_write[P9221_REG_LEN + 1] = {0x0, P9221_REG_RX_COMMMAND, 0x0};

    /* Set initiates the EPT_Code */
    ret = p9221_i2c_write(chip->client, end_power_transfer_write, P9221_REG_LEN + 1);
    if (ret < 0) {
        return;
    }
    pr_info("wlc: EPT send\n");

    /* Read back the command register value */
    ret = p9221_i2c_read(chip->client, addr_command_reg, P9221_REG_LEN, command_reg_read, P9221_REG_LEN);
    if (ret < 0) {
        return;
    }
    pr_debug("wlc: %s[1][0] command_reg : 0x%x  0x%x\n", __func__, command_reg_read[1], command_reg_read[0]);

    /* Trgger send EPT packet to TX pad */
    command_reg_read[0] |= REG_RX_COMMMAND_SEND_END_OF_POWER;
    command_reg_write[2] = command_reg_read[0];
    ret = p9221_i2c_write(chip->client, command_reg_write, P9221_REG_LEN + 1);
    if (ret < 0) {
        return;
    }
    pr_info("wlc: command send\n");
}

#define P9221_RX_FUNC(func_name, rx_reg)                                            \
static u16 p9221_## func_name(struct p9221_wlc_chip *chip)                                                 \
{                                                                                   \
    int ret = 0;                                                                    \
    unsigned char func_name[P9221_REG_LEN] = {0x0};                                 \
    u16 *p_## func_name = (u16 *)func_name;                                         \
                                                                                    \
    ret = p9221_read_reg(chip, rx_reg, P9221_REG_LEN, func_name);                         \
    if (ret < 0) {                                                                  \
        pr_err("wlc: Fail read %s\n", __func__);                                    \
        return 0;                                                                   \
    }                                                                               \
    pr_debug("wlc: %s[1][0]: 0x%x 0x%x\n", __func__, func_name[1], func_name[0]);   \
                                                                                    \
    return *p_## func_name;                                                         \
}

P9221_RX_FUNC(chipid, P9221_REG_CHIPID);
P9221_RX_FUNC(fw_majver, P9221_REG_FW_MAJVER);
P9221_RX_FUNC(fw_minver, P9221_REG_FW_MINVER);
P9221_RX_FUNC(chipstatus, P9221_REG_CHIPSTATUS);
P9221_RX_FUNC(rx_vout, P9221_REG_RX_VOUT);
P9221_RX_FUNC(rx_vrect, P9221_REG_RX_VOUT);
P9221_RX_FUNC(rx_iout, P9221_REG_RX_IOUT);
P9221_RX_FUNC(rx_temp, P9221_REG_RX_TEMP);
P9221_RX_FUNC(rx_opfreq, P9221_REG_RX_OPFREQ);
P9221_RX_FUNC(fod_0, P9221_REG_FOD_0);
P9221_RX_FUNC(fod_1, P9221_REG_FOD_1);
P9221_RX_FUNC(fod_2, P9221_REG_FOD_2);
P9221_RX_FUNC(fod_3, P9221_REG_FOD_3);
P9221_RX_FUNC(fod_4, P9221_REG_FOD_4);
P9221_RX_FUNC(fod_5, P9221_REG_FOD_5);

static int p9221_power_get_property_wireless(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct p9221_wlc_chip *chip = power_supply_get_drvdata(psy);
    unsigned int reg_rx_vout = 0x0, reg_rx_iout = 0x0;
    unsigned int rx_vout = 0, rx_iout = 0;

    if ((p9221_is_dc_present() != true) && (p9221_is_dc_online() != true)) {
        return -EINVAL;
    }

    switch (psp) {
    case POWER_SUPPLY_PROP_PRESENT:
        reg_rx_vout = p9221_rx_vout(chip);
        rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout);
        val->intval = (rx_vout > 0) ? 1 : 0;
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        reg_rx_iout = p9221_rx_iout(chip);
        rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout);
        val->intval = (rx_iout > 0) ? 1 : 0;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        reg_rx_vout = p9221_rx_vout(chip);
        rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout);
        val->intval = rx_vout;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        reg_rx_iout = p9221_rx_iout(chip);
        rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout);
        val->intval = rx_iout;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static void p9221_rxmode_dump_info(struct p9221_wlc_chip *chip)
{
    unsigned int reg_fod_0 = 0x0, reg_fod_1 = 0x0, reg_fod_2 = 0x0, reg_fod_3 = 0x0, reg_fod_4 = 0x0, reg_fod_5 = 0x0;
    unsigned int reg_status = 0x0, reg_rx_vout = 0x0, reg_rx_iout = 0x0, reg_rx_vrect = 0x0, reg_rx_temp = 0x0;
    unsigned int rx_vout = 0, rx_iout = 0, rx_vrect = 0, rx_temp = 0;

    /* Read wireless charger chip status */
    reg_status = p9221_chipstatus(chip);
    msleep(5);

    /* Read wireless charger rx vout */
    reg_rx_vout = p9221_rx_vout(chip);
    msleep(5);

    /* Read wireless charger rx iout */
    reg_rx_iout = p9221_rx_iout(chip);
    msleep(5);

    /* Read wireless charger rx vrect */
    reg_rx_vrect = p9221_rx_vrect(chip);
    msleep(5);

    /* Read wireless charger chip temperature */
    reg_rx_temp = p9221_rx_temp(chip);
    msleep(5);

    rx_vout = (unsigned int)RX_VOUT_MV(reg_rx_vout);
    rx_iout = (unsigned int)RX_IOUT_MA(reg_rx_iout);
    rx_vrect = (unsigned int)RX_VRECT_MV(reg_rx_vrect);
    rx_temp = (unsigned int)RX_TEMP_CELSIUS(reg_rx_temp);

    pr_info("wlc: status:0x%x, rx_vout:(0x%x)%d mV, rx_iout:(0x%x)%d mA, rx_vrect:(0x%x)%d mV, rx_temp=(0x%x)%d\n",
        reg_status,
        reg_rx_vout, rx_vout,
        reg_rx_iout, rx_iout,
        reg_rx_vrect, rx_vrect,
        reg_rx_temp, rx_temp);

    /* Read FOD field 0 */
    reg_fod_0 = p9221_fod_0(chip);
    msleep(5);

    /* Read FOD field 1 */
    reg_fod_1 = p9221_fod_1(chip);
    msleep(5);

    /* Read FOD field 2 */
    reg_fod_2 = p9221_fod_2(chip);
    msleep(5);

    /* Read FOD field 3 */
    reg_fod_3 = p9221_fod_3(chip);
    msleep(5);

    /* Read FOD field 4 */
    reg_fod_4 = p9221_fod_4(chip);
    msleep(5);

    /* Read FOD field 5 */
    reg_fod_5 = p9221_fod_5(chip);
    msleep(5);

    pr_info("wlc: PP:%s, fod_0:0x%x, fod_1:0x%x, fod_2:0x%x, fod_3:0x%x, fod_4:0x%x, fod_5:0x%x\n",
        (chip->power_profile == WLC_EPP) ? "WLC_EPP" : ((chip->power_profile == WLC_BPP ? "WLC_BPP" : "WLC_NONE")),
        reg_fod_0, reg_fod_1, reg_fod_2,
        reg_fod_3, reg_fod_4, reg_fod_5);
}

static ssize_t p9221_chipmode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;
    ssize_t len = 0;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: i2c client NULL\n");
        return -EFAULT;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: i2c client data NULL\n");
        return -ENODATA;
    }

    mutex_lock(&chip->lock);

    if (chip->chipset_enabled == CHIPSET_DISABLE) {
        len += sprintf(buf + len, "Disable\n");
    } else {
        len += sprintf(buf + len, "RxMode\n");
    }

    mutex_unlock(&chip->lock);

    pr_debug("wlc: %s End\n", __func__);
    return len;
}

static ssize_t p9221_chipmode_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t len)
{
    int rc = 0;
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: i2c client NULL\n");
        return -EFAULT;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: i2c client data NULL\n");
        return -ENODATA;
    }

    if (!chip) {
        pr_err("wlc: %s driver not initial\n", __func__);
        return -1;
    }

    mutex_lock(&chip->lock);

    if (!strncmp(buf, "Disable", strlen("Disable")))
    {
        /* Chipset disable */
        if (IS_ERR_OR_NULL(chip->wlc_rxpath_sleep)) {
            pr_err("wlc: not a valid wlc_sleep pinstate\n");
            rc = -EINVAL;
            goto err;
        }

        rc = pinctrl_select_state(chip->wl_pinctrl, chip->wlc_rxpath_sleep);
        if (rc) {
            pr_err("wlc: Cannot set wlc_rxpath_sleep pinstate\n");
            goto err;
        }

        chip->chipset_enabled = CHIPSET_DISABLE;
    }
    else if (!strncmp(buf, "RxMode", strlen("RxMode")))
    {
        /* Turn Rx path on */
        if (IS_ERR_OR_NULL(chip->wlc_rxpath_active)) {
            pr_err("wlc: not a valid wlc_rxpath_active pinstate\n");
            rc = -EINVAL;
            goto err;
        }

        rc = pinctrl_select_state(chip->wl_pinctrl, chip->wlc_rxpath_active);
        if (rc) {
            pr_err("wlc: Cannot set wlc_rxpath_active pinstate\n");
            goto err;
        }

        chip->chipset_enabled = CHIPSET_ENABLE;
    }

    rc = len;

err:
    mutex_unlock(&chip->lock);
    pr_debug("wlc: %s End\n", __func__);
    return rc;
}

static ssize_t p9221_ept_trigger_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t len)
{
    int rc = 0;
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: i2c client NULL\n");
        return -EFAULT;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: i2c client data NULL\n");
        return -ENODATA;
    }

    mutex_lock(&chip->lock);

    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true)) {
        p9221_end_power_transfer_trigger(chip);
    }

    rc = len;
    mutex_unlock(&chip->lock);
    pr_debug("wlc: %s End\n", __func__);
    return rc;
}

static ssize_t p9221_ppmode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;
    ssize_t len = 0;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: i2c client NULL\n");
        return -EFAULT;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: i2c client data NULL\n");
        return -ENODATA;
    }

    mutex_lock(&chip->lock);

    len += sprintf(buf + len, "%d\n", chip->power_profile);

    mutex_unlock(&chip->lock);

    pr_debug("wlc: %s End\n", __func__);
    return len;
}

#define SHOW_OPS(ops_name)                                      \
static ssize_t p9221_## ops_name ##_show(struct device *dev,    \
        struct device_attribute *attr, char *buf)               \
{                                                               \
    struct i2c_client *client = NULL;                           \
    struct p9221_wlc_chip *chip = NULL;                         \
    ssize_t len = 0;                                            \
    int ops_name = 0x0;                                         \
                                                                \
    /* Get i2c client handle */                                 \
    client = to_i2c_client(dev);                                \
    if (client == NULL) {                                       \
        pr_err("wlc: suspend i2c client NULL\n");               \
        return 0;                                               \
    }                                                           \
                                                                \
    /* Get p9221_wlc_chipt handle */                            \
    chip = i2c_get_clientdata(client);                          \
    if (chip == NULL) {                                         \
        pr_err("wlc: suspend i2c client data NULL\n");          \
        return 0;                                               \
    }                                                           \
                                                                \
    ops_name = p9221_## ops_name(chip);                         \
    len += sprintf(buf + len, "0x%x\n", ops_name);              \
                                                                \
    return len;                                                 \
}                                                               \
                                                                \
static DEVICE_ATTR(ops_name, S_IRUGO | S_IWUSR | S_IWGRP,       \
            p9221_## ops_name ##_show, NULL);

static DEVICE_ATTR(chipmode, S_IRUGO | S_IWUSR | S_IWGRP,
            p9221_chipmode_show, p9221_chipmode_store);

static DEVICE_ATTR(ppmode, S_IRUGO | S_IWUSR | S_IWGRP,
            p9221_ppmode_show, NULL);

static DEVICE_ATTR(ept_trigger, S_IRUGO | S_IWUSR | S_IWGRP,
            NULL, p9221_ept_trigger_store);

SHOW_OPS(chipid);
SHOW_OPS(fw_majver);
SHOW_OPS(fw_minver);
SHOW_OPS(chipstatus);
SHOW_OPS(rx_vout);
SHOW_OPS(rx_vrect);
SHOW_OPS(rx_iout);
SHOW_OPS(rx_temp);
SHOW_OPS(rx_opfreq);
SHOW_OPS(fod_0);
SHOW_OPS(fod_1);
SHOW_OPS(fod_2);
SHOW_OPS(fod_3);
SHOW_OPS(fod_4);
SHOW_OPS(fod_5);

static struct attribute *p9221_attrs[] = {
    &dev_attr_chipmode.attr,
    &dev_attr_ppmode.attr,
    &dev_attr_chipid.attr,
    &dev_attr_fw_majver.attr,
    &dev_attr_fw_minver.attr,
    &dev_attr_chipstatus.attr,
    &dev_attr_rx_vout.attr,
    &dev_attr_rx_vrect.attr,
    &dev_attr_rx_iout.attr,
    &dev_attr_rx_temp.attr,
    &dev_attr_rx_opfreq.attr,
    &dev_attr_fod_0.attr,
    &dev_attr_fod_1.attr,
    &dev_attr_fod_2.attr,
    &dev_attr_fod_3.attr,
    &dev_attr_fod_4.attr,
    &dev_attr_fod_5.attr,
    &dev_attr_ept_trigger.attr,
    NULL,
};

static struct attribute_group p9221_attrs_group = {
    .attrs = p9221_attrs,
};

void p9221_wlc_start_rxmode_monitor(struct p9221_wlc_chip *chip)
{
    chip->i2c_failed = false;
    schedule_delayed_work(&chip->wlc_rxmode_monitor_work, msecs_to_jiffies(WLC_RXMODE_WORK_DELAY));
}

void p9221_wlc_start_rxmode_monitor_from_resume(struct p9221_wlc_chip *chip)
{
    chip->i2c_failed = false;
    schedule_delayed_work(&chip->wlc_rxmode_monitor_work, msecs_to_jiffies(WLC_RXMODE_WORK_DELAY_FROM_RESUME));
}

void p9221_wlc_stop_rxmode_monitor(struct p9221_wlc_chip *chip)
{
    cancel_delayed_work(&chip->wlc_rxmode_monitor_work);
}

static void p9221_wlc_rxmode_monitor_work(struct work_struct *work)
{
    struct p9221_wlc_chip *chip = container_of(work,
                struct p9221_wlc_chip,
                wlc_rxmode_monitor_work.work);

    pr_debug("wlc: %s Enter\n", __func__);

    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true)) {
        if (!chip->i2c_failed) {
            p9221_rxmode_dump_info(chip);
        }

        schedule_delayed_work(&chip->wlc_rxmode_monitor_work, msecs_to_jiffies(WLC_RXMODE_WORK_DELAY));
    }

    pr_debug("wlc: %s End\n", __func__);
}

void p9221_wlc_start_power_profile_setup(struct p9221_wlc_chip *chip)
{
    schedule_delayed_work(&chip->wlc_power_profile_setup_work, msecs_to_jiffies(WLC_POWER_PROFILE_SETUP_WORK_DELAY));
}

void p9221_wlc_stop_power_profile_setup(struct p9221_wlc_chip *chip)
{
    cancel_delayed_work(&chip->wlc_power_profile_setup_work);
}

static void p9221_wlc_power_profile_setup_work(struct work_struct *work)
{
    struct p9221_wlc_chip *chip = container_of(work,
                struct p9221_wlc_chip,
                wlc_power_profile_setup_work.work);
    int wipwr_range_status = 0;

    pr_debug("wlc: %s Enter\n", __func__);

    wipwr_range_status = p9221_get_wipwr_range_status();

    pr_debug("wlc: range status: %d count: %d\n",
        wipwr_range_status, chip->power_profile_check_count);

    /* Steup the power profile is EPP or BPP mode */
    if (wipwr_range_status > (int)DCIN_LT_5P5V_BIT) {
        chip->power_profile = WLC_EPP;
    } else {
        chip->power_profile = WLC_BPP;
    }
    chip->power_profile_check_count++;

    /* keep checking wipwr range status if power_profile is BPP and
       power_profile_check_count is less than WLC_POWER_PROFILE_CHECK_MAX_TIMES */
    if ((chip->power_profile == WLC_BPP) &&
        (chip->power_profile_check_count < WLC_POWER_PROFILE_CHECK_MAX_TIMES)) {
        schedule_delayed_work(&chip->wlc_power_profile_setup_work, msecs_to_jiffies(WLC_POWER_PROFILE_SETUP_WORK_DELAY));
    }
}

void p9221_wlc_start_update_fod(struct p9221_wlc_chip *chip)
{
    schedule_delayed_work(&chip->wlc_udpate_fod_work, msecs_to_jiffies(WLC_UDPATE_FOD_WORK_DELAY));
}

void p9221_wlc_stop_update_fod(struct p9221_wlc_chip *chip)
{
    cancel_delayed_work(&chip->wlc_udpate_fod_work);
}

static void p9221_wlc_update_fod_work(struct work_struct *work)
{
    struct p9221_wlc_chip *chip = container_of(work,
                struct p9221_wlc_chip,
                wlc_udpate_fod_work.work);
    unsigned char bpp_fod_0[4] = {0x00, P9221_REG_FOD_0, BPPFOD_0_A, BPPFOD_0_B};
    unsigned char bpp_fod_1[4] = {0x00, P9221_REG_FOD_1, BPPFOD_1_A, BPPFOD_1_B};
    unsigned char bpp_fod_2[4] = {0x00, P9221_REG_FOD_2, BPPFOD_2_A, BPPFOD_2_B};
    unsigned char bpp_fod_3[4] = {0x00, P9221_REG_FOD_3, BPPFOD_3_A, BPPFOD_3_B};
    unsigned char bpp_fod_4[4] = {0x00, P9221_REG_FOD_4, BPPFOD_4_A, BPPFOD_4_B};
    unsigned char bpp_fod_5[4] = {0x00, P9221_REG_FOD_5, BPPFOD_5_A, BPPFOD_5_B};

    unsigned char epp_fod_0[4] = {0x00, P9221_REG_FOD_0, EPPFOD_0_A, EPPFOD_0_B};
    unsigned char epp_fod_1[4] = {0x00, P9221_REG_FOD_1, EPPFOD_1_A, EPPFOD_1_B};
    unsigned char epp_fod_2[4] = {0x00, P9221_REG_FOD_2, EPPFOD_2_A, EPPFOD_2_B};
    unsigned char epp_fod_3[4] = {0x00, P9221_REG_FOD_3, EPPFOD_3_A, EPPFOD_3_B};
    unsigned char epp_fod_4[4] = {0x00, P9221_REG_FOD_4, EPPFOD_4_A, EPPFOD_4_B};
    unsigned char epp_fod_5[4] = {0x00, P9221_REG_FOD_5, EPPFOD_5_A, EPPFOD_5_B};

    pr_debug("wlc: %s Enter\n", __func__);

    /* Update FOD value into wireless charger chipset */
    if (chip->power_profile == WLC_EPP) {
        pr_info("wlc: update FOD EPP\n");
        p9221_write_reg(chip, epp_fod_0, 4);
        p9221_write_reg(chip, epp_fod_1, 4);
        p9221_write_reg(chip, epp_fod_2, 4);
        p9221_write_reg(chip, epp_fod_3, 4);
        p9221_write_reg(chip, epp_fod_4, 4);
        p9221_write_reg(chip, epp_fod_5, 4);
    } else {
        pr_info("wlc: update FOD BPP\n");
        p9221_write_reg(chip, bpp_fod_0, 4);
        p9221_write_reg(chip, bpp_fod_1, 4);
        p9221_write_reg(chip, bpp_fod_2, 4);
        p9221_write_reg(chip, bpp_fod_3, 4);
        p9221_write_reg(chip, bpp_fod_4, 4);
        p9221_write_reg(chip, bpp_fod_5, 4);
    }

    /* The wlc_fod_wakelock to wake unlock system.
       Finish the update FOD value process. */
    if (wake_lock_active(&chip->wlc_fod_wakelock)) {
        pr_info("wlc: fod wake unlock\n");
        wake_unlock(&chip->wlc_fod_wakelock);
    }

    pr_debug("wlc: %s End\n", __func__);
}

void p9221_wlc_start_current_limit(struct p9221_wlc_chip *chip)
{
    schedule_delayed_work(&chip->wlc_current_limit_work, msecs_to_jiffies(WLC_CURRENT_LIMIT_WORK_DELAY));
}

void p9221_wlc_stop_current_limit(struct p9221_wlc_chip *chip)
{
    cancel_delayed_work(&chip->wlc_current_limit_work);
}

static void p9221_wlc_current_limit_work(struct work_struct *work)
{
    struct p9221_wlc_chip *chip = container_of(work,
                struct p9221_wlc_chip,
                wlc_current_limit_work.work);
    int current_max = 0;

    pr_debug("wlc: %s Enter\n", __func__);

    if (chip == NULL) {
        pr_err("wlc: p9221_wlc_current_limit_work chip NULL\n");
        return;
    }

    if (chip->screenon_epp_cur_limit == -1 ||
            chip->screenoff_epp_cur_limit == -1 ||
            chip->bpp_cur_limit == -1) {
        pr_err("wlc: screenon_epp_cur_limit = %d, screenoff_epp_cur_limit = %d, bpp_cur_limit = %d Fail value\n",
            chip->screenon_epp_cur_limit, chip->screenoff_epp_cur_limit,
            chip->bpp_cur_limit);
        goto out;
    }

    /* Get dc current max value */
    current_max = p9221_get_dc_current_max();
    if (current_max == -1) {
        pr_err("wlc: %s Fail to get dc current max\n", __func__);
        return;
    }

    pr_info("wlc: l: %d, p: %s, s %s\n",
        current_max,
        (chip->power_profile == WLC_EPP) ? "WLC_EPP" : (chip->power_profile == WLC_BPP ? "WLC_BPP" : "WLC_NONE"),
        (chip->wlc_lcm_screen == LCM_SCREEN_ON) ? "on" : (chip->wlc_lcm_screen == LCM_SCREEN_OFF ? "off" : "none"));

    /* New currnet limiti max update */
    if ((chip->power_profile == WLC_EPP) &&
            (chip->wlc_lcm_screen == LCM_SCREEN_ON) &&
            (current_max != chip->screenon_epp_cur_limit)) {

        pr_info("wlc: Set dc current %d\n", chip->screenon_epp_cur_limit);
        p9221_set_dc_current_max(chip->screenon_epp_cur_limit);
    } else if ((chip->power_profile == WLC_EPP) &&
            (chip->wlc_lcm_screen == LCM_SCREEN_OFF) &&
            (current_max != chip->screenoff_epp_cur_limit)) {

        pr_info("wlc: Set dc current %d\n", chip->screenoff_epp_cur_limit);
        p9221_set_dc_current_max(chip->screenoff_epp_cur_limit);
    } else if ((chip->power_profile == WLC_BPP) &&
            (current_max != chip->bpp_cur_limit)) {

        pr_info("wlc: Set dc current %d\n", chip->bpp_cur_limit);
        p9221_set_dc_current_max(chip->bpp_cur_limit);
    } else {
        pr_debug("wlc: No need set dc current\n");
    }

    /* Keep to monitor it. If need to change the current limit. */
    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true)) {
        schedule_delayed_work(&chip->wlc_current_limit_work, msecs_to_jiffies(WLC_CURRENT_LIMIT_RUNTIME_WORK_DELAY));
    }

out:
    /* The wlc_icl_wakelock to wake unlock system.
       Finish the current limit process. */
    if (wake_lock_active(&chip->wlc_icl_wakelock)) {
        pr_info("wlc: lcm event icl wake unlock\n");
        wake_unlock(&chip->wlc_icl_wakelock);
    }

    pr_debug("wlc: %s End\n", __func__);
}

static void p9221_wlc_poweroffcharging_current_limit_work(struct work_struct *work)
{
    struct p9221_wlc_chip *chip = container_of(work,
                struct p9221_wlc_chip,
                wlc_poweroff_current_limit_work.work);
    int current_max = 0;

    pr_debug("wlc: %s Enter\n", __func__);

    if (chip == NULL) {
        pr_err("wlc: wlc_poweroff_current_limit_work chip NULL\n");
        return;
    }

    if (chip->poweroff_epp_cur_limit == -1 ||
            chip->poweroff_bpp_cur_limit == -1) {
        pr_err("wlc: poweroff_epp_cur_limit = %d, poweroff_bpp_cur_limit = %d Fail value\n",
            chip->poweroff_epp_cur_limit, chip->poweroff_bpp_cur_limit);
        goto out;
    }

    /* Get dc current max value */
    current_max = p9221_get_dc_current_max();
    if (current_max == -1) {
        pr_err("wlc: %s Fail to get dc current max\n", __func__);
        return;
    }

    pr_info("wlc: l: %d, p: %s, s %s\n",
        current_max,
        (chip->power_profile == WLC_EPP) ? "WLC_EPP" : (chip->power_profile == WLC_BPP ? "WLC_BPP" : "WLC_NONE"),
        (chip->wlc_lcm_screen == LCM_SCREEN_ON) ? "on" : (chip->wlc_lcm_screen == LCM_SCREEN_OFF ? "off" : "none"));

    /* New currnet limiti max update */
    if ((chip->power_profile == WLC_EPP) &&
            (current_max != chip->poweroff_epp_cur_limit)) {

        pr_info("wlc: Set dc current %d\n", chip->poweroff_epp_cur_limit);
        p9221_set_dc_current_max(chip->poweroff_epp_cur_limit);
    } else if ((chip->power_profile == WLC_BPP) &&
            (current_max != chip->poweroff_bpp_cur_limit)) {

        pr_info("wlc: Set dc current %d\n", chip->poweroff_bpp_cur_limit);
        p9221_set_dc_current_max(chip->poweroff_bpp_cur_limit);
    } else {
        pr_debug("wlc: No need set dc current\n");
    }

    /* Keep to monitor it. If need to change the current limit. */
    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true)) {
        schedule_delayed_work(&chip->wlc_poweroff_current_limit_work, msecs_to_jiffies(WLC_POWEROFF_CURRENT_LIMIT_RUNTIME_WORK_DELAY));
    }

out:
    pr_debug("wlc: %s End\n", __func__);
}

/*
      <- t1 ->     <- t2 ->     <- t3 ->     <- t4 ->     <- t5 ->     <- t6 ->          <- tn ->
       ------       ------       ------       ------       ------       ------            ------
      |      |     |      |     |      |     |      |     |      |     |      |          |      |
      |      |     |      |     |      |     |      |     |      |     |      |   ....   |      |
      |      |     |      |     |      |     |      |     |      |     |      |          |      |
-------       -----        -----        -----        -----        -----        ----------        -----
    if condition A:
        ti <= FAKE_CHARGING_TIME_SEC
            cycle_pluginout_count + 1
        else
            cycle_pluginout_count = 0

    if condition B:
        cycle_pluginout_count >= MAX_CYCLE_PLUGINOUT_COUNT

    condition A and B true: Send end of power transfer packet to TX pad.
*/

static void p9221_detect_cycle_pluginout(struct p9221_wlc_chip *chip, enum wlc_plug_state plug_state)
{
    unsigned long charging_time = 0;

    if (plug_state == WLC_PLUG_IN) {
        /* Get plug in time */
        do_gettimeofday(&chip->plugin_t);

        /* Check if need to send end of power transfer packet */
        if (chip->cycle_pluginout_count >= MAX_CYCLE_PLUGINOUT_COUNT) {
            pr_info("wlc: send EPT packet\n");
            chip->cycle_pluginout_count = 0;

			p9221_end_power_transfer_trigger(chip);
        }
    } else if (plug_state == WLC_PLUG_OUT) {
        /* Get plug out time */
        do_gettimeofday(&chip->plugout_t);

        /* Calculate the charing time */
        charging_time = chip->plugout_t.tv_sec - chip->plugin_t.tv_sec;
        pr_info("wlc: charging_time = %lu\n", charging_time);

        /* Check this charging time is fake charging or not */
        if (charging_time <= FAKE_CHARGING_TIME_SEC) {
            chip->cycle_pluginout_count++;
        } else {
            chip->cycle_pluginout_count = 0;
        }
    }
}

static int p9221_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
    struct power_supply *psy = v;
    struct p9221_wlc_chip *chip = container_of(nb, struct p9221_wlc_chip, nb);

    if (!strcmp(psy->desc->name, "dc")) {

        if (ev == PSY_EVENT_PROP_CHANGED) {
            if (p9221_is_dc_present() == true) {
                pr_info("wlc: wireless charger plugin\n");

                /* Starting detect cycle plug in out status.
                   For plug in status process */
                p9221_detect_cycle_pluginout(chip, WLC_PLUG_IN);

                /* The wlc_fod_wakelock to wake lock system.
                   Should finish the update FOD value process before enter system suspend. */
                pr_info("wlc: psy event fod wake lock\n");
                wake_lock(&chip->wlc_fod_wakelock);

                /* Starting to identify power profile mode is EPP or BPP */
                p9221_wlc_start_power_profile_setup(chip);

                /* Starting update FOD value process when wireless charing plug in. */
                p9221_wlc_start_update_fod(chip);

                /* Starting rx mode monitor process when wireless charing plug in. */
                p9221_wlc_start_rxmode_monitor(chip);

                /* Starting current limit process when wireless charing plug in. */
                if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
                    p9221_wlc_start_current_limit(chip);
                }
            } else {
                pr_info("wlc: wireless charger unplug\n");

                /* Starting detect cycle plug in out status.
                   For plug out status process */
                p9221_detect_cycle_pluginout(chip, WLC_PLUG_OUT);

                /* Stop to identify power profile mode */
                p9221_wlc_stop_power_profile_setup(chip);

                /* Stop update FOD value process when wireless charing plug out. */
                p9221_wlc_stop_update_fod(chip);

                /* Stop rx mode monitor process when wireless charing plug out. */
                p9221_wlc_stop_rxmode_monitor(chip);

                /* Stop current limit process when wireless charing plug out. */
                if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
                    p9221_wlc_stop_current_limit(chip);

                    /* Avoid current limit not work, Set the current back to screen on current limit if is EPP mode. */
                    if (chip->power_profile == WLC_EPP) {
                        pr_info("wlc: unplug Set dc current %d\n", chip->screenon_epp_cur_limit);
                        p9221_set_dc_current_max(chip->screenon_epp_cur_limit);
                    }

                    /* The wlc_icl_wakelock to wake unlock system.
                       Because wireless charging have been plug out. */
                    if (wake_lock_active(&chip->wlc_icl_wakelock)) {
                        pr_info("wlc: lcm event icl wake unlock\n");
                        wake_unlock(&chip->wlc_icl_wakelock);
                    }
                }

                /* The wlc_fod_wakelock to wake unlock system.
                   Because wireless charging have been plug out. */
                if (wake_lock_active(&chip->wlc_fod_wakelock)) {
                    pr_info("wlc: psy event fod wake unlock\n");
                    wake_unlock(&chip->wlc_fod_wakelock);
                }

                chip->power_profile = WLC_NONE;
                chip->power_profile_check_count = 0;
            }
        }
    }

    return NOTIFY_OK;
}

static int P9221_register_notifier(struct p9221_wlc_chip *chip)
{
    int rc;

    chip->nb.notifier_call = p9221_notifier_call;
    rc = power_supply_reg_notifier(&chip->nb);
    if (rc < 0) {
        pr_err("wlc: Couldn't register psy notifier rc = %d\n", rc);
        return rc;
    }

    return 0;
}

static int p9221_lcm_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
    struct fb_event *evdata = v;
    struct p9221_wlc_chip *chip = container_of(nb, struct p9221_wlc_chip, lcm_notifier);
	unsigned int blank;

    if (ev != FB_EVENT_BLANK)
        return 0;

    if (evdata && evdata->data && ev == FB_EVENT_BLANK && chip) {
        blank = *(int *)(evdata->data);
        switch (blank) {
            case FB_BLANK_POWERDOWN:
                chip->wlc_lcm_screen = LCM_SCREEN_OFF;
                pr_info("wlc: wlc_lcm_screen off\n");

                if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
                    /* The wlc_icl_wakelock to wake lock system.
                       Should finish the current limit process before enter system suspend. */
                    if (p9221_is_dc_present() == true && !wake_lock_active(&chip->wlc_icl_wakelock)) {
                        pr_info("wlc: lcm event icl wake lock\n");
                        wake_lock(&chip->wlc_icl_wakelock);
                    }
                }
                break;
            case FB_BLANK_UNBLANK:
                chip->wlc_lcm_screen = LCM_SCREEN_ON;
                pr_info("wlc: wlc_lcm_screen on\n");
                break;
            default:
                pr_debug("wlc: blank %d no need support\n", blank);
                break;
        }
    }

    return NOTIFY_OK;
}

static int P9221_register_lcm_notifier(struct p9221_wlc_chip *chip)
{
    int rc;

    chip->lcm_notifier.notifier_call = p9221_lcm_notifier_call;
    rc = fb_register_client(&chip->lcm_notifier);
    if (rc < 0) {
        pr_err("wlc: Couldn't register fb notifier rc = %d\n", rc);
        return rc;
    }

    return 0;
}

static int p9221_wlc_suspend(struct device *dev)
{
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: suspend i2c client NULL\n");
        return 0;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: suspend i2c client data NULL\n");
        return 0;
    }

    chip->device_suspend = true;
    if (chip->power_profile != WLC_NONE) {
        pr_info("wlc: call p9221_wlc_stop_rxmode_monitor\n");
        p9221_wlc_stop_rxmode_monitor(chip);
    }

    return 0;
}

static int p9221_wlc_resume(struct device *dev)
{
    struct i2c_client *client = NULL;
    struct p9221_wlc_chip *chip = NULL;

    pr_debug("wlc: %s Enter\n", __func__);

    /* Get i2c client handle */
    client = to_i2c_client(dev);
    if (client == NULL) {
        pr_err("wlc: resume i2c client NULL\n");
        return 0;
    }

    /* Get p9221_wlc_chipt handle */
    chip = i2c_get_clientdata(client);
    if (chip == NULL) {
        pr_err("wlc: resume i2c client data NULL\n");
        return 0;
    }

    chip->device_suspend = false;
    if (chip->power_profile != WLC_NONE) {
        pr_info("wlc: call p9221_wlc_start_rxmode_monitor\n");
        p9221_wlc_start_rxmode_monitor_from_resume(chip);
    }
    return 0;
}

static const struct dev_pm_ops p9221_wlc_pm_ops = {
    .suspend = p9221_wlc_suspend,
    .resume = p9221_wlc_resume,
};

static int p9221_wl_request_named_gpio(struct device *dev,
		const char *label, int *gpio)
{
    int rc = 0;
    struct device_node *np = dev->of_node;

    if (dev == NULL) {
        pr_err("wlc: dev is null\n");
        return -EFAULT;
    }

    if (np == NULL) {
        pr_err("wlc: np is null\n");
        return -EFAULT;
    }

    if (gpio == NULL) {
        pr_err("wlc: gpio is null\n");
        return -EFAULT;
    }

    rc = of_get_named_gpio(np, label, 0);
    if (rc < 0) {
        pr_err("wlc: failed to get '%s'\n", label);
        return rc;
    }
    *gpio = rc;

    rc = devm_gpio_request(dev, *gpio, label);
    if (rc) {
        pr_err("wlc: failed to request gpio %d\n", *gpio);
        return rc;
    }

    return rc;
}

static int p9221_wlc_parse_dt(struct p9221_wlc_chip *chip)
{
    int rc = 0;
    struct device *dev = chip->dev;

    pr_info("wlc: %s Enter\n", __func__);

    /* Setup irq pin */
    rc = p9221_wl_request_named_gpio(dev, "wl,irq-gpio", &chip->irq_gpio);
    if (rc) {
        pr_err("wlc: Unable to get irq_gpio \n");
        return rc;
    }

    /* Setup enable pin */
    rc = p9221_wl_request_named_gpio(dev, "wl,enable-gpio", &chip->enable_gpio);
    if (rc) {
        pr_err("wlc: Unable to get enable_gpio \n");
        return rc;
    }

    pr_info("wlc: %s End\n", __func__);
    return 0;
}

static int p9221_wlc_pinctrl_init(struct p9221_wlc_chip *chip)
{
    int rc = 0;
    struct device *dev = chip->dev;

    pr_info("wlc: %s Enter\n", __func__);

    if (dev == NULL) {
        pr_err("wlc: dev is null\n");
        return -EFAULT;
    }

    chip->wl_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR_OR_NULL(chip->wl_pinctrl)) {
        pr_err("wlc: Target does not use pinctrl\n");
        rc = PTR_ERR(chip->wl_pinctrl);
        goto err;
    }


    /* Setup pinctrl state of interrupt */
    chip->wlc_intr_active =
    pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_intr_active");
    if (IS_ERR_OR_NULL(chip->wlc_intr_active)) {
        pr_err("wlc: Cannot get pmx_wlc_intr_active pinstate\n");
        rc = PTR_ERR(chip->wlc_intr_active);
        goto err;
    }


    /* Setup pinctrl state of rx path */
    chip->wlc_rxpath_active =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_rx_path_active");
    if (IS_ERR_OR_NULL(chip->wlc_rxpath_active)) {
        pr_err("wlc: Cannot get pmx_wlc_rx_path_active pinstate\n");
        rc = PTR_ERR(chip->wlc_rxpath_active);
        goto err;
    }

    chip->wlc_rxpath_sleep =
        pinctrl_lookup_state(chip->wl_pinctrl, "pmx_wlc_rx_path_sleep");
    if (IS_ERR_OR_NULL(chip->wlc_rxpath_sleep)) {
        pr_err("wlc: Cannot get pmx_wlc_rx_path_sleep pinstate\n");
        rc = PTR_ERR(chip->wlc_rxpath_sleep);
        goto err;
    }


    pr_info("wlc: %s End\n", __func__);
    return 0;

err:
    chip->wl_pinctrl = NULL;
    chip->wlc_intr_active = NULL;
    chip->wlc_rxpath_active = NULL;
    chip->wlc_rxpath_sleep = NULL;
    return rc;
}

static irqreturn_t p9221_wlc_handler(int irq, void *data)
{
    pr_info("wlc: %s\n", __func__);
    return IRQ_HANDLED;
}

static int p9221_wlc_hw_init(struct p9221_wlc_chip *chip)
{
    int rc = 0;

    pr_info("wlc: %s Enter\n", __func__);

    /* Turn on RX path */
    /* set wireless charger chipset enable */
    if (IS_ERR_OR_NULL(chip->wlc_rxpath_active)) {
        pr_err("wlc: not a valid wlc_rxpath_active pinstate\n");
        return -EINVAL;
    }

    rc = pinctrl_select_state(chip->wl_pinctrl, chip->wlc_rxpath_active);
    if (rc) {
        pr_err("wlc: Cannot set wlc_rxpath_active pinstate\n");
        return rc;
    }

    chip->chipset_enabled = CHIPSET_ENABLE;

    /* Setup irq */
    if (!gpio_is_valid(chip->irq_gpio)) {
        pr_err("wlc: irq_gpio is invalid.\n");
        return -EINVAL;
    }

    rc = gpio_direction_input(chip->irq_gpio);
    if (rc) {
        pr_err("wlc: gpio_direction_input (irq) failed.\n");
        return rc;
    }
    chip->irq_num = gpio_to_irq(chip->irq_gpio);

    rc = request_irq(chip->irq_num, p9221_wlc_handler,
            IRQF_TRIGGER_FALLING,
            "p9221_wireless_charger", chip);
    if (rc < 0) {
        pr_err("wlc: p9221_wireless_charger request irq failed\n");
        return rc;
    }

    disable_irq(chip->irq_num);

    pr_info("wlc: %s End\n", __func__);
    return 0;
}

static int p9221_wlc_hw_deinit(struct p9221_wlc_chip *chip)
{
    struct device *dev = chip->dev;

    pr_info("wlc: %s Enter\n", __func__);

    if (chip->irq_gpio > 0)
        devm_gpio_free(dev, chip->irq_gpio);

    if (chip->enable_gpio > 0)
        devm_gpio_free(dev, chip->enable_gpio);

    free_irq(chip->irq_num, chip);

    pr_info("wlc: %s End\n", __func__);
    return 0;
}

static int p9221_wlc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct p9221_wlc_chip *chip;
    struct power_supply_config wireless_psy_cfg = {};
    int rc = 0;

    pr_info("wlc: %s Enter\n", __func__);

    if (client->dev.of_node == NULL) {
        pr_err("wlc: client->dev.of_node null\n");
        rc = -EFAULT;
        goto err;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("wlc: i2c_check_functionality error\n");
        rc = -EIO;
        goto err;
    }

    chip = devm_kzalloc(&client->dev,
        sizeof(struct p9221_wlc_chip), GFP_KERNEL);
    if (!chip) {
        pr_err("wlc: Failed to allocate memory\n");
        rc = -ENOMEM;
        goto err;
    }

    chip->client = client;
    chip->dev = &client->dev;

    rc = p9221_wlc_parse_dt(chip);
    if (rc) {
        pr_err("wlc: DT parsing failed\n");
        goto free_alloc_mem;
    }

    rc = p9221_wlc_pinctrl_init(chip);
    if (rc) {
        pr_err("wlc: pinctrl init failed\n");
        goto free_alloc_mem;
    }

    rc = p9221_wlc_hw_init(chip);
    if (rc) {
        pr_err("wlc: couldn't init hardware rc = %d\n", rc);
        goto free_alloc_mem;
    }

    chip->device_suspend = false;
    chip->chipset_enabled = CHIPSET_UNINIT;
    chip->i2c_failed = false;
    chip->power_profile = WLC_NONE;
    chip->power_profile_check_count = 0;
    chip->wlc_lcm_screen = LCM_SCREEN_NONE;

    /* For current limit */
    chip->screenon_cur_limit_enable = of_property_read_bool(chip->dev->of_node,
            "wl,screenon-current-limit");
    chip->screenon_epp_cur_limit = -1;
    chip->screenoff_epp_cur_limit = -1;
    chip->bpp_cur_limit = -1;

    /* For poweroff charging current limit */
    chip->poweroffcharging_cur_limit_enable = of_property_read_bool(chip->dev->of_node,
            "wl,poweroffcharging-current-limit");
    chip->poweroff_epp_cur_limit = -1;
    chip->poweroff_bpp_cur_limit = -1;

    chip->plugin_t = (struct timeval){0, 0};
    chip->plugout_t = (struct timeval){0, 0};
    chip->cycle_pluginout_count = 0;

    INIT_DELAYED_WORK(&chip->wlc_rxmode_monitor_work, p9221_wlc_rxmode_monitor_work);
    INIT_DELAYED_WORK(&chip->wlc_power_profile_setup_work, p9221_wlc_power_profile_setup_work);
    INIT_DELAYED_WORK(&chip->wlc_udpate_fod_work, p9221_wlc_update_fod_work);
    INIT_DELAYED_WORK(&chip->wlc_current_limit_work, p9221_wlc_current_limit_work);
    INIT_DELAYED_WORK(&chip->wlc_poweroff_current_limit_work, p9221_wlc_poweroffcharging_current_limit_work);
    mutex_init(&chip->lock);

    /* init wake lock */
    wake_lock_init(&chip->wlc_fod_wakelock, WAKE_LOCK_SUSPEND, "wlc_fod_wakelock");
    wake_lock_init(&chip->wlc_icl_wakelock, WAKE_LOCK_SUSPEND, "wlc_icl_wakelock");

    /* register power supply notifier */
    rc = P9221_register_notifier(chip);
    if (rc) {
        pr_err("wlc: couldn't register notifier rc = %d\n", rc);
        goto fail_reg_notifier;
    }

    /* Enable to limit the dc-in current max when screen on */
    if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
        /* screenon_epp_cur_limit read, screen on EPP mode current max limitation */
        if (of_property_read_u32(chip->dev->of_node, "wl,screenon-epp-current",
                &chip->screenon_epp_cur_limit)) {
            pr_err("wlc: Fail get wl,screenon-epp-current\n");
        }

        /* screenoff_epp_cur_limit read, screen off EPP mode current max limitation */
        if (of_property_read_u32(chip->dev->of_node, "wl,screenoff-epp-current",
                &chip->screenoff_epp_cur_limit)) {
            pr_err("wlc: Fail get wl,screenoff-epp-current\n");
        }

        /* bpp_cur_limit read, BPP mode current max limitation */
        if (of_property_read_u32(chip->dev->of_node, "wl,bpp-current",
                &chip->bpp_cur_limit)) {
            pr_err("wlc: Fail get wl,bpp-current\n");
        }

        pr_info("wlc: screenon_cur_limit_enable = %d\n", chip->screenon_cur_limit_enable);
        pr_info("wlc: screenon_epp_cur_limit = %d\n", chip->screenon_epp_cur_limit);
        pr_info("wlc: screenoff_epp_cur_limit = %d\n", chip->screenoff_epp_cur_limit);
        pr_info("wlc: bpp_cur_limit = %d\n", chip->bpp_cur_limit);

        /* register fb notifier */
        rc = P9221_register_lcm_notifier(chip);
        if (rc) {
            pr_err("wlc: couldn't register lcm notifier rc = %d\n", rc);
            goto fail_reg_lcm_notifier;
        }
    }

    /* Enable to limit the dc-in current max when power off charging */
    if (chip->poweroffcharging_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") != NULL)) {
        /* poweroff_epp_cur_limit read, EPP mode current max limitation */
        if (of_property_read_u32(chip->dev->of_node, "wl,poweroff-epp-current",
                &chip->poweroff_epp_cur_limit)) {
            pr_err("wlc: Fail get wl,poweroff-epp-current\n");
        }

        /* poweroff_bpp_cur_limit read, BPP mode current max limitation */
        if (of_property_read_u32(chip->dev->of_node, "wl,poweroff-bpp-current",
                &chip->poweroff_bpp_cur_limit)) {
            pr_err("wlc: Fail get wl,poweroff-bpp-current\n");
        }

        pr_info("wlc: poweroffcharging_cur_limit_enable = %d\n", chip->poweroffcharging_cur_limit_enable);
        pr_info("wlc: poweroff_epp_cur_limit = %d\n", chip->poweroff_epp_cur_limit);
        pr_info("wlc: poweroff_bpp_cur_limit = %d\n", chip->poweroff_bpp_cur_limit);
    }


    i2c_set_clientdata(client, chip);
    pr_info("wlc: i2c address: %x\n", client->addr);

    /* PowerSupply */
    chip->wireless_psy_d.name = "wireless";
    chip->wireless_psy_d.type = POWER_SUPPLY_TYPE_WIRELESS;
    chip->wireless_psy_d.properties = p9221_power_props_wireless;
    chip->wireless_psy_d.num_properties = ARRAY_SIZE(p9221_power_props_wireless);
    chip->wireless_psy_d.get_property = p9221_power_get_property_wireless;

    wireless_psy_cfg.drv_data = chip;
    wireless_psy_cfg.supplied_to = p9221_power_supplied_to;
    wireless_psy_cfg.num_supplicants = ARRAY_SIZE(p9221_power_supplied_to);
    wireless_psy_cfg.of_node = chip->dev->of_node;

    chip->wireless_psy = devm_power_supply_register(chip->dev,
        &chip->wireless_psy_d, &wireless_psy_cfg);
    if (IS_ERR(chip->wireless_psy)) {
        pr_err("wlc: Unable to register wireless_psy rc = %ld\n", PTR_ERR(chip->wireless_psy));
        rc = PTR_ERR(chip->wireless_psy);
        goto fail_reg_power_supply;
    }

	/* Register the sysfs nodes */
	if ((rc = sysfs_create_group(&chip->dev->kobj,
            &p9221_attrs_group))) {
        pr_err("wlc: Unable to create sysfs group rc = %d\n", rc);
        goto fail_sysfs_create;
	}

    fih_chip = chip;

    /* Force trigger I2C core active to config I2C bus pinctrl */
    p9221_chipid_force_read(chip);

    /* Starting the rx mode monitor process, update FOD value process and current limit process.
       If wireless charging have been attached at booting stage. */
    if ((p9221_is_dc_present() == true) && (p9221_is_dc_online() == true)) {
        pr_info("wlc: dc present and online at booting up\n");

        /* Starting to identify power profile mode is EPP or BPP */
        p9221_wlc_start_power_profile_setup(chip);

        /* Starting update FOD value process */
        p9221_wlc_start_update_fod(chip);

        /* Starting rx mode monitor process */
        p9221_wlc_start_rxmode_monitor(chip);

        /* Starting current limit process */
        if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
            schedule_delayed_work(&chip->wlc_current_limit_work, msecs_to_jiffies(WLC_CURRENT_LIMIT_BOOTUP_WORK_DELAY));
        }

        /* Enable to limit the dc-in current max when screen on */
        if (chip->poweroffcharging_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") != NULL)) {
            schedule_delayed_work(&chip->wlc_poweroff_current_limit_work, msecs_to_jiffies(WLC_POWEROFF_CURRENT_LIMIT_BOOTUP_WORK_DELAY));
        }
    }

    pr_info("wlc: %s End\n", __func__);
    return 0;

fail_sysfs_create:
    power_supply_unregister(chip->wireless_psy);

fail_reg_power_supply:
    if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
        fb_unregister_client(&chip->lcm_notifier);
    }
    i2c_set_clientdata(client, NULL);

fail_reg_lcm_notifier:
    power_supply_unreg_notifier(&chip->nb);

fail_reg_notifier:
    wake_lock_destroy(&chip->wlc_fod_wakelock);
    wake_lock_destroy(&chip->wlc_icl_wakelock);

free_alloc_mem:
    p9221_wlc_hw_deinit(chip);

    devm_kfree(&client->dev, chip);
    fih_chip = NULL;
err:
    return rc;
}

static int p9221_wlc_remove(struct i2c_client *client)
{
    struct p9221_wlc_chip *chip = i2c_get_clientdata(client);

    pr_info("wlc: %s Enter\n", __func__);

    /* cancel all of work */
    cancel_delayed_work_sync(&chip->wlc_rxmode_monitor_work);
    cancel_delayed_work_sync(&chip->wlc_power_profile_setup_work);
    cancel_delayed_work_sync(&chip->wlc_udpate_fod_work);
    cancel_delayed_work_sync(&chip->wlc_current_limit_work);

    /* destroy wake lock */
    wake_lock_destroy(&chip->wlc_fod_wakelock);
    wake_lock_destroy(&chip->wlc_icl_wakelock);

    sysfs_remove_group(&chip->dev->kobj, &p9221_attrs_group);
    power_supply_unregister(chip->wireless_psy);

    /* unregister power supply notifier */
    power_supply_unreg_notifier(&chip->nb);

    /* register fb notifier */
    if (chip->screenon_cur_limit_enable && (strstr(saved_command_line,"androidboot.mode=charger") == NULL)) {
        fb_unregister_client(&chip->lcm_notifier);
    }

    p9221_wlc_hw_deinit(chip);

    devm_kfree(&client->dev, chip);
    fih_chip = NULL;

    pr_info("wlc: %s End\n", __func__);
    return 0;
}

 static struct i2c_driver p9221_wlc_driver = {
    .probe = p9221_wlc_probe,
    .remove = p9221_wlc_remove,
    .driver = {
    .name = P9221_WLC_DEV_NAME,
    .owner = THIS_MODULE,
    .of_match_table = p9221_wlc_match_table,
#ifdef CONFIG_PM
    .pm = &p9221_wlc_pm_ops,
#endif
    },
    .id_table = p9221_wlc_id,
};

 static int __init p9221_wlc_init(void)
{
    pr_info("wlc: %s Enter\n", __func__);

    return i2c_add_driver(&p9221_wlc_driver);
}

static void __exit p9221_wlc_exit(void)
{
    pr_info("wlc: %s Enter\n", __func__);

    i2c_del_driver(&p9221_wlc_driver);
}

late_initcall(p9221_wlc_init);
module_exit(p9221_wlc_exit);

MODULE_AUTHOR("AnnYCWang <annycwang@fih-foxconn.com>");
MODULE_DESCRIPTION("IDT P9221 Wireless Charger Driver");
MODULE_LICENSE("GPL v2");
