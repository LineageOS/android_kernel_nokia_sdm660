#include <linux/gcd.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/msm_mdp.h>
#include <linux/gpio.h>
#include <linux/circ_buf.h>
#include <linux/gcd.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"
#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_dsi.h"
#include "mdss_dsi_iris3_pq.h"
#include "mdss_dsi_iris3_lightup.h"
#include "mdss_dsi_iris3_def.h"
#include "mdss_dsi_iris3.h"
#include "mdss_dsi_iris3_lp.h"
#include "mdss_dsi_iris3_lightup_ocp.h"

static bool iris_require_yuv_input;
static bool iris_HDR10;
static bool iris_HDR10_YCoCg;
static int iris_HDRchange_cnt;
static bool shadow_iris_require_yuv_input;
static bool shadow_iris_HDR10;
static bool shadow_iris_HDR10_YCoCg;
static bool iris_yuv_datapath;
static bool iris_capture_ctrl_en;
static u8 iris_dbc_lut_index;
static u8 iris_sdr2hdr_mode;
static bool iris_debug_cap;
static struct iris_setting_info iris_setting;
static bool iris_skip_dma;
u32 iris_min_color_temp;
u32 iris_max_color_temp;
u32 iris_min_x_value;
u32 iris_max_x_value;

#define IRIS_CCT_MIN_VALUE		2500
#define IRIS_CCT_MAX_VALUE		7500
#define IRIS_CCT_STEP			25
#define IRIS_X_6500K			3128
#define IRIS_X_7500K			2991
#define IRIS_X_2500K			4637
#define IRIS_LAST_BIT_CTRL	1

/*range is 2500~10000*/
static u32 iris_color_x_buf[]= {
	4637,4626,4615,4603,
	4591,4578,4565,4552,
	4538,4524,4510,4496,
	4481,4467,4452,4437,
	4422,4407,4392,4377,
	4362,4347,4332,4317,
	4302,4287,4272,4257,
	4243,4228,4213,4199,
	4184,4170,4156,4141,
	4127,4113,4099,4086,
	4072,4058,4045,4032,
	4018,4005,3992,3980,
	3967,3954,3942,3929,
	3917,3905,3893,3881,
	3869,3858,3846,3835,
	3823,3812,3801,3790,
	3779,3769,3758,3748,
	3737,3727,3717,3707,
	3697,3687,3677,3668,
	3658,3649,3639,3630,
	3621,3612,3603,3594,
	3585,3577,3568,3560,
	3551,3543,3535,3527,
	3519,3511,3503,3495,
	3487,3480,3472,3465,
	3457,3450,3443,3436,
	3429,3422,3415,3408,
	3401,3394,3388,3381,
	3375,3368,3362,3356,
	3349,3343,3337,3331,
	3325,3319,3313,3307,
	3302,3296,3290,3285,
	3279,3274,3268,3263,
	3258,3252,3247,3242,
	3237,3232,3227,3222,
	3217,3212,3207,3202,
	3198,3193,3188,3184,
	3179,3175,3170,3166,
	3161,3157,3153,3149,
	3144,3140,3136,3132,
	3128,3124,3120,3116,
	3112,3108,3104,3100,
	3097,3093,3089,3085,
	3082,3078,3074,3071,
	3067,3064,3060,3057,
	3054,3050,3047,3043,
	3040,3037,3034,3030,
	3027,3024,3021,3018,
	3015,3012,3009,3006,
	3003,3000,2997,2994,
	2991,2988,2985,2982,
	2980,2977,2974,2971,
	2969,2966,2963,2961,
	2958,2955,2953,2950,
	2948,2945,2943,2940,
	2938,2935,2933,2930,
	2928,2926,2923,2921,
	2919,2916,2914,2912,
	2910,2907,2905,2903,
	2901,2899,2896,2894,
	2892,2890,2888,2886,
	2884,2882,2880,2878,
	2876,2874,2872,2870,
	2868,2866,2864,2862,
	2860,2858,2856,2854,
	2853,2851,2849,2847,
	2845,2844,2842,2840,
	2838,2837,2835,2833,
	2831,2830,2828,2826,
	2825,2823,2821,2820,
	2818,2817,2815,2813,
	2812,2810,2809,2807,
	2806,2804,2803,2801,
	2800,2798,2797,2795,
	2794,2792,2791,2789,
	2788,
};

/*G0,G1,G2,G3,G4,G5*/
//static u32 m_dwGLuxBuffer[] = {210, 1024, 1096, 1600, 2000, 2400};
/*K0,K1,K2,K3,K4,K5*/
//static u32 m_dwKLuxBuffer[] = {511, 51, 46, 30, 30, 30};
/*X1,X2,X3,X4,X5*/
//static u32 m_dwXLuxBuffer[] = {20, 1200, 1300, 1536, 3584};
/*org=320; Joey modify 20160712*/
//static u32 m_dwBLux = 50;
/*org=128; Joey modify 20160712*/
//static u32 m_dwTH_LuxAdj = 150;

u8 iris_get_dbc_lut_index(void)
{
	return iris_dbc_lut_index;
}

struct iris_setting_info *iris_get_setting(void)
{
	return &iris_setting;
}

void iris_set_yuv_input(bool val)
{
	shadow_iris_require_yuv_input = val;
}

void iris_set_HDR10_YCoCg(bool val)
{
	shadow_iris_HDR10_YCoCg = val;
}

u32 iris_get_csc_type(u32 type)
{
	u32 csc_type = type;
	struct iris_cfg *pcfg = iris_get_cfg();

	if (pcfg->valid > 0) {
		if (iris_require_yuv_input)
			csc_type = MDSS_MDP_CSC_YUV2YUV;
		else if (iris_HDR10)
			csc_type = MDSS_MDP_CSC_YUV2RGB_2020L;
		if (iris_HDR10_YCoCg)
			csc_type = MDSS_MDP_CSC_YCoCg;
	}

	return csc_type;
}

void iris_init_HDRchange(void)
{
	iris_HDRchange_cnt = 0;
}

bool iris_has_HDRchange(void)
{
	struct iris_cfg *pcfg = NULL;

	pcfg = iris_get_cfg();
	return (pcfg->valid > 0 && iris_HDRchange_cnt < 16);
}

void iris_add_HDRchange(void)
{
	iris_HDRchange_cnt++;
}

bool iris_get_debug_cap(void)
{
	return iris_debug_cap;
}

void iris_set_debug_cap(bool val)
{
	iris_debug_cap = val;
}

void iris_set_skip_dma(bool skip)
{
	pr_debug("skip_dma=%d\n", skip);
	iris_skip_dma = skip;
}

