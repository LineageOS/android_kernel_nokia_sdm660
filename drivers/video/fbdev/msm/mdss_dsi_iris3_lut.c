#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/msm_mdp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/sw_sync.h>
#include <linux/kmemleak.h>
#include <linux/firmware.h>

#include <soc/qcom/event_timer.h>
#include <linux/msm-bus.h>
#include "mdss.h"
#include "mdss_debug.h"
#include "mdss_mdp.h"
#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "mdss_smmu.h"
#include "mdss_mdp_wfd.h"
#include "mdss_dsi_clk.h"
#include "mdss_dsi_cmd.h"

#include "mdss_dsi_iris3_def.h"
#include "mdss_dsi_iris3_ioctl.h"
#include "mdss_dsi_iris3.h"
#include "mdss_dsi_iris3_lightup_ocp.h"
#include "mdss_dsi_iris3_lightup.h"

enum {
	IRIS_SEND_LUT_BY_CMDLIST,
	IRIS_SEND_LUT_BY_DMA,
};

struct msm_fb_data_type *g_mfd;


void mdss_dsi_iris_init(struct msm_fb_data_type *mfd)
{
	pr_info("###%s:%d: mfd->panel.type: %i mfd->panel.id: %i\n",
		__func__, __LINE__, mfd->panel.type, mfd->panel.id);

	g_mfd = mfd;
	iris3_debugfs_init(mfd);
	iris_pq_debugfs_init(mfd);
	iris_cont_splash_debugfs_init(mfd);
}

struct msm_fb_data_type *mdss_dsi_get_mfd_handler(void)
{
	return g_mfd;
}

void iris_dump_packet(u8 *data, int size)
{
	char temp[64];
	int len = 64;
	int i, j;

	pr_err("size: 0x%02X\n", size);
	len -= 1;
	for (i = 0; i < size; i += 4) {
		snprintf(temp, len, "0x%02x,", *(data+i));
		len -= 1;
		for (j = 1; j < 4 && (i + j) < size; j++) {
			snprintf(temp + 5 + 6 * (j - 1), len,
				" 0x%02x,", *(data + i + j));
			len -= 1;
		}
		pr_err("%s\n", temp);
	}
}

static u8 payload_size;

static u8 *iris_dbc_lut_buf = NULL;
static u8 *iris_cm_lut_buf = NULL;
static u8 *iris_sdr2hdr_lut_buf = NULL;
static u8 *iris_scaler1d_lut_buf = NULL;
static u8 *iris_gamma_lut_buf = NULL;

static struct dsi_cmd_desc *cm_lut_send_cmd = NULL;
static struct dsi_cmd_desc *scaler1d_lut_send_cmd = NULL;
static struct dsi_cmd_desc *sdr2hdr_lut_send_cmd = NULL;
static struct dsi_cmd_desc *sdr2hdr_inv_uv_send_cmd = NULL;/*write the PI*/



static struct dsi_cmd_desc *dbc_lut_income_send_cmd = NULL;
static struct dsi_cmd_desc *dbc_lut_compk_send_cmd = NULL;
static struct dsi_cmd_desc *dbc_lut_cabc_dlv_send_cmd = NULL;

static struct dsi_cmd_desc *gamma_lut_send_cmd = NULL;


static struct dsi_panel_cmds panel_lut_cmds;

static struct msmfb_iris_ambient_info iris_ambient_lut;
static uint32_t lut_lut2[LUT_LEN] = {};
static uint32_t LUT2_fw[LUT_LEN+LUT_LEN+LUT_LEN] = {};

static u8 *iris_ambient_lut_buf = NULL;
/* SDR2HDR_UVYGAIN_BLOCK_CNT > SDR2HDR_LUT2_BLOCK_CNT */
static struct dsi_cmd_desc *dynamic_lut_send_cmd = NULL;

struct msmfb_iris_maxcll_info iris_maxcll_lut;
static uint32_t lut_luty[LUT_LEN] = {};
static uint32_t lut_lutuv[LUT_LEN] = {};
static uint32_t LUTUVY_fw[LUT_LEN+LUT_LEN+LUT_LEN+LUT_LEN+LUT_LEN+LUT_LEN] = {};

static u8 *iris_maxcll_lut_buf = NULL;
static struct dsi_cmd_desc *dynamic_lutuvy_send_cmd = NULL;

struct lut_node {
	u32 dbc_income_pkt_cnt;
	u32 dbc_compk_pkt_cnt;
	u32 dbc_dlv_pkt_cnt;

	u32 cm_one_lut_pkt_cnt;

	u32 hdr_one_lut_pkt_cnt;
	u32 uv_one_lut_pkt_cnt;

	u32 scaler_one_lut_pkt_cnt;

	u32 gamma_one_lut_pkt_cnt;

	u32 lut_cmd_cnts_max;

	u32 hdr_lut2_pkt_cnt;

	u32 hdr_lutuvy_pkt_cnt;
};

static struct lut_node iris_lut_param;

struct msmfb_iris_ambient_info *iris_get_ambient_lut(void)
{
	return &iris_ambient_lut;
}

static void iris_ambient_lut_init(void)
{
	iris_ambient_lut.ambient_lux = 0;
	iris_ambient_lut.ambient_bl_ratio = 0;
	iris_ambient_lut.lut_lut2_payload = &lut_lut2;

	kfree(iris_ambient_lut_buf);
	iris_ambient_lut_buf = NULL;
	kfree(dynamic_lut_send_cmd);
	dynamic_lut_send_cmd = NULL;
}

static void iris_maxcll_lut_init(void)
{
	iris_maxcll_lut.mMAXCLL= 2200;
	iris_maxcll_lut.lut_luty_payload = &lut_luty;
	iris_maxcll_lut.lut_lutuv_payload = &lut_lutuv;

	kfree(iris_maxcll_lut_buf);
	iris_maxcll_lut_buf = NULL;
	kfree(dynamic_lutuvy_send_cmd);
	dynamic_lutuvy_send_cmd = NULL;
}

static void iris_lut_out_cmds_init(void)
{
	/* sent out cmds
	iris_out_cmds_buf_reset();*/

	memset(&panel_lut_cmds, 0x00, sizeof(panel_lut_cmds));
	panel_lut_cmds.link_state = DSI_HS_MODE;
}

static void iris_lut_buf_init(void)
{

	struct iris_cfg *pcfg = NULL;

	pcfg = iris_get_cfg();
	payload_size = pcfg->split_pkt_size;

	kfree(iris_dbc_lut_buf);
	iris_dbc_lut_buf = NULL;
	kfree(iris_cm_lut_buf);
	iris_cm_lut_buf = NULL;
	kfree(iris_sdr2hdr_lut_buf);
	iris_sdr2hdr_lut_buf = NULL;
	kfree(iris_scaler1d_lut_buf);
	iris_scaler1d_lut_buf = NULL;
	kfree(iris_gamma_lut_buf);
	iris_gamma_lut_buf = NULL;

	kfree(dbc_lut_income_send_cmd);
	dbc_lut_income_send_cmd = NULL;
	kfree(dbc_lut_compk_send_cmd);
	dbc_lut_compk_send_cmd = NULL;
	kfree(dbc_lut_cabc_dlv_send_cmd);
	dbc_lut_cabc_dlv_send_cmd = NULL;
	kfree(cm_lut_send_cmd);
	cm_lut_send_cmd = NULL;
	kfree(sdr2hdr_lut_send_cmd);
	sdr2hdr_lut_send_cmd = NULL;
	kfree(sdr2hdr_inv_uv_send_cmd);
	sdr2hdr_inv_uv_send_cmd = NULL;
	kfree(scaler1d_lut_send_cmd);
	scaler1d_lut_send_cmd = NULL;
	kfree(gamma_lut_send_cmd);
	gamma_lut_send_cmd = NULL;

	memset(&iris_lut_param, 0x00, sizeof(iris_lut_param));

	iris_lut_out_cmds_init();

	/* for HDR ambient light */
	iris_ambient_lut_init();

	/* for HDR maxcll */
	iris_maxcll_lut_init();
}

