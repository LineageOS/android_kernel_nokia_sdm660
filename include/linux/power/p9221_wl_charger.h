#ifndef P9221_WL_CHARGER_H
#define P9221_WL_CHARGER_H

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>

/* register address of chipset */
#define P9221_REG_LEN 2

#define P9221_REG_CHIPID 0x0

#define P9221_REG_FW_MAJVER 0x4

#define P9221_REG_FW_MINVER 0x6

#define P9221_REG_CHIPSTATUS 0x34

#define P9221_REG_RX_BAT_CHARG_STATUS 0x3A

#define P9221_REG_RX_END_POWER_TRANSFER 0x3B

#define P9221_REG_RX_VOUT 0x3C

#define P9221_REG_RX_VRECT 0x40

#define P9221_REG_RX_IOUT 0x44

#define P9221_REG_RX_TEMP 0x46

#define P9221_REG_RX_OPFREQ 0x48

#define P9221_REG_RX_COMMMAND 0x4E

#define P9221_REG_FOD_0 0x68

#define P9221_REG_FOD_1 0x6A

#define P9221_REG_FOD_2 0x6C

#define P9221_REG_FOD_3 0x6E

#define P9221_REG_FOD_4 0x70

#define P9221_REG_FOD_5 0x72

/*
    Wireless charger FOD (Foreign Object Detection) configuration value
    BPP(Baseline Power Profile): (5V/1A)
    EPP(Extended Power Profile): (9V/1A)
    Reg.    Reg field name      BPP FOD     EPP FOD
    0x68        FOD_0_A         0xA6        0xA8
    0x69        FOD_0_B         0x25        0x30
    0x6A        FOD_1_A         0x88        0x8B
    0x6B        FOD_1_B         0x30        0x50
    0x6C        FOD_2_A         0x8D        0x88
    0x6D        FOD_2_B         0x28        0x54
    0x6E        FOD_3_A         0x9A        0x94
    0x6F        FOD_3_B         0x0D        0x28
    0x70        FOD_4_A         0xA1        0x95
    0x71        FOD_4_B         0xF8        0x27
    0x72        FOD_5_A         0xAA        0x9E
    0x73        FOD_5_B         0xDA        0xF8
*/

#define BPPFOD_0_A 0xA6
#define BPPFOD_0_B 0x25
#define BPPFOD_1_A 0x88
#define BPPFOD_1_B 0x30
#define BPPFOD_2_A 0x8D
#define BPPFOD_2_B 0x28
#define BPPFOD_3_A 0x9A
#define BPPFOD_3_B 0x0D
#define BPPFOD_4_A 0xA1
#define BPPFOD_4_B 0xF8
#define BPPFOD_5_A 0xAA
#define BPPFOD_5_B 0xDA

#define EPPFOD_0_A 0xA8
#define EPPFOD_0_B 0x28
#define EPPFOD_1_A 0x8B
#define EPPFOD_1_B 0x48
#define EPPFOD_2_A 0x88
#define EPPFOD_2_B 0x4C
#define EPPFOD_3_A 0x94
#define EPPFOD_3_B 0x20
#define EPPFOD_4_A 0x95
#define EPPFOD_4_B 0x1F
#define EPPFOD_5_A 0x9E
#define EPPFOD_5_B 0xF0


/* Status of chipset */
#define CHIPSET_UNINIT -1
#define CHIPSET_DISABLE 0
#define CHIPSET_ENABLE 1

/* Wireless charger Tx mode */
#define WL_TX_OFF 0
#define WL_TX_ON 1

/* PM660 Register SCHG_DC_WIPWR_RANGE_STATUS bit maps */
#define DCIN_GT_8V              BIT(3)
#define DCIN_BETWEEN_6P5V_8V    BIT(2)
#define DCIN_BETWEEN_5P5V_6P5V  BIT(1)
#define DCIN_LT_5P5V_BIT        BIT(0)

/* P9221_REG_RX_COMMMAND bit maps */
#define REG_RX_COMMMAND_CLEAR_INTERRUPT   BIT(5)
#define REG_RX_COMMMAND_SEND_END_OF_POWER BIT(3)
#define REG_RX_COMMMAND_TOGGLE_LDO        BIT(1)


enum wlc_power_profile {
    WLC_NONE = 0,
	WLC_BPP,        /* BPP(Baseline Power Profile): (5V/1A) */
	WLC_EPP,        /* EPP(Extended Power Profile): (9V/1A) */
};

enum wlc_lcm_screen_state {
    LCM_SCREEN_NONE = 0,
    LCM_SCREEN_ON,
    LCM_SCREEN_OFF,
};

enum wlc_plug_state {
    WLC_PLUG_IN = 0,
    WLC_PLUG_OUT,
};

struct p9221_wlc_chip {
    struct i2c_client *client;
    struct device *dev;

    /* locks */
    struct mutex lock;

    /* suspend wake lock */
    struct wake_lock wlc_fod_wakelock;
    struct wake_lock wlc_icl_wakelock;

    /* power supplies */
    struct power_supply_desc wireless_psy_d;
    struct power_supply *wireless_psy;

    /* notifiers */
    struct notifier_block	nb;
    /* notifiers for lcm screen state */
    struct notifier_block lcm_notifier;

    /* work */
    struct delayed_work wlc_rxmode_monitor_work;
    struct delayed_work wlc_power_profile_setup_work;
    struct delayed_work wlc_udpate_fod_work;
    struct delayed_work wlc_current_limit_work;
    struct delayed_work wlc_poweroff_current_limit_work;

    /* gpio */
    int irq_gpio;
    int enable_gpio;

    /* irq */
    int irq_num;

    /* pin control */
    struct pinctrl *wl_pinctrl;
    struct pinctrl_state *wlc_intr_active;
    struct pinctrl_state *wlc_rxpath_active;
    struct pinctrl_state *wlc_rxpath_sleep;

    bool device_suspend;

    int chipset_enabled;
    enum wlc_power_profile power_profile;
    int power_profile_check_count;
    int i2c_failed;

    enum wlc_lcm_screen_state wlc_lcm_screen;
    bool screenon_cur_limit_enable;
    int screenon_epp_cur_limit;
    int screenoff_epp_cur_limit;
    int bpp_cur_limit;

    bool poweroffcharging_cur_limit_enable;
    int poweroff_epp_cur_limit;
    int poweroff_bpp_cur_limit;

    /* Detect cycle plug-in-out */
    struct timeval plugin_t;
    struct timeval plugout_t;
    int cycle_pluginout_count;
};

#endif /* P9221_WL_CHARGER_H */