void iris_set_sdr2hdr_mode(u8 mode)
{
	iris_sdr2hdr_mode = mode;
}

u8 iris_get_sdr2hdr_mode(void)
{
	return iris_sdr2hdr_mode;
}

static int iris_capture_disable_pq(struct iris_update_ipopt *popt, bool *skiplast)
{
	int len = 0;

	*skiplast = 0;
	if ((!iris_capture_ctrl_en) && (!iris_debug_cap)) {
		if (!iris_dynamic_power_get())
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x50, 0x50, IRIS_LAST_BIT_CTRL);
		else
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x52, 0x52, IRIS_LAST_BIT_CTRL);

		*skiplast = 1;
	} else if (!iris_dynamic_power_get())
		*skiplast = 1;
	return len;
}

static int iris_capture_enable_pq(struct iris_update_ipopt *popt, int oldlen)
{
	int len = oldlen;

	if ((!iris_capture_ctrl_en) && (!iris_debug_cap))
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			0x51, 0x51,
			(!iris_dynamic_power_get()) ? 0x01 : 0x0);

	if (!iris_dynamic_power_get() && !iris_skip_dma)
		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_DMA, 0xe2, 0xe2, 0);
	return len;
}

static int iris_capture_disable_lce(struct iris_update_ipopt *popt, bool *skiplast)
{
	int len = 0;

	*skiplast = 0;
	if ((!iris_capture_ctrl_en)
			&& (iris_lce_power_status_get())
			&& (!iris_debug_cap)) {
		if (!iris_dynamic_power_get())
			iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x50, 0x50, IRIS_LAST_BIT_CTRL);
		else
			iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x52, 0x52, IRIS_LAST_BIT_CTRL);

		*skiplast = 1;
	} else if (!iris_dynamic_power_get())
		*skiplast = 1;
	return len;
}

static int iris_capture_enable_lce(struct iris_update_ipopt *popt, int oldlen)
{
	int len = oldlen;

	if ((!iris_capture_ctrl_en)
			&& (iris_lce_power_status_get())
			&& (!iris_debug_cap))
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			0x51, 0x51, (!iris_dynamic_power_get())?0x01:0x0);

	if (!iris_dynamic_power_get() && !iris_skip_dma) {
		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_DMA, 0xe3, 0xe3, 0x01);
		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_DMA, 0xe2, 0xe2, 0);
	}
	return len;
}

static int iris_capture_enable_demo(struct iris_update_ipopt *popt, int oldlen)
{
	int len = oldlen;

	if ((!iris_capture_ctrl_en) && (!iris_debug_cap))
		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_PWIL,
			0x51, 0x51, (!iris_dynamic_power_get())?0x01:0x0);

	if (!iris_dynamic_power_get()) {
		if (iris_lce_power_status_get())
			iris_init_update_ipopt_t(
				popt, IP_OPT_MAX, IRIS_IP_DMA,
				0xe3, 0xe3, 0x01);

		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_DMA, 0xe2, 0xe2, 0);
	}
	return len;
}

void iris_init_ipopt_ip(struct iris_update_ipopt *ipopt,  int len)
{
	int i = 0;

	for (i = 0; i < len; i++)
		ipopt[i].ip = 0xff;
}

static u32 iris_color_temp_x_get(u32 index)
{
	return iris_color_x_buf[index];
}

void iris_pq_parameter_init(void)
{
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	struct iris_cfg *pcfg = NULL;
	struct mdss_panel_info *pinfo = NULL;
	u32 index;
	pcfg = iris_get_cfg();
	pinfo = &(pcfg->ctrl->panel_data.panel_info);

	if (pqlt_cur_setting->pq_setting.sdr2hdr
			== SDR2HDR_Bypass)
		iris_yuv_datapath = false;
	else
		iris_yuv_datapath = true;

	iris_dbc_lut_index = 0;
	if (pinfo->mipi.mode == DSI_VIDEO_MODE)
		iris_debug_cap = true;

	iris_min_color_temp = pcfg->min_color_temp;
	iris_max_color_temp = pcfg->max_color_temp;

	index = (iris_min_color_temp-IRIS_CCT_MIN_VALUE)/IRIS_CCT_STEP;
	iris_min_x_value = iris_color_temp_x_get(index);

	index = (iris_max_color_temp-IRIS_CCT_MIN_VALUE)/IRIS_CCT_STEP;
	iris_max_x_value = iris_color_temp_x_get(index);

	pr_err("%s, iris_min_x_value=%d, iris_max_x_value = %d\n", __func__, iris_min_x_value, iris_max_x_value);
}

void iris_peaking_level_set(u32 level)
{
	u32 csc;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	bool skiplast = 0;
	int len;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);

	csc = (level == 0) ? 0x11 : 0x10;
	if (iris_yuv_datapath == true)
		csc = 0x11;

	len = iris_capture_disable_pq(popt, &skiplast);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PEAKING,
			level, 0x01);
	len  = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PEAKING,
			csc, skiplast);

	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);

	pr_info("peaking level=%d, len=%d\n", level, len);
}


static int iris_cm_csc_set(struct iris_update_ipopt *popt, uint8_t skip_last, bool enable)
{
	struct iris_update_regval regval;
	int len = 0;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	uint32_t  *payload = NULL;
	u32 csc_value;

	payload = iris_get_ipopt_payload_data(IRIS_IP_CM,
			pqlt_cur_setting->pq_setting.cm6axis, 2);

	regval.ip = IRIS_IP_CM;
	regval.opt_id = 0xfb;
	regval.mask = 0xffffffff;
	if (enable == true)
		csc_value = 0x08000000;
	else
		csc_value = 0x80000000;
	regval.value = ((*payload) & 0x77ffffff)|csc_value;
	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_CM,
			0xfb, 0xfb, skip_last);
	pr_info("enable value=%d\n", enable);
	return len;
}

void iris_cm_6axis_level_set(u32 level)
{
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	bool cm_csc_enable = true;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);
	if ((level == 0)
			&& (pqlt_cur_setting->pq_setting.cm6axis == 0)
			&& (iris_yuv_datapath == false))
		cm_csc_enable = false;

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_CM, level, 0x01);

	len = iris_cm_csc_set(popt, skiplast, cm_csc_enable);

	len = iris_capture_enable_pq(popt, len);

	iris_update_pq_opt(popt, len);
	pr_info("cm 6axis level=%d, len=%d\n", level, len);
}