static int iris_dbc_lut_read(const u8 *fw_data)
{
	u32 len = 0;
	u32 pos = 0;
	u32 level_index = 0, block_index = 0, lut_pkt_cmd_index = 0;

	u32 iris_lut_buf_index = 0, cmd_payload_len = 0;

	u32 ucmds_cnt, ab_table_index = 0, temp_index = 0, temp_index2 = 0;
	u32 addr_offset = 0;

	u32 dbc_payload_size = payload_size;
	u32 dbc_pkt_size = dbc_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 dbc_income_pkt_cnt =
		(DBC_INCOME_BL_SIZE + dbc_payload_size - 1) / dbc_payload_size;

	u32 dbc_compk_block_pkt_cnt =
		(DBC_COMPK_BLOCK_SIZE + dbc_payload_size - 1)
			/ dbc_payload_size;
	u32 dbc_compk_pkt_cnt = dbc_compk_block_pkt_cnt * DBC_COMPK_BLOCK_NUM;

	u32 dbc_dlv_pkt_cnt =
		(DBC_CABC_DLV_SIZE + dbc_payload_size - 1)
			/ dbc_payload_size;

	struct ocp_header ocp_burst_header;

	memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
	ocp_burst_header.header = 0x0020000C;
	ocp_burst_header.address = INCOME_BL_A;


	if (dbc_lut_income_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc) * AB_TABLE * dbc_income_pkt_cnt;
		dbc_lut_income_send_cmd = kzalloc(len, GFP_KERNEL);

		iris_lut_param.lut_cmd_cnts_max +=
				AB_TABLE * dbc_income_pkt_cnt;
		iris_lut_param.dbc_income_pkt_cnt = dbc_income_pkt_cnt;

		pr_err("%s, iris_lut_param.lut_cmd_cnts_max = %d\n",
				__func__, iris_lut_param.lut_cmd_cnts_max);
	}

	if (dbc_lut_compk_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc) * AB_TABLE
				* dbc_compk_pkt_cnt * DBC_LEVEL_CNT;
		dbc_lut_compk_send_cmd = kzalloc(len, GFP_KERNEL);

		iris_lut_param.lut_cmd_cnts_max +=
				AB_TABLE * dbc_compk_pkt_cnt;
		iris_lut_param.dbc_compk_pkt_cnt = dbc_compk_pkt_cnt;
	}
	if (dbc_lut_cabc_dlv_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
				* dbc_dlv_pkt_cnt * DBC_LEVEL_CNT;
		dbc_lut_cabc_dlv_send_cmd = kzalloc(len, GFP_KERNEL);

		iris_lut_param.lut_cmd_cnts_max += dbc_dlv_pkt_cnt;
		iris_lut_param.dbc_dlv_pkt_cnt = dbc_dlv_pkt_cnt;
	}

	ucmds_cnt = AB_TABLE * dbc_income_pkt_cnt
			+ AB_TABLE * DBC_LEVEL_CNT * dbc_compk_pkt_cnt
			+ DBC_LEVEL_CNT * dbc_dlv_pkt_cnt;

	if (iris_dbc_lut_buf == NULL)
		iris_dbc_lut_buf =
			kzalloc(dbc_pkt_size * ucmds_cnt, GFP_KERNEL);

	if (!dbc_lut_income_send_cmd
			|| !dbc_lut_compk_send_cmd
			|| !dbc_lut_cabc_dlv_send_cmd
			|| !iris_dbc_lut_buf) {
		pr_err("%s:failed alloc mem: lut_cmd: %p, :%p, :%p buf: %p\n",
				__func__, dbc_lut_income_send_cmd,
				dbc_lut_compk_send_cmd,
				dbc_lut_cabc_dlv_send_cmd,
				iris_dbc_lut_buf);

		return IRIS_FAILED;
	}

	/*parse income bl*/

	addr_offset = 0;
	for (lut_pkt_cmd_index = 0; lut_pkt_cmd_index < dbc_income_pkt_cnt;
			lut_pkt_cmd_index++) {

		if (lut_pkt_cmd_index == dbc_income_pkt_cnt-1)
			cmd_payload_len = DBC_INCOME_BL_SIZE
				- (dbc_income_pkt_cnt-1) * dbc_payload_size;
		else
			cmd_payload_len = dbc_payload_size;

		for (ab_table_index = 0; ab_table_index < AB_TABLE;
				ab_table_index++) {
			pos = ab_table_index * dbc_income_pkt_cnt
					+ lut_pkt_cmd_index;
			dbc_lut_income_send_cmd[pos].dchdr.dtype = 0x29;
			dbc_lut_income_send_cmd[pos].dchdr.dlen =
				cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
			dbc_lut_income_send_cmd[pos].dchdr.wait =
				LUT_CMD_WAIT_MS;
			dbc_lut_income_send_cmd[pos].payload =
				iris_dbc_lut_buf + iris_lut_buf_index;

			ocp_burst_header.address = (ab_table_index == 0)
				? INCOME_BL_A : INCOME_BL_B;
			ocp_burst_header.address += addr_offset;
			memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
				&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
			memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
				fw_data, cmd_payload_len);
			iris_lut_buf_index += cmd_payload_len;
		}
		addr_offset += cmd_payload_len;
		fw_data += cmd_payload_len;

	}


	/*parse compk*/
	for (level_index = 0; level_index < DBC_LEVEL_CNT; level_index++) {

		ocp_burst_header.header = 0x0020000C;

		for (block_index = 0; block_index < DBC_COMPK_BLOCK_NUM;
				block_index++){

			addr_offset = 0;
			for (lut_pkt_cmd_index = 0;
					lut_pkt_cmd_index < dbc_compk_block_pkt_cnt;
							lut_pkt_cmd_index++) {

				if (lut_pkt_cmd_index == dbc_compk_block_pkt_cnt-1)
					cmd_payload_len = DBC_COMPK_BLOCK_SIZE
						- (dbc_compk_block_pkt_cnt-1)
							* dbc_payload_size;
				else
					cmd_payload_len = dbc_payload_size;

				for (ab_table_index = 0;
						ab_table_index < AB_TABLE;
							ab_table_index++) {

					temp_index =
						(level_index * AB_TABLE + ab_table_index)
							* dbc_compk_pkt_cnt;
					temp_index2 = block_index
						* dbc_compk_block_pkt_cnt
							+ lut_pkt_cmd_index;

					pos = temp_index + temp_index2;
					dbc_lut_compk_send_cmd[pos].dchdr.dtype = 0x29;
					dbc_lut_compk_send_cmd[pos].dchdr.dlen =
						cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
					dbc_lut_compk_send_cmd[pos].payload =
						iris_dbc_lut_buf + iris_lut_buf_index;

					if (block_index == 0)
						ocp_burst_header.address
							= (ab_table_index == 0)
								? COMPK_DLV_A_0 : COMPK_DLV_B_0;
					else
						ocp_burst_header.address
							= (ab_table_index == 0)
								? COMPK_DLV_A_1 : COMPK_DLV_B_1;

					ocp_burst_header.address += addr_offset;

					memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
						&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);

					iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
					memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
						fw_data, cmd_payload_len);
					iris_lut_buf_index += dbc_payload_size;

				}
				addr_offset += cmd_payload_len;
				fw_data += cmd_payload_len;

			}
		}

		/*use ocp burst to write IP*/
		memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
		ocp_burst_header.header = 0x00000000;
		ocp_burst_header.address = CABC_DLV_0;

		/*parse CABC_DLV*/
		{
			for (lut_pkt_cmd_index = 0;
				lut_pkt_cmd_index < dbc_dlv_pkt_cnt;
					lut_pkt_cmd_index++) {

				if (lut_pkt_cmd_index == dbc_dlv_pkt_cnt - 1)
					cmd_payload_len = DBC_CABC_DLV_SIZE
							- (dbc_dlv_pkt_cnt-1) * dbc_payload_size;
				else
					cmd_payload_len = dbc_payload_size;

				pos = level_index*dbc_dlv_pkt_cnt + lut_pkt_cmd_index;
				dbc_lut_cabc_dlv_send_cmd[pos].dchdr.dtype = 0x29;
				dbc_lut_cabc_dlv_send_cmd[pos].dchdr.dlen =
					cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
				dbc_lut_cabc_dlv_send_cmd[pos].dchdr.wait =
					LUT_CMD_WAIT_MS;
				dbc_lut_cabc_dlv_send_cmd[pos].payload =
					iris_dbc_lut_buf + iris_lut_buf_index;

				memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
					&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);
				iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
				memcpy(&iris_dbc_lut_buf[iris_lut_buf_index],
					fw_data, cmd_payload_len);
				iris_lut_buf_index += dbc_payload_size;

				fw_data += cmd_payload_len;
			}
		}

	}

	return IRIS_SUCCESS;
}


