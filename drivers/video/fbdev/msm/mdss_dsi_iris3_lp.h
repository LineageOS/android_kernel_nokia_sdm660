#ifndef MDSS_DSI_IRIS_LP
#define MDSS_DSI_IRIS_LP

#define ONE_WIRED_CMD_VIA_WAKEUP_GPIO
#define POR_CLOCK 180	/* 0.1 Mhz*/

enum iris_onewired_cmd {
	POWER_UP_SYS = 1,
	ENTER_ANALOG_BYPASS = 2,
	EXIT_ANALOG_BYPASS = 3,
	POWER_DOWN_SYS = 4,
	RESET_SYS = 5,
	FORCE_ENTER_ANALOG_BYPASS = 6,
	FORCE_EXIT_ANALOG_BYPASS = 7,
	POWER_UP_MIPI = 8,
};

enum iris_abypass_status {
	PASS_THROUGH_MODE = 0,
	ANALOG_BYPASS_MODE,
};

/* dynamic power gating set */
void iris_dynamic_power_set(bool enable);

/* dynamic power gating get */
bool iris_dynamic_power_get(void);

/* TE delay or EVS delay select. */
void iris_te_select(int sel);

/* power on/off core domain via PMU */
void iris_pmu_core_set(bool enable);

/* power on/off lce domain via PMU */
void iris_pmu_lce_set(bool enable);

/* lce dynamic pmu mask enable */
void iris_lce_dynamic_pmu_mask_set(bool enable);

/* send one wired commands via GPIO */
void iris_one_wired_cmd_send(struct mdss_dsi_ctrl_pdata *ctrl, int cmd);

int iris_one_wired_cmd_init(struct mdss_dsi_ctrl_pdata *ctrl);

void iris_abypass_switch_proc(struct msm_fb_data_type *mfd, int mode);

void iris_lce_power_status_set(bool enable);

bool iris_lce_power_status_get(void);

/* trigger DMA to load */
void iris_dma_trigger_load(void);

#endif /* MDSS_DSI_IRIS_LP */