void iris_cm_ftc_enable_set(bool enable)
{
	u32 locallevel;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	locallevel = 0x10 | (u8)enable;
	len = iris_capture_disable_pq(popt, &skiplast);

	len = iris_update_ip_opt(
		popt, IP_OPT_MAX, IRIS_IP_CM, locallevel, skiplast);

	len = iris_capture_enable_pq(popt, len);

	iris_update_pq_opt(popt, len);
	pr_info("cm ftc enable=%d, len=%d\n", enable, len);
}

int iris_cm_ratio_set(struct iris_update_ipopt *popt, uint8_t skip_last)
{
	u32 tablesel;
	u32 index;
	u32 xvalue;
	u32 ratio;
	u32 value;
	u32 regvalue = 0;
	struct iris_update_regval regval;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	int len;

	if (pqlt_cur_setting->pq_setting.cmcolortempmode == IRIS_COLOR_TEMP_MANUL)
		value = pqlt_cur_setting->colortempvalue;
	else if (pqlt_cur_setting->pq_setting.cmcolortempmode == IRIS_COLOR_TEMP_AUTO)
		value = pqlt_cur_setting->cctvalue;


	if(value > iris_max_color_temp)
		value = iris_max_color_temp;
	else if(value < iris_min_color_temp)
		value = iris_min_color_temp;
	index = (value - IRIS_CCT_MIN_VALUE)/25;
	xvalue = iris_color_temp_x_get(index);

	if (xvalue == iris_min_x_value) {
		tablesel = 0;
		regvalue = tablesel |0x02;
	} else if( (xvalue < iris_min_x_value) && ( xvalue >= IRIS_X_6500K )) {
		tablesel = 0;
		ratio = ((xvalue - IRIS_X_6500K)*16383)/(iris_min_x_value - IRIS_X_6500K);
		regvalue = tablesel | (ratio<<16);
	} else if( (xvalue >= iris_max_x_value) && (xvalue < IRIS_X_6500K) ) {
		tablesel = 1;
		ratio = ((xvalue - iris_max_x_value)*16383)/(IRIS_X_6500K - iris_max_x_value);
		regvalue = tablesel | (ratio<<16);
	}

	regval.ip = IRIS_IP_CM;
	regval.opt_id = 0xfd;
	regval.mask = 0xffffffff;
	regval.value = regvalue;

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(
		popt, IP_OPT_MAX, IRIS_IP_CM, 0xfd, 0xfd, skip_last);
	pr_info("cm color temperature value=%d\n", value);
	return len;
}

void iris_cm_colortemp_mode_set(u32 mode)
{
	struct iris_update_regval regval;
	bool skiplast = 0;
	int len = 0;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	struct iris_cfg *pcfg = iris_get_cfg();

	/*do not generate lut table during mode switch.*/
	if (pqlt_cur_setting->source_switch != 1) {
		iris_init_ipopt_ip(popt,  IP_OPT_MAX);
		regval.ip = IRIS_IP_CM;
		regval.opt_id = 0xfc;
		regval.mask = 0x00000020;
		regval.value = (mode == 0)?0x00000020:0x00000000;
		len = iris_capture_disable_pq(popt, &skiplast);
		if (mode == IRIS_COLOR_TEMP_OFF) {
			iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
					0xE0, 0x01);
		} else {
			iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
					pqlt_cur_setting->pq_setting.cmcolorgamut + 0xE1,
					0x01);
		}
		iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DPP,
				0xfe, 0xfe, 0x01);
		if (pcfg->lut_mode == INTERPOLATION_MODE) {

			if (mode > IRIS_COLOR_TEMP_OFF)
				len = iris_cm_ratio_set(popt, 0x01);

			if (pqlt_cur_setting->source_switch == 2) {
				regval.mask |= 0x00000011;
				regval.value |= 0x00000011;
				pqlt_cur_setting->source_switch = 0;
			} else if (!iris_dynamic_power_get()) {
				regval.mask |= 0x00000011;
				if (mode == 0)
					regval.value |= 0x00000000;
				else
					regval.value |= 0x00000011;
			}
		}
		iris_update_bitmask_regval_nonread(&regval, false);
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_CM,
				0xfc, 0xfc, skiplast);

		len = iris_capture_enable_pq(popt, len);

		iris_update_pq_opt(popt, len);
	}
	pr_info("cm color temperature mode=%d, len=%d\n", mode, len);
}

void iris_cm_color_temp_set(void)
{
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct iris_cfg *pcfg = iris_get_cfg();
	/*struct quality_setting *pqlt_cur_setting = & iris_setting.quality_cur;*/

	/*do not have color temp function for single mode*/
	if (pcfg->lut_mode == SINGLE_MODE)
		return;

	/*if(pqlt_cur_setting->pq_setting.cmcolorgamut == 0) {*/
	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);
	len = iris_cm_ratio_set(popt, skiplast);

	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);
	/*}*/
	pr_info("%s, len = %d\n",  __func__, len);
}

void iris_cm_color_gamut_pre_set(u32 source_switch)
{
	struct iris_update_regval regval;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	struct iris_cfg *pcfg = iris_get_cfg();

	/*add protection for source and scene switch at the same time*/
	if (source_switch == 3)
		source_switch = 1;
	pqlt_cur_setting->source_switch = source_switch;

	/*do not have color temp function for single mode*/
	if (pcfg->lut_mode == SINGLE_MODE)
		return;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);

	regval.ip = IRIS_IP_CM;
	regval.opt_id = 0xfc;
	regval.mask = 0x00000011;
	regval.value = 0x00000000;

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_CM,
			0xfc, 0xfc, skiplast);
	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);

	pr_info("source switch = %d, len=%d\n", source_switch, len);
}

void iris_cm_color_gamut_set(u32 level)
{
	struct iris_update_regval regval;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	u32 gammalevel;
	struct iris_cfg *pcfg = iris_get_cfg();

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);

	regval.ip = IRIS_IP_CM;
	regval.opt_id = 0xfc;
	if (pcfg->lut_mode == INTERPOLATION_MODE) {
		regval.mask = 0x00000011;
		regval.value = 0x00000011;
	} else {
		regval.mask = 0x0000000c;
		regval.value = level << 2;
	}

	/*use liner gamma if cm lut disable*/
	if (pqlt_cur_setting->pq_setting.cmcolortempmode ==
		IRIS_COLOR_TEMP_OFF)
		gammalevel = 0;
	else
		gammalevel = level + 1;


	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT, 0xE0 + gammalevel, 0x01);
	if (pcfg->lut_mode == INTERPOLATION_MODE) {
		iris_init_update_ipopt_t(popt,  IP_OPT_MAX, IRIS_IP_DPP, 0xfe, 0xfe, 0x01);
		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT, 0x40 + level * 3 + 0, 0x01);
		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT, 0x40 + level * 3 + 1, 0x01);
		len = iris_update_ip_opt(
			popt, IP_OPT_MAX, IRIS_IP_EXT,
			0x40 + level*3 + 2, (pqlt_cur_setting->source_switch == 0) ? 0x01 : skiplast);
	} else
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DPP, 0xfe, 0xfe, (pqlt_cur_setting->source_switch == 0) ? 0x01 : skiplast);

	/*do not generate lut table for source switch.*/
	if (pqlt_cur_setting->source_switch == 0) {
		iris_update_bitmask_regval_nonread(&regval, false);
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_CM,
				0xfc, 0xfc, skiplast);
	}
	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);
	pr_info("cm color gamut=%d, len=%d\n", level, len);
}