int iris_cm_lut_read(u32 lut_table_index, const u8 *fw_data)
{
	u32 len = 0;
	u32 pos = 0;
	u32 lut_pkt_cmd_index = 0;

	u32 iris_lut_buf_index, cmd_payload_len = 0;

	u32 cm_pkt_size = payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 cm_block_pkt_cnt =
			(CM_LUT_BLOCK_SIZE + payload_size - 1) / payload_size;
	u32 cm_one_lut_pkt_cnt = cm_block_pkt_cnt * CM_LUT_BLOCK_NUMBER;

	struct ocp_header ocp_burst_header;
	struct iris_cfg *pcfg = iris_get_cfg();

	memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
	ocp_burst_header.header = 0x0008000C;
	ocp_burst_header.address = CM_LUT_ADDRESS;

	if (iris_cm_lut_buf == NULL) {
		len = cm_pkt_size * cm_one_lut_pkt_cnt * CM_LUT_NUMBER;
		iris_cm_lut_buf = kzalloc(len, GFP_KERNEL);
	}

	if (cm_lut_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
				* cm_one_lut_pkt_cnt * CM_LUT_NUMBER;
		cm_lut_send_cmd = kzalloc(len, GFP_KERNEL);
		iris_lut_param.lut_cmd_cnts_max +=
				cm_one_lut_pkt_cnt * CM_LUT_GROUP;
		iris_lut_param.cm_one_lut_pkt_cnt = cm_one_lut_pkt_cnt;

		pr_err("%s, iris_lut_param.lut_cmd_cnts_max = %d\n",
				__func__, iris_lut_param.lut_cmd_cnts_max);
	}

	if (!cm_lut_send_cmd || !iris_cm_lut_buf) {
		pr_err("%s: failed to alloc mem: lut_cmd: %p, buf: %p\n",
				__func__, cm_lut_send_cmd, iris_scaler1d_lut_buf);
		return IRIS_FAILED;
	}


	pr_debug("%s:cm_block_pkt_cnt=%d", __func__, cm_block_pkt_cnt);
	pr_debug("cm_one_lut_pkt_cnt=%d lut_table_index=%d\n",
			cm_one_lut_pkt_cnt, lut_table_index);

	{
		if (pcfg->lut_mode == INTERPOLATION_MODE)
			ocp_burst_header.address = CM_LUT_ADDRESS
				+ (lut_table_index % CM_LUT_GROUP) * CM_LUT_BLOCK_SIZE;
		else
			ocp_burst_header.address = CM_LUT_ADDRESS
				+ (lut_table_index / CM_LUT_GROUP) * CM_LUT_BLOCK_SIZE;
		for (lut_pkt_cmd_index = 0;
				lut_pkt_cmd_index < cm_one_lut_pkt_cnt;
						lut_pkt_cmd_index++) {

			iris_lut_buf_index =
				lut_table_index * cm_pkt_size * cm_one_lut_pkt_cnt
					+ lut_pkt_cmd_index * cm_pkt_size;

			/* need update ocp_burst_header.address here */
			if ((lut_pkt_cmd_index % cm_block_pkt_cnt) == 0) {
				if (pcfg->lut_mode == INTERPOLATION_MODE)
					ocp_burst_header.address = CM_LUT_ADDRESS
							+ (lut_pkt_cmd_index / cm_block_pkt_cnt)
							* CM_LUT_BLOCK_ADDRESS_INC
							+ (lut_table_index%CM_LUT_GROUP)
							* CM_LUT_BLOCK_SIZE;
				else
					ocp_burst_header.address = CM_LUT_ADDRESS
							+ (lut_pkt_cmd_index / cm_block_pkt_cnt)
							* CM_LUT_BLOCK_ADDRESS_INC
							+ (lut_table_index/CM_LUT_GROUP)
							* CM_LUT_BLOCK_SIZE;
				}

			if (CM_LUT_BLOCK_SIZE >=
					((lut_pkt_cmd_index % cm_block_pkt_cnt + 1)
						* payload_size))
				cmd_payload_len = payload_size;
			else/* last pkt may less than payload_size*/
				cmd_payload_len = CM_LUT_BLOCK_SIZE
						- (cm_block_pkt_cnt-1) * payload_size;

			pos = lut_pkt_cmd_index + cm_one_lut_pkt_cnt * lut_table_index;
			cm_lut_send_cmd[pos].dchdr.dtype = 0x29;
			cm_lut_send_cmd[pos].dchdr.dlen =
				cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
			cm_lut_send_cmd[pos].dchdr.wait = LUT_CMD_WAIT_MS;
			cm_lut_send_cmd[pos].payload =
				iris_cm_lut_buf + iris_lut_buf_index;

			pr_debug("%s: lut_table_index=%d lut_pkt_cmd_index=%d",
					__func__, lut_table_index, lut_pkt_cmd_index);
			pr_debug("ocp_burst_header address=%x header=%x\n",
				ocp_burst_header.address, ocp_burst_header.header);
			memcpy(&iris_cm_lut_buf[iris_lut_buf_index],
				&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;

			memcpy(&iris_cm_lut_buf[iris_lut_buf_index],
				fw_data, cmd_payload_len);

			fw_data += cmd_payload_len;
			ocp_burst_header.address += cmd_payload_len;
		}
	}

	return IRIS_SUCCESS;
}

static void iris_cm_lut_check(const u8 *fw_data)
{

	u32 lut_table_index = 0;

	for (lut_table_index = 0; lut_table_index < CM_LUT_NUMBER;
			lut_table_index++)
		iris_cm_lut_read(lut_table_index,
				(fw_data + CM_FW_START_ADDR
				+ lut_table_index * CM_LUT_SIZE));
}

static int iris_sdr2hdr_lut_read(
		u32 lut_group_index, const u8 *fw_data)
{
	u32 len = 0;
	u32 pos = 0;
	u32 lut_block_index = 0, lut_pkt_cmd_index = 0;
	u32 temp_index = 0;
	u32 iris_lut_buf_index, cmd_payload_len = 0;

	u32 hdr_payload_size = payload_size;
	u32 hdr_pkt_size = hdr_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 hdr_block_pkt_cnt =
		(SDR2HDR_LUT_BLOCK_SIZE + hdr_payload_size - 1)
			/ hdr_payload_size;
	u32 hdr_one_lut_pkt_cnt = hdr_block_pkt_cnt * SDR2HDR_LUT_BLOCK_NUMBER;

	u32 uv_block_pkt_cnt =
			(SDR2HDR_INV_UV_SIZE + hdr_payload_size - 1)
			/ hdr_payload_size;
	u32 uv_one_lut_pkt_cnt = uv_block_pkt_cnt * SDR2HDR_INV_UV_NUMBER;

	u32 one_group_size =
		hdr_pkt_size * (hdr_one_lut_pkt_cnt + uv_one_lut_pkt_cnt);

	struct ocp_header ocp_dbus_header;
	struct ocp_header ocp_burst_header;

	memset(&ocp_dbus_header, 0, sizeof(ocp_dbus_header));
	ocp_dbus_header.header = 0x0004000C;
	ocp_dbus_header.address = SDR2HDR_LUT_ADDRESS;


	if (sdr2hdr_lut_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
			 * hdr_one_lut_pkt_cnt * SDR2HDR_LUT_GROUP_CNT;
		sdr2hdr_lut_send_cmd = kzalloc(len, GFP_KERNEL);

		iris_lut_param.lut_cmd_cnts_max += hdr_one_lut_pkt_cnt;
		iris_lut_param.hdr_one_lut_pkt_cnt = hdr_one_lut_pkt_cnt;

		pr_err("%s, iris_lut_param.lut_cmd_cnts_max = %d\n",
				__func__, iris_lut_param.lut_cmd_cnts_max);
	}

	if (sdr2hdr_inv_uv_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
				* uv_one_lut_pkt_cnt * SDR2HDR_LUT_GROUP_CNT;
		sdr2hdr_inv_uv_send_cmd = kzalloc(len, GFP_KERNEL);
		iris_lut_param.lut_cmd_cnts_max += uv_one_lut_pkt_cnt;
		iris_lut_param.uv_one_lut_pkt_cnt = uv_one_lut_pkt_cnt;
	}

	if (iris_sdr2hdr_lut_buf == NULL) {
		len = one_group_size * SDR2HDR_LUT_GROUP_CNT;
		iris_sdr2hdr_lut_buf = kzalloc(len, GFP_KERNEL);
	}

	if (!iris_sdr2hdr_lut_buf
			|| !sdr2hdr_lut_send_cmd
			|| !sdr2hdr_inv_uv_send_cmd) {
		pr_err("%s: failed to alloc mem:", __func__);
		pr_err("lut_cmd: %p, uv_cmd: %p, buf: %p\n",
				sdr2hdr_lut_send_cmd,
				sdr2hdr_inv_uv_send_cmd,
				iris_sdr2hdr_lut_buf);
		return IRIS_FAILED;
	}

	{
		/*use DB to write*/
		ocp_dbus_header.header = 0x0004000C;
		ocp_dbus_header.address = SDR2HDR_LUT_ADDRESS;
		/*parse LUT0,1,2,3 and UVY0,1,2*/
		for (lut_block_index = 0;
			lut_block_index < SDR2HDR_LUT_BLOCK_NUMBER;
				lut_block_index++){

			ocp_dbus_header.address =
					lut_block_index * SDR2HDR_LUT_BLOCK_ADDRESS_INC;
			for (lut_pkt_cmd_index = 0;
				lut_pkt_cmd_index < hdr_block_pkt_cnt;
					lut_pkt_cmd_index++) {

				iris_lut_buf_index =
					lut_group_index * one_group_size;
				iris_lut_buf_index +=
					lut_block_index * hdr_pkt_size
						* hdr_block_pkt_cnt
					+ lut_pkt_cmd_index * hdr_pkt_size;

				if (lut_pkt_cmd_index == hdr_block_pkt_cnt-1)
					cmd_payload_len = SDR2HDR_LUT_BLOCK_SIZE
						- (hdr_block_pkt_cnt - 1) * hdr_payload_size;
				else
					cmd_payload_len = hdr_payload_size;

				temp_index = lut_pkt_cmd_index
					+ hdr_block_pkt_cnt * lut_block_index;

				pos = lut_group_index * hdr_one_lut_pkt_cnt
						+ temp_index;
				sdr2hdr_lut_send_cmd[pos].dchdr.dtype = 0x29;
				sdr2hdr_lut_send_cmd[pos].dchdr.dlen =
					cmd_payload_len
						+ DIRECT_BUS_HEADER_SIZE;
				sdr2hdr_lut_send_cmd[pos].dchdr.wait = 0;
				sdr2hdr_lut_send_cmd[pos].payload =
					iris_sdr2hdr_lut_buf
						+ iris_lut_buf_index;

				pr_debug("%s:lut_table_index=%d, iris_lut_buf_index=%d",
					__func__, lut_block_index,
					iris_lut_buf_index);
				pr_debug("temp_index =%d, ocp_dbus_header=%x header=%x\n",
					temp_index, ocp_dbus_header.address,
					ocp_dbus_header.header);

				memcpy(
				&iris_sdr2hdr_lut_buf[iris_lut_buf_index],
					&ocp_dbus_header,
						DIRECT_BUS_HEADER_SIZE);

				iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
				memcpy(
				&iris_sdr2hdr_lut_buf[iris_lut_buf_index],
					fw_data, cmd_payload_len);

				fw_data += cmd_payload_len;
				ocp_dbus_header.address += cmd_payload_len;

			}
		}
		/*sdr2hdr_lut_send_cmd[lut_group_index]
				[SDR2HDR_LUT_CMD_CNT_TOTAL/2 -1].dchdr.last = 1;
		//to fix AP(8096 1080p*15fps) underflow issue
		sdr2hdr_lut_send_cmd[lut_group_index]
		[SDR2HDR_LUT_CMD_CNT_TOTAL -1].dchdr.last = 1;*/

		/*use ocp burst to write IP*/
		memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
		ocp_burst_header.header = 0x00000000;
		ocp_burst_header.address = SDR2HDR_INV_UV_ADDRESS;

		/*parse INV_UV0,1*/
		for (lut_block_index = 0;
			lut_block_index < SDR2HDR_INV_UV_NUMBER;
				lut_block_index++){

			for (lut_pkt_cmd_index = 0;
				lut_pkt_cmd_index < uv_block_pkt_cnt;
					lut_pkt_cmd_index++) {

				iris_lut_buf_index =
					lut_group_index * one_group_size;
				iris_lut_buf_index +=
					hdr_one_lut_pkt_cnt * hdr_pkt_size;
				iris_lut_buf_index +=
					lut_pkt_cmd_index * hdr_pkt_size
					+ lut_block_index * hdr_pkt_size
						* uv_block_pkt_cnt;

				if (lut_pkt_cmd_index == uv_block_pkt_cnt-1)
					cmd_payload_len = SDR2HDR_INV_UV_SIZE
						- (uv_block_pkt_cnt-1) * hdr_payload_size;
				else
					cmd_payload_len = hdr_payload_size;

				temp_index = lut_pkt_cmd_index
					+ uv_block_pkt_cnt * lut_block_index;

				pos = lut_group_index * uv_one_lut_pkt_cnt
						+ temp_index;
				sdr2hdr_inv_uv_send_cmd[pos].dchdr.dtype = 0x29;
				sdr2hdr_inv_uv_send_cmd[pos].dchdr.dlen =
					cmd_payload_len
					+ DIRECT_BUS_HEADER_SIZE;

				sdr2hdr_inv_uv_send_cmd[pos].payload =
					iris_sdr2hdr_lut_buf
					+ iris_lut_buf_index;

				pr_debug("%s: lut_table_index=%d, iris_lut_buf_index=%d",
					__func__, lut_block_index,
						iris_lut_buf_index);
				pr_debug("temp_index=%d, ocp_burst_header=%x header=%x\n",
					temp_index, ocp_burst_header.address,
						ocp_burst_header.header);
				memcpy(
				&iris_sdr2hdr_lut_buf[iris_lut_buf_index],
					&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);

				iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
				memcpy(
				&iris_sdr2hdr_lut_buf[iris_lut_buf_index],
						fw_data, cmd_payload_len);

				fw_data += cmd_payload_len;
				ocp_burst_header.address += cmd_payload_len;

				/*iris_dump_packet(
				&iris_sdr2hdr_lut_buf[iris_lut_buf_index]
				, LUT_ONE_PKT_SIZE);*/

			}
		}
	}
	return IRIS_SUCCESS;

}

static void iris_sdr2hdr_lut_check(const u8 *fw_data)
{

	u32 lut_table_index = 0;

	for (lut_table_index = 0; lut_table_index < SDR2HDR_LUT_GROUP_CNT;
			lut_table_index++)
		iris_sdr2hdr_lut_read(lut_table_index,
			(fw_data + SDR2HDR_FW_START_ADDR
				+ lut_table_index * SDR2HDR_GROUP_SIZE));
}


static void iris_scaler1d_lut_address_update(
		struct ocp_header *ocp_burst_header, u32 lut_pkt_cmd_index)
{
	u32 address_shift;

	switch (lut_pkt_cmd_index) {
	case 0: /* Y_H */
		address_shift = SCALER1D_H_Y_HIGH;
		break;
	case 1: /*UV_H*/
		address_shift = SCALER1D_H_UV_HIGH;
		break;
	case 2: /* Y_L*/
		address_shift = SCALER1D_H_Y_LOW;
		break;
	case 3: /*UV_L*/
		address_shift = SCALER1D_H_UV_LOW;
		break;
	case 4: /* Y*/
		address_shift = SCALER1D_V_Y;
		break;
	case 5: /*UV*/
		address_shift = SCALER1D_V_UV;
		break;
	}


	ocp_burst_header->address = SCALER1D_LUT_ADDRESS + address_shift;
}

static int iris_scaler1d_lut_read(
		u32 lut_table_index, const u8 *fw_data)
{
	u32 len = 0;
	u32 pos = 0;
	u32 block_index = 0, lut_pkt_cmd_index = 0;

	u32 iris_lut_buf_index, cmd_payload_len = 0;

	u32 scaler_payload_size =
			(payload_size >= SCALER1D_LUT_BLOCK_SIZE)
			? SCALER1D_LUT_BLOCK_SIZE : payload_size;

	u32 scaler_pkt_size = scaler_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 scaler_block_pkt_cnt =
			(SCALER1D_LUT_BLOCK_SIZE + scaler_payload_size - 1)
			/ scaler_payload_size;
	u32 scaler_one_lut_pkt_cnt =
			scaler_block_pkt_cnt * SCALER_LUT_BLOCK_NUMBER;

	const u8 *fw_data_start = fw_data;

	struct ocp_header ocp_burst_header;

	memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
	ocp_burst_header.header = 0x0010000C;
	ocp_burst_header.address = SCALER1D_LUT_ADDRESS;

	if (scaler1d_lut_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
			* scaler_one_lut_pkt_cnt * SCALER1D_LUT_NUMBER;
		scaler1d_lut_send_cmd = kzalloc(len, GFP_KERNEL);
		iris_lut_param.lut_cmd_cnts_max += scaler_one_lut_pkt_cnt;
		iris_lut_param.scaler_one_lut_pkt_cnt = scaler_one_lut_pkt_cnt;

		pr_err("%s lut_cmd_cnts_max = %d scaler_block_pkt_cnt =%d\n",
				__func__, iris_lut_param.lut_cmd_cnts_max,
				scaler_block_pkt_cnt);
	}

	if (iris_scaler1d_lut_buf == NULL) {
		len = scaler_pkt_size
				* scaler_one_lut_pkt_cnt * SCALER1D_LUT_NUMBER;
		iris_scaler1d_lut_buf = kzalloc(len, GFP_KERNEL);
	}
	if (!scaler1d_lut_send_cmd || !iris_scaler1d_lut_buf) {
		pr_err("%s: failed to alloc mem: lut_cmd: %p, buf: %p\n",
			__func__, scaler1d_lut_send_cmd, iris_scaler1d_lut_buf);
		return IRIS_FAILED;
	}

	{
		for (lut_pkt_cmd_index = 0;
			lut_pkt_cmd_index < scaler_one_lut_pkt_cnt;
				lut_pkt_cmd_index++) {

			iris_lut_buf_index =
				lut_table_index * scaler_pkt_size
					* scaler_one_lut_pkt_cnt
				+ lut_pkt_cmd_index * scaler_pkt_size;

			block_index = lut_pkt_cmd_index / scaler_block_pkt_cnt;
			/*update address at new block*/
			if (lut_pkt_cmd_index % scaler_block_pkt_cnt == 0) {
				iris_scaler1d_lut_address_update(
					&ocp_burst_header,
					block_index);

				fw_data = fw_data_start
					+ block_index / 2 * SCALER1D_LUT_BLOCK_SIZE;
			}

			cmd_payload_len = scaler_payload_size;

			if (SCALER1D_LUT_BLOCK_SIZE >=
				((lut_pkt_cmd_index%scaler_block_pkt_cnt + 1)
					* scaler_payload_size))
				cmd_payload_len = scaler_payload_size;
			else/*last pkt may less than payload_size*/
				cmd_payload_len = SCALER1D_LUT_BLOCK_SIZE
					- (scaler_block_pkt_cnt-1)
						* scaler_payload_size;

			pos = lut_pkt_cmd_index
				+ scaler_one_lut_pkt_cnt * lut_table_index;
			scaler1d_lut_send_cmd[pos].dchdr.dtype = 0x29;
			scaler1d_lut_send_cmd[pos].dchdr.dlen = cmd_payload_len
				+ DIRECT_BUS_HEADER_SIZE;
			scaler1d_lut_send_cmd[pos].dchdr.wait = LUT_CMD_WAIT_MS;
			scaler1d_lut_send_cmd[pos].payload =
				iris_scaler1d_lut_buf + iris_lut_buf_index;

			pr_debug("%s: lut_table_index=%d, lut_pkt_cmd_index=%d",
				__func__, lut_table_index, lut_pkt_cmd_index);
			pr_debug(",ocp_burst_header address=%x header=%x\n",
				ocp_burst_header.address,
				ocp_burst_header.header);

			memcpy(&iris_scaler1d_lut_buf[iris_lut_buf_index],
				&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
			memcpy(&iris_scaler1d_lut_buf[iris_lut_buf_index],
					fw_data, cmd_payload_len);

			fw_data += cmd_payload_len;
			ocp_burst_header.address += cmd_payload_len;

		}
	}
	return IRIS_SUCCESS;
}
static void iris_scaler1d_lut_check(const u8 *fw_data)
{
	u32 lut_table_index = 0;

	for (lut_table_index = 0;
		lut_table_index < SCALER1D_LUT_NUMBER;
			lut_table_index++)
		iris_scaler1d_lut_read(lut_table_index,
			(fw_data + SCALER1D_FW_START_ADDR
				+ lut_table_index * SCALER1D_LUT_SIZE));
}


/* only need payload, header will be overwritten by dtsi*/
static int iris_gamma_lut_read(
		u32 lut_table_index, const u8 *fw_data)
{
	u32 len = 0;
	u32 pos = 0;
	u32 lut_pkt_cmd_index = 0;
	u32 iris_lut_buf_index, cmd_payload_len = 0;

	u32 gamma_payload_size = payload_size;
	u32 gamma_pkt_size = gamma_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 gamma_one_lut_pkt_cnt =
		(GAMMA_LUT_SIZE + gamma_payload_size - 1) / gamma_payload_size;

	struct ocp_header ocp_burst_header;

	memset(&ocp_burst_header, 0, sizeof(ocp_burst_header));
	ocp_burst_header.header = 0x00000000;
	/*address will be overwritten*/
	ocp_burst_header.address = GAMMA_REG_ADDRESS;

	if (gamma_lut_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
			* gamma_one_lut_pkt_cnt * GAMMA_LUT_NUMBER;
		gamma_lut_send_cmd = kzalloc(len, GFP_KERNEL);

		iris_lut_param.lut_cmd_cnts_max += gamma_one_lut_pkt_cnt;
		iris_lut_param.gamma_one_lut_pkt_cnt = gamma_one_lut_pkt_cnt;

		pr_err("%s, iris_lut_param.lut_cmd_cnts_max = %d\n",
			__func__, iris_lut_param.lut_cmd_cnts_max);
	}

	if (iris_gamma_lut_buf == NULL) {
		len = gamma_pkt_size * gamma_one_lut_pkt_cnt * GAMMA_LUT_NUMBER;
		iris_gamma_lut_buf = kzalloc(len, GFP_KERNEL);
	}

	if (!iris_gamma_lut_buf || !gamma_lut_send_cmd) {
		pr_err("%s:failed to alloc mem gamma_lut_send_cmd:%p, buf:%p\n",
			__func__, gamma_lut_send_cmd, iris_gamma_lut_buf);
		return IRIS_FAILED;
	}

	{

		ocp_burst_header.address = GAMMA_REG_ADDRESS;
		for (lut_pkt_cmd_index = 0;
			lut_pkt_cmd_index < gamma_one_lut_pkt_cnt;
				lut_pkt_cmd_index++) {

			iris_lut_buf_index =
				lut_table_index * gamma_pkt_size
				* gamma_one_lut_pkt_cnt
				+ lut_pkt_cmd_index * gamma_pkt_size;

			cmd_payload_len = gamma_payload_size;
			if (lut_pkt_cmd_index == gamma_one_lut_pkt_cnt - 1)
				cmd_payload_len = GAMMA_LUT_SIZE
					- gamma_payload_size * lut_pkt_cmd_index;

			pos = lut_table_index * gamma_one_lut_pkt_cnt
					+ lut_pkt_cmd_index;
			gamma_lut_send_cmd[pos].dchdr.dtype = 0x29;
			gamma_lut_send_cmd[pos].dchdr.dlen =
				cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
			gamma_lut_send_cmd[pos].dchdr.wait = LUT_CMD_WAIT_MS;
			gamma_lut_send_cmd[pos].payload =
				iris_gamma_lut_buf + iris_lut_buf_index;

			pr_debug("%s: lut_table_index=%d, lut_pkt_cmd_index=%d",
				__func__, lut_table_index, lut_pkt_cmd_index);
			pr_debug(",ocp_burst_header address=%x header=%x\n",
				ocp_burst_header.address,
				ocp_burst_header.header);

			memcpy(&iris_gamma_lut_buf[iris_lut_buf_index],
				&ocp_burst_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;
			memcpy(&iris_gamma_lut_buf[iris_lut_buf_index],
				fw_data, cmd_payload_len);

			fw_data += cmd_payload_len;
			ocp_burst_header.address += cmd_payload_len;

		}
	}
	return IRIS_SUCCESS;
}
static void iris_gamma_lut_check(const u8 *fw_data)
{
	u32 lut_table_index;

	for (lut_table_index = 0;
		lut_table_index < GAMMA_LUT_NUMBER;
			lut_table_index++)
		iris_gamma_lut_read(lut_table_index,
			(fw_data + GAMMA_FW_START_ADDR
				+ lut_table_index * GAMMA_LUT_SIZE));
}

static void iris_panel_nits_check(const u8 *fw_data)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	pcfg->panel_nits = *(fw_data + PANEL_NITS_FW_START_ADDR + 1);
	pcfg->panel_nits = (pcfg->panel_nits << 8)
		+ *(fw_data + PANEL_NITS_FW_START_ADDR);
}

static int iris_lut_data_read(const u8 *fw_data, size_t fw_size)
{
	if (fw_size != IRIS_FW_SIZE)
		pr_err("%s: fw_size is wrong should =%d bytes\n",
				__func__, (IRIS_FW_SIZE));
	else {
		iris_dbc_lut_read(fw_data);
		iris_cm_lut_check(fw_data);
		iris_sdr2hdr_lut_check(fw_data);
		iris_scaler1d_lut_check(fw_data);
		iris_gamma_lut_check(fw_data);
		iris_panel_nits_check(fw_data);

		iris_set_lut_cnt(iris_lut_param.lut_cmd_cnts_max);
	}

	return IRIS_SUCCESS;
}

int iris_parse_lut_cmds(const char *name)
{
	const struct firmware *fw = NULL;
	int ret = 0, result = IRIS_SUCCESS;

	/*init buf before parse. parce once is ok.*/
	iris_lut_buf_init();

	if (name) {
		/* Firmware file must be in /system/etc/firmware/ */
		ret = request_firmware(&fw, name, g_mfd->fbi->dev);
		if (ret) {
			pr_err("%s: failed to request firmware: %s, ret = %d\n",
					__func__, name, ret);
			result = IRIS_FAILED;
		} else {
			pr_info("%s:request firmware: name = %s, size = %zu bytes\n",
					__func__, name, fw->size);
			iris_lut_data_read(fw->data, fw->size);
			release_firmware(fw);
		}
	} else {
		pr_err("%s: firmware is null\n", __func__);
		result = IRIS_FAILED;
	}

	return result;
}

/******************************
**** for DBC, lut_table_index means DBC_LEVEL;
****    lut_abtable_index means AB_table.
****: EX: lut_table_index=DBC_INIT, lut_abtable_index=0;
****  means send income_BL A table to iris.
****
**** bSendFlag means send commands out or not;
**** bSendFlag = true, means send out directly.
**** bSendFlag = false, means just pack it
**** and send it together with next command
********************************/

int iris_lut_send(
		u8 lut_type, u8 lut_table_index,
		u32 lut_abtable_index, bool bSendFlag)
{
	u32 pos = 0;
	/*used to tranfser IP domin luts.*/
	static struct dsi_panel_cmds panel_cmds;
	struct iris_cfg *pcfg;
	struct dsi_cmd_desc *pdesc = NULL;
	u8 ip;
	/*fixed for Lut table update in lightup sequence*/
	u8 opt_id = 0xfe;
	u8 type = IRIS_SEND_LUT_BY_DMA;
	u32 cmds_cnt_temp;

	pcfg = iris_get_cfg();

	pdesc = pcfg->iris_cmds.iris_cmds_buf;
	cmds_cnt_temp = 0;

	switch (lut_type) {
	case DBC_LUT:
		if (!iris_dbc_lut_buf
			|| !dbc_lut_income_send_cmd
			|| !dbc_lut_compk_send_cmd)
			break;

		if (lut_table_index > DBC_HIGH)
			break;

		/*lut_abtable_index will be used at AB_table index here.*/
		if (lut_abtable_index > 0)
			lut_abtable_index = 1;

		if (lut_table_index == DBC_INIT) {
			pos = lut_abtable_index
				* iris_lut_param.dbc_income_pkt_cnt;

			memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
				&dbc_lut_income_send_cmd[pos],
				sizeof(struct dsi_cmd_desc)
					* iris_lut_param.dbc_income_pkt_cnt);
			pcfg->iris_cmds.cmds_index +=
					iris_lut_param.dbc_income_pkt_cnt;
		} else {
			pos = (lut_abtable_index
				+ (lut_table_index-1) * AB_TABLE)
					* iris_lut_param.dbc_compk_pkt_cnt;

			memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
				&dbc_lut_compk_send_cmd[pos],
				sizeof(struct dsi_cmd_desc)
					* iris_lut_param.dbc_compk_pkt_cnt);
			pcfg->iris_cmds.cmds_index +=
					iris_lut_param.dbc_compk_pkt_cnt;
		}
		pr_debug("%s: call DBC_LUT\n", __func__);
	break;
	case CM_LUT:
		if (!iris_cm_lut_buf || !cm_lut_send_cmd)
			break;
		if (lut_table_index >= CM_LUT_NUMBER)
			break;

		pos = iris_lut_param.cm_one_lut_pkt_cnt * lut_table_index;
		memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
				&cm_lut_send_cmd[pos],
				sizeof(struct dsi_cmd_desc)
					* iris_lut_param.cm_one_lut_pkt_cnt);
		pcfg->iris_cmds.cmds_index +=
				iris_lut_param.cm_one_lut_pkt_cnt;

	break;
	case SDR2HDR_LUT:
		if (!iris_sdr2hdr_lut_buf || !sdr2hdr_lut_send_cmd)
			break;
		if (lut_table_index >= SDR2HDR_LUT_GROUP_CNT)
			break;

		pos = lut_table_index * iris_lut_param.hdr_one_lut_pkt_cnt;
		memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
				&sdr2hdr_lut_send_cmd[pos],
				sizeof(struct dsi_cmd_desc)
					* iris_lut_param.hdr_one_lut_pkt_cnt);
		pcfg->iris_cmds.cmds_index +=
				iris_lut_param.hdr_one_lut_pkt_cnt;

	break;
	case SCALER1D_LUT:
		if (!iris_scaler1d_lut_buf || !scaler1d_lut_send_cmd)
			break;
		if (lut_table_index >= SCALER1D_LUT_NUMBER)
			break;

		pos = iris_lut_param.scaler_one_lut_pkt_cnt*lut_table_index;
		memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
			&scaler1d_lut_send_cmd[pos],
			sizeof(struct dsi_cmd_desc)
				* iris_lut_param.scaler_one_lut_pkt_cnt);
		pcfg->iris_cmds.cmds_index +=
				iris_lut_param.scaler_one_lut_pkt_cnt;

	break;

	case AMBINET_HDR_GAIN:
		if (!iris_maxcll_lut_buf || !dynamic_lutuvy_send_cmd)
			break;

		memcpy(&pcfg->iris_cmds.iris_cmds_buf[pcfg->iris_cmds.cmds_index], \
				&dynamic_lutuvy_send_cmd[0], \
				sizeof(struct dsi_cmd_desc)*iris_lut_param.hdr_lutuvy_pkt_cnt);

		pcfg->iris_cmds.cmds_index += iris_lut_param.hdr_lutuvy_pkt_cnt;
		pr_err("%s\n", __func__);
	break;

	case AMBINET_SDR2HDR_LUT:
		if (!iris_ambient_lut_buf || !dynamic_lut_send_cmd)
			break;

		memcpy(&pdesc[pcfg->iris_cmds.cmds_index],
			&dynamic_lut_send_cmd[0],
			sizeof(struct dsi_cmd_desc)
				* iris_lut_param.hdr_lut2_pkt_cnt);

		pcfg->iris_cmds.cmds_index +=
				iris_lut_param.hdr_lut2_pkt_cnt;

	break;

	default:
		pr_debug("%s:type of %d have no cmd\n", __func__, lut_type);
	break;

	}

	pr_debug("%s: called lut_type=%d lut_table_index=%d ",
			__func__, lut_type, lut_table_index);
	pr_debug("lut_abtable_index=%d cmd_cnt %d, max_cnt =%d bSendFlag=%d\n",
		lut_abtable_index, pcfg->iris_cmds.cmds_index,
		iris_lut_param.lut_cmd_cnts_max, bSendFlag);

	panel_cmds.cmd_cnt = 0;
	switch (lut_type) {
	case DBC_LUT:
		if (!iris_dbc_lut_buf || !dbc_lut_cabc_dlv_send_cmd)
			break;

		ip = IRIS_IP_DBC;
		if (lut_table_index >= DBC_OFF && lut_table_index <= DBC_HIGH) {
			pos = (lut_table_index - 1)
					* iris_lut_param.dbc_dlv_pkt_cnt;
			panel_cmds.cmds = &dbc_lut_cabc_dlv_send_cmd[pos];
			panel_cmds.cmd_cnt = iris_lut_param.dbc_dlv_pkt_cnt;
		}

	break;
	case SDR2HDR_LUT:
		if (!iris_sdr2hdr_lut_buf || !sdr2hdr_inv_uv_send_cmd)
			break;
		/* for sdr2hdr's case 709->709, 709->p3,
			709->2020, should write PI.*/
		if (lut_table_index < 3)
			break;

		ip = IRIS_IP_SDR2HDR;
		pos = lut_table_index * iris_lut_param.uv_one_lut_pkt_cnt;
		panel_cmds.cmds = &sdr2hdr_inv_uv_send_cmd[pos];
		panel_cmds.cmd_cnt = iris_lut_param.uv_one_lut_pkt_cnt;

	break;

	case GAMMA_LUT:
		if (!iris_gamma_lut_buf || !gamma_lut_send_cmd)
			break;

		ip = IRIS_IP_DPP;
		pos = lut_table_index * iris_lut_param.gamma_one_lut_pkt_cnt;
		panel_cmds.cmds = &gamma_lut_send_cmd[pos];
		panel_cmds.cmd_cnt = iris_lut_param.gamma_one_lut_pkt_cnt;

	break;
	default:
		pr_debug("%s:type of %d have no register level cmd\n",
				__func__, lut_type);
	break;

	}

	pr_debug("%s: cmd_cnt %d\n", __func__, panel_cmds.cmd_cnt);

	if (panel_cmds.cmd_cnt != 0) {
		if (type == IRIS_SEND_LUT_BY_CMDLIST) {
			panel_cmds.link_state = DSI_HS_MODE;
			iris_dsi_cmds_send(pcfg->ctrl, &panel_cmds);
		} else if (type == IRIS_SEND_LUT_BY_DMA) {
			int32_t rc = 0;

			rc = iris_update_lut_payload(ip, opt_id, &panel_cmds);
			if (rc != 0) {
				pr_err("update lut payload is error\n");
				return -EINVAL;
			}
		}
	}

	return IRIS_SUCCESS;
}

