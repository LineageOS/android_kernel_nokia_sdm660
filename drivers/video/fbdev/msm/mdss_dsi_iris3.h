#ifndef MDSS_DSI_IRIS3
#define MDSS_DSI_IRIS3

#include "mdss_dsi.h"
#include "mdss_dsi_iris3_def.h"

/* Use Iris3 Analog bypass mode to light up panel
   Note: input timing should be same with output timing
*/
// #define IRIS3_ABYP_LIGHTUP

#define DSI_DMA_TX_BUF_SIZE	SZ_512K
#define IRIS_FIRMWARE_NAME	"iris3.fw"


#define DBC_INCOME_BL_SIZE (256 * 4)

#define DBC_COMPK_BLOCK_SIZE (128 * 4)
#define DBC_COMPK_BLOCK_NUM (2)


#define DBC_CABC_DLV_SIZE (64 * 4)

#define DBC_LEVEL_CNT 4
#define AB_TABLE 2

#define DBC_LUT_SIZE1 \
		(DBC_LEVEL_CNT * \
		(DBC_COMPK_BLOCK_SIZE * \
		DBC_COMPK_BLOCK_NUM + DBC_CABC_DLV_SIZE))

#define DBC_LUT_SIZE (DBC_INCOME_BL_SIZE + DBC_LUT_SIZE1)


#define CM_FW_START_ADDR (DBC_LUT_SIZE)
#define CM_LUT_BLOCK_SIZE (729 * 4)
#define CM_LUT_BLOCK_NUMBER 8
#define CM_LUT_SIZE (CM_LUT_BLOCK_SIZE * 8) /* eight block*/
#define CM_LUT_NUMBER 21
/* table 0,3,6 should store at the same address in iris */
#define CM_LUT_GROUP 3

#define SDR2HDR_FW_START_ADDR (CM_FW_START_ADDR + CM_LUT_SIZE * CM_LUT_NUMBER)
#define SDR2HDR_LUT_BLOCK_SIZE (128 * 4)
#define SDR2HDR_LUT_BLOCK_NUMBER (4 * 6 + 3 * 4)
#define SDR2HDR_LUT2_BLOCK_NUMBER (6)
#define SDR2HDR_LUTUVY_BLOCK_NUMBER (12)

#define SDR2HDR_INV_UV_SIZE (128 * 4 * 2)
#define SDR2HDR_INV_UV_NUMBER 1

/* 0: HDR10In_Ycbcr; 1: HDR10In_Ictcp;
2:IctcpIn_Ycbcr; 3: 709->709;
4: 709->p3;  5: 709->2020; */
#define SDR2HDR_LUT_GROUP_CNT 6

#define SDR2HDR_GROUP_SIZE \
	(SDR2HDR_LUT_BLOCK_SIZE * SDR2HDR_LUT_BLOCK_NUMBER \
	+ SDR2HDR_INV_UV_SIZE * SDR2HDR_INV_UV_NUMBER)


#define SCALER1D_FW_START_ADDR \
	(SDR2HDR_FW_START_ADDR + SDR2HDR_GROUP_SIZE * SDR2HDR_LUT_GROUP_CNT)

#define SCALER1D_LUT_SIZE (33 * 3 * 4)
/* scaler1d lut format is special*/
#define SCALER1D_LUT_BLOCK_SIZE (33 * 4)
#define SCALER1D_LUT_NUMBER 9
#define SCALER_LUT_BLOCK_NUMBER (3 * 2)

#define GAMMA_FW_START_ADDR \
	(SCALER1D_FW_START_ADDR + \
	SCALER1D_LUT_SIZE * SCALER1D_LUT_NUMBER)

#define GAMMA_LUT_SIZE (33 * 3 * 4)
#define GAMMA_LUT_NUMBER 8

#define PANEL_NITS_FW_START_ADDR \
	(GAMMA_FW_START_ADDR + \
	GAMMA_LUT_SIZE * GAMMA_LUT_NUMBER)
#define PANEL_NITS_SIZE (2)

#define IRIS_FW_SIZE \
	(GAMMA_FW_START_ADDR + \
	GAMMA_LUT_SIZE * GAMMA_LUT_NUMBER + \
	PANEL_NITS_SIZE)

/*move begin: the following define from lut.c to here*/

#define DIRECT_BUS_HEADER_SIZE 8
#define CM_LUT_ADDRESS 0x8000
#define CM_LUT_BLOCK_ADDRESS_INC 0x3000

#define SDR2HDR_LUT_ADDRESS 0x0
#define SDR2HDR_LUT_BLOCK_ADDRESS_INC 0x400
#define SDR2HDR_INV_UV_ADDRESS 0xF15A1400
#define SDR2HDR_LUT2_ADDRESS 0x3000

#define SDR2HDR_LUTUVY_ADDRESS 0x6000

#define SCALER1D_LUT_ADDRESS 0x0
#define SCALER1D_H_Y_LOW 0x0
#define SCALER1D_H_Y_HIGH 0x400
#define SCALER1D_H_UV_LOW 0x100
#define SCALER1D_H_UV_HIGH 0x500
#define SCALER1D_V_Y 0x200
#define SCALER1D_V_UV 0x300