void iris_dpp_gamma_set(void)
{
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	u32 gammalevel;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);

	if (pqlt_cur_setting->pq_setting.cmcolortempmode
			== IRIS_COLOR_TEMP_OFF)
		gammalevel = 0; /*use liner gamma if cm lut disable*/
	else
		gammalevel = pqlt_cur_setting->pq_setting.cmcolorgamut + 1;

	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
			0xE0 + gammalevel, 0x01);
	len = iris_init_update_ipopt_t(popt,  IP_OPT_MAX, IRIS_IP_DPP,
			0xfe, 0xfe, skiplast);

	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);
}

static int iris_lce_gamm1k_set(
		struct iris_update_ipopt *popt, uint8_t skip_last)
{
	u32 dwGain, dwAHEGAMMA1K;
	u32 level;
	struct iris_update_regval regval;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	int len = 0;
	uint32_t  *payload = NULL;

	level = (pqlt_cur_setting->pq_setting.lcemode) * 5
		+ pqlt_cur_setting->pq_setting.lcelevel;
	payload = iris_get_ipopt_payload_data(IRIS_IP_LCE, level, 5);

	if (pqlt_cur_setting->pq_setting.alenable == true) {
		if (pqlt_cur_setting->luxvalue > 100000)
			pqlt_cur_setting->luxvalue = 100000;
		if (pqlt_cur_setting->luxvalue < 10000)
			pqlt_cur_setting->luxvalue = 10000;
		dwGain = pqlt_cur_setting->luxvalue;
		dwAHEGAMMA1K = ((*payload) & 0xff000000) >> 24;

		dwAHEGAMMA1K =dwAHEGAMMA1K + (255 * dwGain)/100000;
		regval.ip = IRIS_IP_LCE;
		regval.opt_id = 0xfd;
		regval.mask = 0xffffffff;
		regval.value = ((*payload) & 0x00ffffff)|(dwAHEGAMMA1K<<24);

		iris_update_bitmask_regval_nonread(&regval, false);
		len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_LCE,
			0xfd, 0xfd, skip_last);
	}
	pr_err("lux value=%d\n", pqlt_cur_setting->luxvalue);
	return len;
}

static int iris_lce_gamm1k_restore(
	struct iris_update_ipopt *popt, uint8_t skip_last)
{
	u32 level;
	struct iris_update_regval regval;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	int len = 0;
	uint32_t  *payload = NULL;


	level = (pqlt_cur_setting->pq_setting.lcemode) * 5
				+ pqlt_cur_setting->pq_setting.lcelevel;
	payload = iris_get_ipopt_payload_data(IRIS_IP_LCE, level, 5);

	regval.ip = IRIS_IP_LCE;
	regval.opt_id = 0xfd;
	regval.mask = 0xffffffff;
	regval.value = *payload;
	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(
			popt, IP_OPT_MAX, IRIS_IP_LCE,
			0xfd, 0xfd, skip_last);

	pr_err("%s, len = %d\n", __func__, len);
	return len;
}

void iris_lce_mode_set(u32 mode)
{
	u32 locallevel;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	bool skiplast = 0;
	int len = 0;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_lce(popt, &skiplast);

	if (mode == IRIS_LCE_GRAPHIC)
		locallevel = pqlt_cur_setting->pq_setting.lcelevel;
	else
		locallevel = pqlt_cur_setting->pq_setting.lcelevel + 6;

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_LCE, locallevel, 0x01);

	if (pqlt_cur_setting->pq_setting.alenable == true
		&& pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
		len = iris_lce_gamm1k_set(popt, skiplast);
	else
		len = iris_lce_gamm1k_restore(popt, skiplast);

	len = iris_capture_enable_lce(popt, len);

	iris_update_pq_opt(popt, len);

	pr_info("lce mode=%d, len=%d\n", mode, len);
}

void iris_lce_level_set(u32 level)
{
	u32 locallevel;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_lce(popt, &skiplast);

	if (pqlt_cur_setting->pq_setting.lcemode
			== IRIS_LCE_GRAPHIC)
		locallevel = level;
	else
		locallevel = level + 6;

	len = iris_update_ip_opt(
		popt, IP_OPT_MAX, IRIS_IP_LCE, locallevel, 0x01);

	/* hdr's al may use lce */
	if (pqlt_cur_setting->pq_setting.alenable == true
		&& pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
		len = iris_lce_gamm1k_set(popt, skiplast);
	else
		len = iris_lce_gamm1k_restore(popt, skiplast);

	len = iris_capture_enable_lce(popt, len);

	iris_update_pq_opt(popt, len);

	pr_info("lce level=%d, len=%d\n", level, len);
}

void iris_lce_graphic_det_set(bool enable)
{
	u32 locallevel;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_lce(popt, &skiplast);

	locallevel = 0x10 | (u8)enable;

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_LCE,
			locallevel, skiplast);

	len = iris_capture_enable_lce(popt, len);

	iris_update_pq_opt(popt, len);
	pr_info("lce graphic det=%d, len=%d\n", enable, len);
}

void iris_lce_al_set(bool enable)
{
	if (enable == true)
		iris_lce_lux_set();
	else {
		bool skiplast = 0;
		int len;
		struct iris_update_ipopt popt[IP_OPT_MAX];

		iris_init_ipopt_ip(popt,  IP_OPT_MAX);
		len = iris_capture_disable_lce(popt, &skiplast);
		len = iris_lce_gamm1k_restore(popt, skiplast);

		len = iris_capture_enable_lce(popt, len);
		iris_update_pq_opt(popt, len);
		pr_info("%s, len = %d\n", __func__, len);
	}
	pr_info("lce al enable=%d\n", enable);
}

