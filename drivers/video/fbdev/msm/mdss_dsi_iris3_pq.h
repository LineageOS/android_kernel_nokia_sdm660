#ifndef MDSS_DSI_IRIS_PQ
#define MDSS_DSI_IRIS_PQ

#include "mdss_dsi_iris3_lightup.h"

enum {
	IRIS_LCE_GRAPHIC = 0x00,
	IRIS_LCE_VIDEO,
};

enum {
	IRIS_COLOR_TEMP_OFF = 0x00,
	IRIS_COLOR_TEMP_MANUL,
	IRIS_COLOR_TEMP_AUTO,
};

enum {
	IRIS_MAGENTA_GAIN_TYPE = 0,
	IRIS_RED_GAIN_TYPE,
	IRIS_YELLOW_GAIN_TYPE,
	IRIS_GREEN_GAIN_TYPE,
	IRIS_BLUE_GAIN_TYPE,
	IRIS_CYAN_GAIN_TYPE,
};

#define IP_OPT_MAX				20

void iris_set_skip_dma(bool skip);

void iris_pq_parameter_init(void);

void iris_peaking_level_set(u32 level);

void iris_cm_6axis_level_set(u32 level);

void iris_cm_ftc_enable_set(bool enable);

void iris_cm_colortemp_mode_set(u32 mode);

void iris_cm_color_temp_set(void);

void iris_cm_color_gamut_pre_set(u32 source_switch);

void iris_cm_color_gamut_set(u32 level);

void iris_dpp_gamma_set(void);

void iris_lce_mode_set(u32 mode);

void iris_lce_level_set(u32 level);

void iris_lce_graphic_det_set(bool enable);

void iris_lce_al_set(bool enable);

void iris_dbc_level_set(u32 level);

void iris_pwm_freq_set(u32 value);

void iris_pwm_enable_set(bool enable);

void iris_dbc_bl_user_set(u32 value);

void iris_dbc_led0d_gain_set(u32 value);

void iris_demo_mode_set(u32 mode);

void iris_reading_mode_set(u32 level);

void iris_lce_lux_set(void);
void iris_ambient_light_lut_set(void);

void iris_maxcll_lut_set(void);

void iris_dbclce_datapath_set(bool bEn);

void iris_dbclce_power_set(bool bEn);

void iris_dbc_compenk_set(u8 lut_table_index);
void iris_sdr2hdr_level_set(u32 level);
void iris_set_sdr2hdr_mode(u8 mode);
u8 iris_get_sdr2hdr_mode(void);
void iris_panel_nits_set(u32 bl_ratio, bool bSystemRestore, int level);
void iris_scaler_filter_update(u32 level);

void iris_peaking_idle_clk_enable(bool enable);

void iris_cm_6axis_seperate_gain(u8 gain_type, u32 value);

void iris_init_ipopt_ip(struct iris_update_ipopt *ipopt,  int len);
void iris_hdr_csc_prepare(void);
void iris_hdr_csc_complete(int step);
void iris_hdr_csc_frame_ready(void);

int32_t  iris_update_ip_opt(
		struct iris_update_ipopt *popt, int len, uint8_t ip,
		uint8_t opt_id, uint8_t skip_last);
#endif