void iris_ambient_lut_update(enum LUT_TYPE lutType)
{
	u32 len = 0;
	u32 hdr_payload_size = payload_size;
	u32 hdr_pkt_size = hdr_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 hdr_block_pkt_cnt =
			(SDR2HDR_LUT_BLOCK_SIZE + hdr_payload_size - 1)
				/ hdr_payload_size;

	u32 iris_lut_buf_index, lut_block_index, lut_block_cnt;
	u32 lut_pkt_cmd_index;
	u32 temp_index, index_i;
	u32 dbus_addr_start;

	u32 lut_fw_index;
	u32 cmd_payload_len;

	struct ocp_header ocp_dbus_header;

	memset(&ocp_dbus_header, 0, sizeof(ocp_dbus_header));
	ocp_dbus_header.header = 0x0004000C;
	ocp_dbus_header.address = SDR2HDR_LUT2_ADDRESS;

	if (lutType == AMBINET_SDR2HDR_LUT) {
		dbus_addr_start = SDR2HDR_LUT2_ADDRESS;
		lut_block_cnt = SDR2HDR_LUT2_BLOCK_CNT;

		/* copy lut2 to the firmware format.
		  lut2 is EVEN+ODD,
		  LUT2_fw is  EVEN ODD EVEN ODD EVEN ODD*/
		for (index_i = 0; index_i < LUT_LEN; index_i++) {

			if (index_i % 2 == 0) /* EVEN*/
				lut_fw_index = index_i/2;
			else /* ODD */
				lut_fw_index = index_i/2 + LUT_LEN/2;

			LUT2_fw[lut_fw_index] = lut_lut2[index_i];
			LUT2_fw[lut_fw_index + LUT_LEN] = lut_lut2[index_i];
			LUT2_fw[lut_fw_index + LUT_LEN + LUT_LEN] =
					lut_lut2[index_i];
		}
	} else {
		pr_err("%s input lutType error %d\n", __func__, lutType);
		return;
	}

	if (dynamic_lut_send_cmd == NULL) {
		len = sizeof(struct dsi_cmd_desc)
			* hdr_pkt_size * hdr_block_pkt_cnt
				* SDR2HDR_LUT2_BLOCK_NUMBER;
		dynamic_lut_send_cmd = kzalloc(len, GFP_KERNEL);
		iris_lut_param.lut_cmd_cnts_max +=
				hdr_block_pkt_cnt * SDR2HDR_LUT2_BLOCK_NUMBER;
		iris_lut_param.hdr_lut2_pkt_cnt =
				hdr_block_pkt_cnt * SDR2HDR_LUT2_BLOCK_NUMBER;
	}

	if (iris_ambient_lut_buf)
		memset(iris_ambient_lut_buf, 0,
				hdr_pkt_size * iris_lut_param.hdr_lut2_pkt_cnt);

	if (!iris_ambient_lut_buf) {
		len = hdr_pkt_size * iris_lut_param.hdr_lut2_pkt_cnt;
		iris_ambient_lut_buf = kzalloc(len, GFP_KERNEL);
	}
	if (!iris_ambient_lut_buf)
		return;

	lut_fw_index = 0;
	/*parse LUT2*/
	for (lut_block_index = 0;
		lut_block_index < lut_block_cnt;
			lut_block_index++){

		ocp_dbus_header.address = dbus_addr_start
				+ lut_block_index
				* SDR2HDR_LUT_BLOCK_ADDRESS_INC;

		for (lut_pkt_cmd_index = 0;
			lut_pkt_cmd_index < hdr_block_pkt_cnt;
				lut_pkt_cmd_index++) {

			iris_lut_buf_index =
				lut_block_index * hdr_pkt_size
					* hdr_block_pkt_cnt
				+ lut_pkt_cmd_index * hdr_pkt_size;

			if (lut_pkt_cmd_index == hdr_block_pkt_cnt-1)
				cmd_payload_len = SDR2HDR_LUT_BLOCK_SIZE
					- (hdr_block_pkt_cnt-1) * hdr_payload_size;
			else
				cmd_payload_len = hdr_payload_size;

			temp_index = lut_pkt_cmd_index
					+ hdr_block_pkt_cnt * lut_block_index;
			dynamic_lut_send_cmd[temp_index].dchdr.dtype = 0x29;
			dynamic_lut_send_cmd[temp_index].dchdr.dlen =
				cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
			dynamic_lut_send_cmd[temp_index].dchdr.wait = 0;
			dynamic_lut_send_cmd[temp_index].payload =
				iris_ambient_lut_buf + iris_lut_buf_index;

			memcpy(&iris_ambient_lut_buf[iris_lut_buf_index],
				&ocp_dbus_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;

			memcpy(&iris_ambient_lut_buf[iris_lut_buf_index],
				&LUT2_fw[lut_fw_index], cmd_payload_len);

			lut_fw_index += cmd_payload_len / 4;
			ocp_dbus_header.address += cmd_payload_len;
		}
	}
}

void iris_maxcll_lut_update(enum LUT_TYPE lutType)
{
#if 0
	int i;
	lutType = lutType;
	for(i=0; i<LUT_LEN; i++) {
		pr_err("luty[%d] = %d\n", i, lut_luty[i]);
	}
	for(i=0; i<LUT_LEN; i++) {
		pr_err("lutuv[%d] = %d\n", i, lut_lutuv[i]);
	}
#endif
	u32 hdr_payload_size = payload_size;
	u32 hdr_pkt_size = hdr_payload_size + DIRECT_BUS_HEADER_SIZE;
	u32 hdr_block_pkt_cnt = (SDR2HDR_LUT_BLOCK_SIZE + hdr_payload_size - 1)/hdr_payload_size;

	u32 iris_lut_buf_index, lut_block_index, lut_block_cnt,lut_pkt_cmd_index;
	u32 temp_index, index_i;
	u32 dbus_addr_start;

	u32 lut_fw_index;
	u32 cmd_payload_len;

	struct ocp_header ocp_dbus_header;

	memset(&ocp_dbus_header, 0, sizeof(ocp_dbus_header));
	ocp_dbus_header.header = 0x0004000C;
	ocp_dbus_header.address = SDR2HDR_LUTUVY_ADDRESS;

	if (lutType == AMBINET_HDR_GAIN) {
		dbus_addr_start = SDR2HDR_LUTUVY_ADDRESS;
		lut_block_cnt = SDR2HDR_LUTUVY_BLOCK_CNT;

		// copy lutuvy to the firmware format.
		// lutuvy is EVEN+ODD, LUT2_fw is  EVEN ODD EVEN ODD EVEN ODD
		for(index_i = 0; index_i < LUT_LEN; index_i++) {

			if(index_i%2 == 0) // EVEN
				lut_fw_index = index_i/2;
			else // ODD
				lut_fw_index = index_i/2 + LUT_LEN/2;

			LUTUVY_fw[lut_fw_index] = lut_lutuv[index_i];
			LUTUVY_fw[lut_fw_index + LUT_LEN] = lut_lutuv[index_i];
			LUTUVY_fw[lut_fw_index + LUT_LEN + LUT_LEN] = lut_lutuv[index_i];
			LUTUVY_fw[lut_fw_index + LUT_LEN + LUT_LEN+ LUT_LEN] = lut_lutuv[index_i];
			LUTUVY_fw[lut_fw_index + LUT_LEN + LUT_LEN+ LUT_LEN+ LUT_LEN] = lut_luty[index_i];
			LUTUVY_fw[lut_fw_index + LUT_LEN + LUT_LEN+ LUT_LEN+ LUT_LEN+ LUT_LEN] = lut_luty[index_i];
		}
	} else {
		pr_err("%s input lutType error %d\n", __func__, lutType);
		return;
	}

	if (dynamic_lutuvy_send_cmd == NULL) {
		dynamic_lutuvy_send_cmd = kzalloc(sizeof(struct dsi_cmd_desc)*hdr_pkt_size*hdr_block_pkt_cnt*SDR2HDR_LUTUVY_BLOCK_NUMBER, GFP_KERNEL);
		iris_lut_param.lut_cmd_cnts_max+= hdr_block_pkt_cnt*SDR2HDR_LUTUVY_BLOCK_NUMBER;
		iris_lut_param.hdr_lutuvy_pkt_cnt = hdr_block_pkt_cnt*SDR2HDR_LUTUVY_BLOCK_NUMBER;
	}

	if (iris_maxcll_lut_buf)
		memset(iris_maxcll_lut_buf, 0, hdr_pkt_size*iris_lut_param.hdr_lutuvy_pkt_cnt);

	if (!iris_maxcll_lut_buf)
		iris_maxcll_lut_buf = kzalloc(hdr_pkt_size*iris_lut_param.hdr_lutuvy_pkt_cnt, GFP_KERNEL);

	if (!iris_maxcll_lut_buf) {
		pr_err("%s: failed to alloc mem\n", __func__);
		return;
	}

	lut_fw_index = 0;
	//parse LUTUVY
	for(lut_block_index = 0; lut_block_index < lut_block_cnt; lut_block_index++){

		ocp_dbus_header.address = dbus_addr_start + lut_block_index*SDR2HDR_LUT_BLOCK_ADDRESS_INC;
		for(lut_pkt_cmd_index = 0; lut_pkt_cmd_index < hdr_block_pkt_cnt; lut_pkt_cmd_index++) {

			iris_lut_buf_index = lut_block_index*hdr_pkt_size*hdr_block_pkt_cnt + lut_pkt_cmd_index*hdr_pkt_size;

			if(lut_pkt_cmd_index == hdr_block_pkt_cnt-1)
				cmd_payload_len = SDR2HDR_LUT_BLOCK_SIZE - (hdr_block_pkt_cnt-1) * hdr_payload_size;
			else
				cmd_payload_len = hdr_payload_size;

			temp_index = lut_pkt_cmd_index+ hdr_block_pkt_cnt*lut_block_index;
			dynamic_lutuvy_send_cmd[temp_index].dchdr.dtype= 0x29;
			dynamic_lutuvy_send_cmd[temp_index].dchdr.dlen = cmd_payload_len + DIRECT_BUS_HEADER_SIZE;
			dynamic_lutuvy_send_cmd[temp_index].dchdr.wait = 0;
			dynamic_lutuvy_send_cmd[temp_index].payload = iris_maxcll_lut_buf + iris_lut_buf_index;

			memcpy(&iris_maxcll_lut_buf[iris_lut_buf_index], &ocp_dbus_header, DIRECT_BUS_HEADER_SIZE);
			iris_lut_buf_index += DIRECT_BUS_HEADER_SIZE;

			memcpy(&iris_maxcll_lut_buf[iris_lut_buf_index], &LUTUVY_fw[lut_fw_index], cmd_payload_len);

			lut_fw_index += cmd_payload_len/4;
			ocp_dbus_header.address += cmd_payload_len;


		}
	}
}



int iris_gamma_lut_update(u32 lut_table_index, const u32 *fw_data)
{
	u16 *gammaformat = NULL;
	int i;

	pr_info("%s, index = %d\n", __func__, lut_table_index);
	gammaformat = kmalloc(sizeof(u16) * GAMMA_LUT_LENGTH, GFP_KERNEL);

	memset(gammaformat, 0, GAMMA_LUT_LENGTH * sizeof(u16));

	for (i = 0; i < GAMMA_LUT_LENGTH - 3; i++) {
		if (i < 66)
			gammaformat[i] = (u16)fw_data[i];
		else if (i >= 66 && i < 130)
			gammaformat[i+1] = (u16)fw_data[i];
		else
			gammaformat[i+2] = (u16)fw_data[i];

	}
	iris_gamma_lut_read(lut_table_index, (u8 *)gammaformat);
	kfree(gammaformat);
	return IRIS_SUCCESS;
}