void iris_dbc_level_set(u32 level)
{
	u32 locallevel;
	u32 dbcenable;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	u8 localindex;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_lce(popt, &skiplast);

	dbcenable = (level > 0) ? 0x01:0x00;
	locallevel = 0x10 | level;

	iris_dbc_lut_index ^= 1;

	localindex = 0x20 | iris_dbc_lut_index;
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
			0x20 + level+1, 0x01);
	iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DBC,
			0xfe, 0xfe, 0x01);
	iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DBC,
			localindex, localindex, 0x01);
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_DBC,
			dbcenable, 0x01);
	/*len = iris_update_ip_opt(popt,  IP_OPT_MAX, IRIS_IP_DBC, locallevel, skiplast);*/
	len = iris_capture_enable_lce(popt, len);

	iris_update_pq_opt(popt, len);

	pr_info("dbc level=%d, len=%d\n", level, len);
}

void iris_demo_mode_set(u32 mode)
{
	u32 locallevel;
	u32 cmlevel;
	u32 lcesdrlevel;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);

	locallevel = 0xe0 | mode;
	lcesdrlevel = (mode == 0) ? 0xe0:0xe1;

	switch (mode) {
	case 0:
		cmlevel = 0xe0;
		break;
	case 1:
	case 2:
		cmlevel = 0xe1;
		break;
	case 3:
		cmlevel = 0xe2;
		break;
	default:
		break;
	}

	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PEAKING,
			locallevel, 0x01);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_CM,
			cmlevel, 0x01);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			locallevel, 0x01);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
			lcesdrlevel, 0x01);
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_LCE,
			lcesdrlevel, skiplast);

	len = iris_capture_enable_demo(popt, len);

	iris_update_pq_opt(popt, len);
	pr_info("demo mode=%d, len=%d\n", mode, len);
}

void iris_reading_mode_set(u32 level)
{
	u32 locallevel;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	bool cm_csc_enable = true;

	/*only take affect when sdr2hdr bypass */
	if (pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass) {
		iris_init_ipopt_ip(popt,  IP_OPT_MAX);
		len = iris_capture_disable_pq(popt, &skiplast);

		locallevel = 0x40 | level;

		if ((level == 0) &&
			(pqlt_cur_setting->pq_setting.cm6axis == 0))
			cm_csc_enable = false;

		len = iris_cm_csc_set(popt, 0x01, cm_csc_enable);

		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_CM,
				locallevel, 0x01);
		len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_DPP,
				locallevel, skiplast);

		len = iris_capture_enable_pq(popt, len);
		iris_update_pq_opt(popt, len);
	}
	pr_info("reading mode=%d\n", level);
}

void iris_lce_lux_set(void)
{
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_lce(popt, &skiplast);
	len = iris_lce_gamm1k_set(popt, skiplast);

	len = iris_capture_enable_lce(popt, len);
	iris_update_pq_opt(popt, len);
	pr_info("%s, len = %d\n", __func__, len);
}

void iris_ambient_light_lut_set(void)
{
#define AL_SDR2HDR_INDEX_OFFSET 7
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	bool skiplast = 0;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);
	if (pqlt_cur_setting->pq_setting.alenable == true) {

		if (pqlt_cur_setting->pq_setting.sdr2hdr >= HDR10In_ICtCp &&
			pqlt_cur_setting->pq_setting.sdr2hdr <= ICtCpIn_YCbCr) {
			iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
					(AL_SDR2HDR_INDEX_OFFSET + 2), 0x01);

			len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
					0xc0, skiplast);
		} else if (pqlt_cur_setting->pq_setting.sdr2hdr >= SDR709_2_709 &&
				pqlt_cur_setting->pq_setting.sdr2hdr <= SDR709_2_2020) {
			iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
				(AL_SDR2HDR_INDEX_OFFSET + 5), 0x01);
		/*(AL_SDR2HDR_INDEX_OFFSET +
			pqlt_cur_setting->pq_setting.sdr2hdr), 0x01);*/
			len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
					0xc0, skiplast);
		}
	} else {
		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
				pqlt_cur_setting->pq_setting.sdr2hdr, 0x01);

		len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
				0x60 + pqlt_cur_setting->pq_setting.sdr2hdr - 1,
				skiplast);
	}

	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);
	pr_info("al=%d hdr level=%d\n",
		pqlt_cur_setting->pq_setting.alenable,
		pqlt_cur_setting->pq_setting.sdr2hdr);
}

void iris_maxcll_lut_set(void)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	bool skiplast = 0;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);
	/*Set skiplast=0 due to iris_capture_enable_pq() do nothing*/
	if (!iris_dynamic_power_get() && iris_skip_dma)
		skiplast = 0;
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT, 0xa0, skiplast);
	len = iris_capture_enable_pq(popt,len);
	iris_update_pq_opt(popt, len);
	pr_info("%s, len=%d\n", __func__, len);
}


void iris_dbclce_datapath_set(bool bEn)
{
	struct iris_update_regval regval;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	len = iris_capture_disable_pq(popt, &skiplast);

	if (!iris_lce_power_status_get())
		bEn = 0;/*cannot enable lce dbc data path if power off*/
	regval.ip = IRIS_IP_PWIL;
	regval.opt_id = 0xfd;
	regval.mask = 0x00000001;
	regval.value = (bEn == true)?0x00000001:0x00000000;
	iris_update_bitmask_regval_nonread(&regval, false);

	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			0xfd, 0xfd, skiplast);
	if ((!iris_capture_ctrl_en)
		&& (!iris_dynamic_power_get())
		&& (!iris_debug_cap))
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x30, 0x30, 1);
	len = iris_capture_enable_pq(popt, len);
	iris_update_pq_opt(popt, len);
	pr_info("dbclce_en=%d, len=%d\n", bEn, len);
}


void iris_dbclce_power_set(bool bEn)
{
	struct iris_update_regval regval;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	iris_lce_power_status_set(bEn);


	regval.ip = IRIS_IP_SYS;
	regval.opt_id = 0xfc;
	regval.mask = 0x00000020;
	regval.value = (bEn == true) ? 0x00000020 : 0x00000000;
	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_SYS,
			0xfc, 0xfc, 0);

	iris_update_pq_opt(popt, len);
	pr_info("dbclce_power=%d, len=%d\n", bEn, len);
}

