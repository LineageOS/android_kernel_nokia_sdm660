#ifndef STWLC33_WL_CHARGER_H
#define STWLC33_WL_CHARGER_H

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>

/* STWLC33 register address */
#define STWLC33_REG_LEN                         2
#define STWLC33_REG_CHIP_ID                     0x0
#define STWLC33_REG_CHIP_REV                    0x2
#define STWLC33_REG_FW_MAJVER                   0x4
#define STWLC33_REG_FW_MINVER                   0x6
#define STWLC33_REG_RX_STATUS                   0x34
#define STWLC33_REG_RX_INT                      0x36
#define STWLC33_REG_RX_INT_ENABLE               0x38
#define STWLC33_REG_EOP                         0x3B
#define STWLC33_REG_RX_VOUT_SET                 0x3E
#define STWLC33_REG_RX_IOUT                     0x44
#define STWLC33_REG_RX_DIE_TEMP                 0x46
#define STWLC33_REG_RX_ILIM_SET                 0x4A
#define STWLC33_REG_RX_SYS_OP_MODE              0x4D
#define STWLC33_REG_RX_COM                      0x4E
#define STWLC33_REG_FW_SWITCH_KEY               0x4F
#define STWLC33_REG_RX_INT_CLEAR                0x56
#define STWLC33_REG_RX_ID_0                     0x5C
#define STWLC33_REG_RX_ID_1                     0x5D
#define STWLC33_REG_RX_ID_2                     0x5E
#define STWLC33_REG_RX_ID_3                     0x5F
#define STWLC33_REG_RX_ID_4                     0x60
#define STWLC33_REG_RX_ID_5                     0x61
#define STWLC33_REG_RX_OVP_SET                  0x62
#define STWLC33_REG_RX_VRECT                    0x64
#define STWLC33_REG_RX_VOUT                     0x66
#define STWLC33_REG_RX_PMA_ADV                  0x78
#define STWLC33_REG_RX_OP_FREQ                  0xFA
#define STWLC33_REG_RX_PING_FREQ                0xFC
#define STWLC33_REG_RX_PRMC_ID                  0x80

/* QI */
#define STWLC33_REG_RX_QI_DATA_SEND_CTL         0x87
#define STWLC33_REG_RX_QI_DATA_RCVD_STATUS      0xD0
#define STWLC33_REG_RX_QI_DATA_RCVD_LENGTH      0xD1
#define STWLC33_REG_RX_QI_DATA_RCVD_FORMAT      0xD2
#define STWLC33_REG_RX_QI_DATA_RCVD_BASE        0xD3
#define STWLC33_REG_RX_QI_DATA_RCVD_LEN         5

/* NVM */
#define STWLC33_REG_NVM_CTL                     0x8F
#define STWLC33_REG_NVM_BASE                    0x90
#define STWLC33_REG_NVM_LEN                     0x20


/* Status of chipset */
#define CHIPSET_UNINIT                          -1
#define CHIPSET_DISABLE                         0
#define CHIPSET_ENABLE                          1

/* Status of chipset match */
#define CHIP_NOT_CONFIRM                        -1
#define CHIP_CONFIRM_DISMATCH                   0
#define CHIP_CONFIRM_MATCH                      1
#define STWLC33_CHIP_ID                         0x21

struct stwlc33_wlc_chip {
    struct i2c_client *client;
    struct device *dev;

    /* locks */
    struct mutex lock;
    struct mutex nvm_lock;

    /* power supplies */
    struct power_supply_desc wireless_psy_d;
    struct power_supply *wireless_psy;
    struct power_supply	*dc_psy;

    /* notifiers */
    struct notifier_block nb;

    /* work */
    struct work_struct	wlc_interrupt_work;
    struct delayed_work wlc_rxmode_monitor_work;

    /* gpio */
    int irq_gpio;

    /* irq */
    int irq_num;

    int chipset_enabled;
    int i2c_failed;
    int chip_match;
};

#endif /* STWLC33_WL_CHARGER_H */