/* for gamma */
#define GAMMA_REG_ADDRESS 0xF0580040
#define GAMMA_LUT_LENGTH (198)

/*for ambient light lut*/
#define SDR2HDR_LUT2_BLOCK_CNT (6)  /* for SDR2HDR*/

/*for maxcll lut*/
#define SDR2HDR_LUTUVY_BLOCK_CNT (12)

#define LUT_LEN 256


#ifdef PXLW_IRIS3_FPGA
#define LUT_CMD_WAIT_MS 0
#else
#define LUT_CMD_WAIT_MS 0
#endif


/*move end:*/

enum DBC_LUT_ADDR {
	INCOME_BL_A = 0x1800,
	INCOME_BL_B = 0x1C00,
	COMPK_DLV_A_0 = 0x800,
	COMPK_DLV_A_1 = 0xC00,
	COMPK_DLV_B_0 = 0x1000,
	COMPK_DLV_B_1 = 0x1400,
	CABC_DLV_0 = 0xF1540304,
};

enum DBC_LEVEL {
	DBC_INIT = 0,
	DBC_OFF,
	DBC_LOW,
	DBC_MIDDLE,
	DBC_HIGH,
};


enum LUT_TYPE {
	DBC_LUT = 1,
	CM_LUT,
	SDR2HDR_LUT,
	SCALER1D_LUT,
	AMBINET_HDR_GAIN, /*HDR case*/
	AMBINET_SDR2HDR_LUT, /*SDR2HDR case;*/
	GAMMA_LUT,
};
enum result {
	IRIS_FAILED = -1,
	IRIS_SUCCESS = 0,
};

enum PANEL_TYPE {
	PANEL_LCD_SRGB = 0,
	PANEL_LCD_P3,
	PANEL_OLED,
};

enum LUT_MODE {
	INTERPOLATION_MODE = 0,
	SINGLE_MODE,
};

enum {
	IRIS_CONT_SPLASH_LK = 1,
	IRIS_CONT_SPLASH_KERNEL,
	IRIS_CONT_SPLASH_NONE,
};

struct ocp_header {
	u32 header;
	u32 address;
};
void iris_dump_packet(u8 *data, int size);
int iris_parse_lut_cmds(const char *name);

int iris_lut_send
		(u8 lut_type, u8 lut_table_index,
		u32 lut_pkt_index, bool bSendFlag);
void mdss_dsi_iris_init(struct msm_fb_data_type *mfd);

/*
* @ Description: this function will update lut payload,
it will change the [2...n] payload value
*@param ip   ip value which will be our ip enum.
*@param opt  ip options
*@param pcmd cmds which have payload and dchdr descriptor
*@return 0 OK, !0 not OK
*/
int32_t iris_update_lut_payload(
		int32_t ip, int32_t opt_id,
		struct dsi_panel_cmds *pcmds);
void iris_ambient_lut_update(enum LUT_TYPE lutType);
void iris_maxcll_lut_update(enum LUT_TYPE lutType);
int iris_gamma_lut_update(u32 lut_table_index, const u32 *fw_data);
int iris_cm_lut_read(u32 lut_table_index, const u8 *fw_data);
int iris3_debugfs_init(struct msm_fb_data_type *mfd);
int iris_pq_debugfs_init(struct msm_fb_data_type *mfd);

/*
* @Description: send continuous splash commands
* @param type IRIS_CONT_SPLASH_LK/IRIS_CONT_SPLASH_KERNEL
*/
void iris_send_cont_splash_pkt(uint32_t type);

/*
* @Description: get mfd handler
* @return mfd handler
*/
struct msm_fb_data_type *mdss_dsi_get_mfd_handler(void);
/*
* @Description: debug cont_splash init
* @param: mfd handler
*/
int iris_cont_splash_debugfs_init(struct msm_fb_data_type *mfd);

/*
*@Description: get current continue splash stage
				first light up panel only
				second pq effect
*/
uint8_t iris_get_cont_splash_type(void);

/*
*@Description: print continuous splash commands for bootloader
*@param: pcmd: cmds array  cnt: cmds cound
*/
void  iris_print_cmds(struct dsi_cmd_desc *pcmd, int cnt);


/* set iris low power */
void iris_lp_set(void);

void iris_set_lut_cnt(uint32_t cnt);

/*alloc space for seq space*/
void iris_alloc_seq_space(void);

void iris_set_cfg_name(const uint8_t *name);

void iris_add_vblank(uint32_t *vblank);

u8 iris_get_dbc_lut_index(void);

struct iris_setting_info *iris_get_setting(void);

void iris_set_yuv_input(bool val);

void iris_set_HDR10_YCoCg(bool val);

u32 iris_get_csc_type(u32 type);

void iris_init_HDRchange(void);

bool iris_has_HDRchange(void);

void iris_add_HDRchange(void);

bool iris_get_debug_cap(void);

void iris_set_debug_cap(bool val);

struct msmfb_iris_ambient_info *iris_get_ambient_lut(void);

void iris_set_cont_splash(bool enable);

void iris_set_bklt_ctrl(struct led_trigger *bl_led);

void iris_display_prepare(void);

bool iris_is_valid_cfg(void);

int iris_pcc_set_config(void *cfg_data);

#endif