void iris_sdr2hdr_level_set(u32 level)
{
	struct iris_update_regval regval;
	u32 pwil_channel_order;
	u32 pwil_csc;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	u32 cm_csc = 0x40;
	bool cm_csc_enable = true;
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	u32 peaking_csc;
	bool skiplast = 0;
	bool dma_sent = false;
	struct iris_cfg *pcfg = iris_get_cfg();
	u32 gammalevel;

	// Don't set sdr2hdr level.
	if (iris_sdr2hdr_mode == 2 && level == SDR709_2_p3)
		return;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	if (!iris_dynamic_power_get() && iris_capture_ctrl_en)
		skiplast = 1;

	if (pqlt_cur_setting->pq_setting.readingmode != 0)
		cm_csc = 0x41;

	//shadow_iris_require_yuv_input = iris_require_yuv_input;
	//shadow_iris_HDR10 = iris_HDR10;
	//shadow_iris_HDR10_YCoCg = iris_HDR10_YCoCg;
	if ((level <= ICtCpIn_YCbCr) && (level >= HDR10In_ICtCp)) {
		/*Not change iris_require_yuv_input due to magic code in ioctl.*/
		shadow_iris_HDR10 = true;
	} else if (level == SDR709_2_709) {
		shadow_iris_require_yuv_input = true;
		shadow_iris_HDR10 = false;
	} else {
		shadow_iris_require_yuv_input = false;
		shadow_iris_HDR10 = false;
		shadow_iris_HDR10_YCoCg = false;
	}

	if (level >= HDR10In_ICtCp && level <= SDR709_2_2020) {
		iris_yuv_datapath = true;
		/*sdr2hdr level in irisconfigure start from 1,
		but lut table start from 0, so need -1.*/
		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT,
				0x60 + level - 1, 0x01);
		iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
				0xfe, 0xfe, 0x01);
	} else if (level == SDR2HDR_Bypass) {
		iris_yuv_datapath = false;
	} else {
		pr_err("%s something is  wrong level=%d\n", __func__, level);
		return;
	}

	regval.ip = IRIS_IP_PWIL;
	regval.opt_id = 0xfd;
	regval.mask = 0x180004;
	if ((level <= ICtCpIn_YCbCr)
		&& (level >= HDR10In_ICtCp)
		&& (shadow_iris_require_yuv_input || shadow_iris_HDR10_YCoCg)) {
		/*channel order YUV, ICtCp*/
		pwil_channel_order = 0x60;
		if (level == ICtCpIn_YCbCr)
			pwil_csc = 0x91;/*pwil csc ICtCp to RGB. */
		else
			pwil_csc = 0x90;/*pwil csc YUV to RGB*/
		regval.value = 0x100000;
		if (shadow_iris_HDR10_YCoCg) {/*RGBlike solution*/
			pwil_channel_order = 0x62;
			pwil_csc = 0x94;
		}
	} else if (level == SDR709_2_709) {
		pwil_channel_order = 0x60;/*channel order YUV*/
		pwil_csc = 0x92;/*pwil csc YUV to YUVFull */
		regval.value = 0x100000;
	} else {
		pwil_channel_order = 0x61;/*channel order RGB*/
		pwil_csc = 0x93;/*pwil csc default*/
		regval.value = 0x04;
	}
	switch (level) {
	case HDR10In_ICtCp:
	case HDR10In_YCbCr:
	case ICtCpIn_YCbCr:
		cm_csc = 0x45;
		break;
	case SDR709_2_709:
		/*TODOS*/
		break;
	case SDR709_2_p3:
		cm_csc = 0x46;
		break;
	case SDR709_2_2020:
		/*TODOS*/
		break;
	default:
		break;
	}

	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			pwil_channel_order, 0x01);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			pwil_csc, 0x01);
	iris_update_bitmask_regval_nonread(&regval, false);
	iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
			0xfd, 0xfd, 0x01);

	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SDR2HDR,
			level, 0x01);
	iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_CM,
			cm_csc, 0x01);

	if (pcfg->lut_mode == INTERPOLATION_MODE) {
		/*add the workaround to let hdr and cm lut
		table take affect at the same time*/
		if (pqlt_cur_setting->pq_setting.cmcolortempmode > IRIS_COLOR_TEMP_OFF)
			/*change ratio when source switch*/
			len = iris_cm_ratio_set(popt, 0x01);
	}

	regval.ip = IRIS_IP_CM;
	regval.opt_id = 0xfc;
	if (pcfg->lut_mode == INTERPOLATION_MODE) {
		regval.mask = 0x00000031;
		regval.value = (pqlt_cur_setting->pq_setting.cmcolortempmode == 0)
						? 0x00000031 : 0x00000011;
	} else {
		regval.mask = 0x0000002c;
		regval.value = pqlt_cur_setting->pq_setting.cmcolorgamut << 2;
		if (pqlt_cur_setting->pq_setting.cmcolortempmode == 0)
			regval.value += 0x20;
	}

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_CM,
			0xfc, 0xfc, 0x01);

	if (pqlt_cur_setting->source_switch == 1) {
		/*use liner gamma if cm lut disable*/
		if (pqlt_cur_setting->pq_setting.cmcolortempmode ==
			IRIS_COLOR_TEMP_OFF)
			gammalevel = 0;
		else
			gammalevel = pqlt_cur_setting->pq_setting.cmcolorgamut + 1;


		iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_EXT, 0xE0 + gammalevel, 0x01);
		len = iris_init_update_ipopt_t(popt,  IP_OPT_MAX, IRIS_IP_DPP, 0xfe, 0xfe, 0x01);
	}
	if (iris_yuv_datapath == true)
		peaking_csc = 0x11;
	else {
		if ((pqlt_cur_setting->pq_setting.readingmode == 0)
			&& (pqlt_cur_setting->pq_setting.cm6axis == 0))
			cm_csc_enable = false;

		if (pqlt_cur_setting->pq_setting.peaking == 0)
			peaking_csc = 0x11;
		else
			peaking_csc = 0x10;
	}

	len = iris_cm_csc_set(popt, 0x01, cm_csc_enable);
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PEAKING,
			peaking_csc, skiplast);

	if (!iris_dynamic_power_get() && iris_capture_ctrl_en) {
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA,
				0xe2, 0xe2, 0);
		dma_sent = true;
	}

	pr_debug("send sdr2hdr cmds\n");
	iris_update_pq_opt(popt, len);

	if (dma_sent) {
		pr_debug("sdr2hdr cmds complete\n");
		iris_require_yuv_input = shadow_iris_require_yuv_input;
		iris_HDR10 = shadow_iris_HDR10;
		iris_HDR10_YCoCg = shadow_iris_HDR10_YCoCg;
	}

	pqlt_cur_setting->source_switch = 0;
	pr_info("sdr2hdr level =%d, len = %d\n", level, len);
}

void iris_peaking_idle_clk_enable(bool enable)
{
	u32 locallevel;
	bool skiplast = 0;
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	locallevel = 0xa0 | (u8)enable;
	len = iris_capture_disable_pq(popt, &skiplast);

	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PEAKING,
			locallevel, locallevel, skiplast);

	len = iris_capture_enable_pq(popt, len);

	iris_update_pq_opt(popt, len);
	pr_info("peaking idle clk enable=%d\n", enable);
}

void iris_cm_6axis_seperate_gain(u8 gain_type, u32 value)
{
	struct iris_update_ipopt popt[IP_OPT_MAX];
	bool skiplast = 0;
	int len;
	struct iris_update_regval regval;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);

	len = iris_capture_disable_pq(popt, &skiplast);

	regval.ip = IRIS_IP_CM;
	/*6 axis seperate gain id start from 0xb0*/
	regval.opt_id = 0xb0 + gain_type;
	regval.mask = 0x00fc0000;
	regval.value = value << 18;

	iris_update_bitmask_regval_nonread(&regval, false);
	len  = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_CM,
			regval.opt_id, regval.opt_id, skiplast);

	len = iris_capture_enable_pq(popt, len);

	iris_update_pq_opt(popt, len);

	pr_err("cm gain type = %d, value = %d, len = %d\n",
			gain_type, value, len);
}

void iris_pwm_freq_set(u32 value)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct iris_update_regval regval;
	u32 regvalue = 0;

	iris_init_ipopt_ip(popt, IP_OPT_MAX);

	regvalue = 1000000 / value;
	regvalue = (27000000 / 1024) / regvalue;

	regval.ip = IRIS_IP_PWM;
	regval.opt_id = 0xfd;
	regval.mask = 0xffffffff;
	regval.value = regvalue;

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWM,
			0xfd, 0xfd, 0);

	iris_update_pq_opt(popt, len);

	pr_err("%s, blc_pwm freq=%d\n", __func__, regvalue);
}

void iris_pwm_enable_set(bool enable)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_PWM, enable, 0);


	iris_update_pq_opt(popt, len);
	pr_err("%s, blc_pwm enable=%d\n", __func__, enable);
}

void iris_dbc_bl_user_set(u32 value)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct iris_update_regval regval;
	u32 regvalue = 0;
	bool skiplast = 0;

       if (iris_setting.quality_cur.pq_setting.dbc == 0)
              return;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);
	/*if ((!iris_dynamic_power_get())
	&& (iris_lce_power_status_get()))*/
	skiplast = 1;
	
	//regvalue = (value * 0xfff) / 0xff;
	regvalue = value;

	regval.ip = IRIS_IP_DBC;
	regval.opt_id = 0xfd;
	regval.mask = 0xffffffff;
	regval.value = regvalue;

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DBC,
			0xfd, 0xfd, skiplast);

	/*if ((!iris_dynamic_power_get())
		&& (iris_lce_power_status_get()))*/
	if (!iris_skip_dma)
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA,
					0xe3, 0xe3, 0);

	iris_update_pq_opt(popt, len);

	pr_err("%s,bl_user value=%d\n", __func__, regvalue);
}

void iris_dbc_led0d_gain_set(u32 value)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct iris_update_regval regval;

	iris_init_ipopt_ip(popt,  IP_OPT_MAX);

	regval.ip = IRIS_IP_DBC;
	regval.opt_id = 0xfc;
	regval.mask = 0xffffffff;
	regval.value = value;

	iris_update_bitmask_regval_nonread(&regval, false);
	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DBC,
			regval.opt_id, regval.opt_id, 1);

	if (!iris_skip_dma)
		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA,
					0xe3, 0xe3, 0);

	iris_update_pq_opt(popt, len);

	pr_err("%s,dbc_led0d value=%d\n", __func__, value);
}

/*****************
** only for vv OLED panel
******************/
extern int fih_set_fs_curr(int fs_curr);

void iris_panel_nits_set(u32 bl_ratio, bool bSystemRestore, int level)
{
	struct dcs_cmd_req cmdreq;
	u8 brightness[2] = {0x51, 0x0};
	struct dsi_cmd_desc backlight_cmd = {
		{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(brightness)}, brightness
	};
	struct iris_cfg *pcfg = iris_get_cfg();
	u32 bl_level;

	/* Don't control panel's brightness when sdr2hdr mode is 3 */
	if (iris_sdr2hdr_mode == 3)
		return;

	if (bSystemRestore)
		bl_level = iris_setting.quality_cur.system_brightness;
	else
		bl_level = bl_ratio * pcfg->panel_dimming_brightness / PANEL_BL_MAX_RATIO;

	switch (pcfg->ctrl->bklt_ctrl) {
	case BL_WLED:
		if (pcfg->bl_led)
			led_trigger_event(pcfg->bl_led, bl_level);
		break;
	case BL_DCS_CMD:
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = &backlight_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_LP_MODE | CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		/* sent dimming brightness */
		brightness[1] = bl_level;
		iris_panel_cmd_passthrough(pcfg->ctrl, &cmdreq);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n", __func__);
		break;
	}

	if (bSystemRestore) {
		if (pcfg->fs_curr == 1) {
			msleep(1);
			fih_set_fs_curr(0);
			pcfg->fs_curr = 0;
		}
	} else if (level == 2) {
		if (pcfg->fs_curr == 0) {
			msleep(1);
			fih_set_fs_curr(1);
			pcfg->fs_curr = 1;
		}
	}

	pr_err("%s: bl_level=0x%x, bSystemRestore=%d\n",
			__func__, bl_level, bSystemRestore);
}


void iris_scaler_filter_update(u32 level)
{
	struct iris_update_ipopt popt[IP_OPT_MAX];
	int len;

	iris_init_ipopt_ip(popt, IP_OPT_MAX);

	len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_EXT,
			0x80 + level, 0x80 + level, 0);

	iris_update_pq_opt(popt, len);

	pr_info("scaler filter level=%d\n", level);
}


void iris_hdr_csc_prepare(void)
{
	struct iris_update_ipopt popt[IP_OPT_MAX];
	int len;
	bool skiplast = 0;

	if (iris_capture_ctrl_en == false) {
		pr_debug("AP csc prepare.\n");
		iris_capture_ctrl_en = true;
		iris_init_ipopt_ip(popt, IP_OPT_MAX);
		if (!iris_dynamic_power_get())
			skiplast = 1;

		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x52, 0x52, skiplast);
		if (!iris_dynamic_power_get() && !iris_skip_dma)
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA,
					0xe2, 0xe2, 0);

		iris_update_pq_opt(popt, len);
	}
}

void iris_hdr_csc_frame_ready(void)
{
	struct iris_cfg *pcfg = pcfg = iris_get_cfg();
	if (pcfg->valid > 0)
		complete(&pcfg->frame_ready_completion);
}

void iris_hdr_csc_complete(int step)
{
	int len;
	struct iris_update_ipopt popt[IP_OPT_MAX];
	bool skiplast = 0;

	ATRACE_BEGIN(__func__);

	if (step == 0 || step == 2) {
		pr_info("AP csc start.\n");
		iris_require_yuv_input = shadow_iris_require_yuv_input;
		iris_HDR10 = shadow_iris_HDR10;
		iris_HDR10_YCoCg = shadow_iris_HDR10_YCoCg;
		if (step == 0)
			goto end;
	} else if (step == 1) {
		struct iris_cfg *pcfg = pcfg = iris_get_cfg();
		ATRACE_BEGIN("iris_wait_frame_ready");
		reinit_completion(&pcfg->frame_ready_completion);
		if (!wait_for_completion_timeout(&pcfg->frame_ready_completion,
						 msecs_to_jiffies(50)))
			pr_err("%s: timeout waiting for frame ready\n", __func__);
		ATRACE_END("iris_wait_frame_ready");
	}

	pr_info("AP csc complete.\n");
	iris_init_ipopt_ip(popt, IP_OPT_MAX);
	if (iris_capture_ctrl_en == true) {
		if (!iris_dynamic_power_get()) {
			skiplast = 1;
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
					0x30, 0x30, 1);
		}

		len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_PWIL,
				0x51, 0x51, skiplast);

		if (!iris_dynamic_power_get())
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA,
					0xe2, 0xe2, 0);

		iris_update_pq_opt(popt, len);
		pr_info("iris csc complete.\n");
		iris_capture_ctrl_en = false;
	} else {
		if (!iris_dynamic_power_get()) {
			len = iris_init_update_ipopt_t(popt, IP_OPT_MAX, IRIS_IP_DMA, 0xe2, 0xe2, 0);
			iris_update_pq_opt(popt, len);
			pr_info("iris csc complete.\n");
		}
	}
end:
	ATRACE_END(__func__);
}


int32_t  iris_update_ip_opt(
		struct iris_update_ipopt *popt, int len,
		uint8_t ip, uint8_t opt_id, uint8_t skip_last)
{
	int i = 0;
	uint8_t old_opt;
	int32_t cnt = 0;
	struct iris_cfg *pcfg = iris_get_cfg();

	struct iris_pq_ipopt_val *pq_ipopt_val = iris_get_cur_ipopt_val(ip);

	if (pq_ipopt_val == NULL) {
		pr_err("can not get pq ipot val ip = %02x\n", ip);
		return 1;
	}

	if (ip == IRIS_IP_EXT)	{
		if (((opt_id & 0xe0) == 0x40)
			&& (pcfg->lut_mode == INTERPOLATION_MODE)) {  /*CM LUT table*/
			for (i = 0; i < pq_ipopt_val->opt_cnt; i++) {
				if (((opt_id & 0x1f)%CM_LUT_GROUP)
						== (((pq_ipopt_val->popt[i]) & 0x1f)
								% CM_LUT_GROUP)) {
					old_opt = pq_ipopt_val->popt[i];
					pq_ipopt_val->popt[i] = opt_id;
					cnt = iris_init_update_ipopt_t(popt, len, ip,
							old_opt, opt_id, skip_last);
					return cnt;
				}
			}
		} else if (((opt_id & 0xe0) == 0x60)
				|| ((opt_id & 0xe0) == 0xa0)
				|| ((opt_id & 0xe0) == 0xe0)
				|| (((opt_id & 0xe0) == 0x40)
					&& (pcfg->lut_mode == SINGLE_MODE))) {
			/*SDR2HDR LUT table and gamma table*/
			for (i = 0; i < pq_ipopt_val->opt_cnt; i++) {
				if ((opt_id & 0xe0)
						== ((pq_ipopt_val->popt[i]) & 0xe0)) {
					old_opt = pq_ipopt_val->popt[i];
					pq_ipopt_val->popt[i] = opt_id;
					cnt = iris_init_update_ipopt_t(popt, len, ip,
							old_opt, opt_id, skip_last);
					return cnt;
				}
			}
		} else { /*DBC LUT table*/
			for (i = 0; i < pq_ipopt_val->opt_cnt; i++) {
				if (((opt_id & 0xe0)
					== ((pq_ipopt_val->popt[i]) & 0xe0))
					&& (((pq_ipopt_val->popt[i]) & 0x1f) != 0)) {

					old_opt = pq_ipopt_val->popt[i];
					pq_ipopt_val->popt[i] = opt_id;

					cnt = iris_init_update_ipopt_t(popt, len, ip,
							old_opt, opt_id, skip_last);
					return cnt;
				}
			}
		}
	}
	for (i = 0; i < pq_ipopt_val->opt_cnt; i++) {
		if ((opt_id & 0xf0) == ((pq_ipopt_val->popt[i]) & 0xf0)) {
			old_opt  = pq_ipopt_val->popt[i];
			pq_ipopt_val->popt[i] = opt_id;
			cnt = iris_init_update_ipopt_t(popt, len, ip,
					old_opt, opt_id, skip_last);
			return cnt;
		}
	}
	return 1;
}

int iris_pcc_set_config(void *cfg_data)
{
	struct quality_setting *pqlt_cur_setting = &iris_setting.quality_cur;
	struct mdp_pcc_cfg_data *pcc_cfg_data = NULL;

	if (!cfg_data) {
		pr_err("invalid params cfg_data %pK\n",
			cfg_data);
		return -EINVAL;
	}
	pcc_cfg_data = (struct mdp_pcc_cfg_data *) cfg_data;

	if (pqlt_cur_setting->pq_setting.sdr2hdr != SDR2HDR_Bypass) {
		if (pqlt_cur_setting->pcc_ops != MDP_PP_OPS_DISABLE) {
			pr_info("Disable pcc: %x", pcc_cfg_data->ops);
			pqlt_cur_setting->pcc_ops = MDP_PP_OPS_DISABLE;
			pcc_cfg_data->ops |= MDP_PP_OPS_DISABLE;
			return 1;
		}
	} else if (pqlt_cur_setting->pcc_ops == MDP_PP_OPS_DISABLE) {
		pr_info("Restore pcc to enable: %x", pcc_cfg_data->ops);
		pcc_cfg_data->ops &= ~(MDP_PP_OPS_DISABLE);
		pqlt_cur_setting->pcc_ops = 0;
		return 1;
	}

	return 0;
}
