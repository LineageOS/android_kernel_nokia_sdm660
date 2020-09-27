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

#include "mdss_dsi_iris3_ioctl.h"
#include "mdss_dsi_iris3_lightup_ocp.h"
#include "mdss_dsi_iris3.h"
#include "mdss_dsi_iris3_def.h"
#include "mdss_dsi_iris3_lp.h"
#include "mdss_dsi_iris3_pq.h"


extern struct msmfb_iris_maxcll_info iris_maxcll_lut;

static int mdss_mipi_dsi_command_t(struct mdss_panel_data *pdata, void __user *argp)
{
	uint32_t len = 0;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct msmfb_mipi_dsi_cmd cmd;

	struct dsi_cmd_desc desc = {
		.payload = NULL,
	};
	struct dsi_cmd_desc *pdesc_muti, *pdesc;
	char read_response_buf[16] = {0};
	struct dcs_cmd_req req = {
		.cmds = &desc,
		.cmds_cnt = 1,
		.flags = CMD_REQ_COMMIT | CMD_REQ_NO_MAX_PKT_SIZE,
		.rlen = 16,
		.rbuf = (char *)&read_response_buf,
		.cb = NULL
	};
	int ret, indx, cmd_len, cmd_cnt;
	char *pcmd_indx;

	struct iris_ocp_dsi_tool_input iris_ocp_input = {0, 0, 0, 0, 0};

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pr_err("%s:%d: mdss_panel_data: %p mdss_dsi_ctrl_pdata: %p\n",
			__func__, __LINE__, pdata, ctrl);
	ret = copy_from_user(&cmd, argp, sizeof(cmd));
	if (ret)
		return -EPERM;

	pr_info("#### %s:%d vc=%u d=%02x f=%u l=%u\n", __func__, __LINE__,
		   cmd.vc, cmd.dtype, cmd.flags, cmd.length);

	pr_info("#### %s:%d %x, %x, %x\n", __func__, __LINE__,
		   cmd.iris_ocp_type, cmd.iris_ocp_addr, cmd.iris_ocp_size);

	if (cmd.length) {
		desc.payload = kmalloc(cmd.length, GFP_KERNEL);
		if (!desc.payload)
			return -ENOMEM;
		ret = copy_from_user(desc.payload, cmd.payload, cmd.length);
		if (ret) {
			ret = -EPERM;
			goto err;
		}
	}

	desc.dchdr.dtype = cmd.dtype;
	desc.dchdr.vc = cmd.vc;
	desc.dchdr.last = !!(cmd.flags & MSMFB_MIPI_DSI_COMMAND_LAST);
	desc.dchdr.ack = !!(cmd.flags & MSMFB_MIPI_DSI_COMMAND_ACK);
	desc.dchdr.dlen = cmd.length;
	desc.dchdr.wait = 0;
	if (cmd.dtype == 0x0f) {
		cmd_cnt = *desc.payload;
		len = sizeof(struct dsi_cmd_desc) * cmd_cnt;
		pdesc_muti = kmalloc(len, GFP_KERNEL);
		pcmd_indx = desc.payload + cmd_cnt + 1;
		for (indx = 0; indx < cmd_cnt; indx++) {
			pdesc = pdesc_muti + indx;
			cmd_len = *(desc.payload + 1 + indx);
			pdesc->dchdr.dtype = *pcmd_indx;
			pdesc->dchdr.vc = 0;
			pdesc->dchdr.last = 0;
			pdesc->dchdr.ack = 0;
			pdesc->dchdr.dlen = cmd_len - 1;
			pdesc->dchdr.wait = 0;
			pdesc->payload = pcmd_indx + 1;

			pcmd_indx += cmd_len;
			if (indx == (cmd_cnt - 1))
				pdesc->dchdr.last = 1;
				pr_info("dtype:%x, dlen: %d, last: %d\n",
					pdesc->dchdr.dtype,
					pdesc->dchdr.dlen,
					pdesc->dchdr.last);
		}
		req.cmds = pdesc_muti;
		req.cmds_cnt = cmd_cnt;
		req.flags = CMD_REQ_COMMIT;
	}

	if (cmd.flags & MSMFB_MIPI_DSI_COMMAND_ACK)
		req.flags = req.flags | CMD_REQ_RX;

	/* This is debug for switch from BFRC Mode directly to PSR Mode*/
	/*cmd.flags & MSMFB_MIPI_DSI_COMMAND_DEBUG) {
		struct mdss_data_type *mdata = mdss_mdp_get_mdata();
		static char iris_psr_update_cmd[2] = { 0x1, 0x2 };
		struct dsi_cmd_desc iris_psr_update = {
			{ DTYPE_GEN_WRITE2, 1, 0, 0, 0,
			  sizeof(iris_psr_update_cmd) }, iris_psr_update_cmd
		};

		iris_wait_for_vsync(mdata->ctl_off);
		mdss_dsi_cmd_hs_mode(1, pdata);
		mdss_dsi_cmds_tx(ctrl, &iris_psr_update, 1, (CMD_REQ_DMA_TPG & CMD_REQ_COMMIT));
		mdss_dsi_cmd_hs_mode(0, pdata);
	*/

	/*cmd.flags & MSMFB_MIPI_DSI_COMMAND_BLLP) {
		struct mdss_data_type *mdata = mdss_mdp_get_mdata();

		iris_wait_for_vsync(mdata->ctl_off);
	*/

	if (cmd.flags & MSMFB_MIPI_DSI_COMMAND_HS)
		req.flags |= CMD_REQ_HS_MODE;

	if (cmd.flags & MSMFB_MIPI_DSI_COMMAND_TO_PANEL)
		iris_panel_cmd_passthrough(ctrl, &req);
	else if (cmd.flags & MSMFB_MIPI_DSI_COMMAND_T) {
		u32 pktCnt = (cmd.iris_ocp_type >> 8) & 0xFF;

		/*only test LUT send command*/
		if ((cmd.iris_ocp_type & 0xF) == PXLW_DIRECTBUS_WRITE) {
			u8 lut_type = (cmd.iris_ocp_type >> 8) & 0xFF;
			u8 lut_index = (cmd.iris_ocp_type >> 16) & 0xFF;
			u8 lut_parse = (cmd.iris_ocp_type >> 24) & 0xFF;
			u32 lut_pkt_index = cmd.iris_ocp_addr;

			if (lut_parse) /* only parse firmware when value is not zero;*/
				iris_parse_lut_cmds(IRIS_FIRMWARE_NAME);
			iris_lut_send(lut_type, lut_index, lut_pkt_index, true);
		} else { /* test ocp wirte*/
			if (pktCnt > DSI_CMD_CNT)
				pktCnt = DSI_CMD_CNT;

			if (cmd.iris_ocp_size < OCP_MIN_LEN)
				cmd.iris_ocp_size = OCP_MIN_LEN;

			iris_ocp_input.iris_ocp_type = cmd.iris_ocp_type & 0xF;
			iris_ocp_input.iris_ocp_cnt = pktCnt;
			iris_ocp_input.iris_ocp_addr = cmd.iris_ocp_addr;
			iris_ocp_input.iris_ocp_value = cmd.iris_ocp_value;
			iris_ocp_input.iris_ocp_size = cmd.iris_ocp_size;

			if (pktCnt)
				iris_write_test_muti_pkt(ctrl, &iris_ocp_input);
			else
				iris_write_test(ctrl, cmd.iris_ocp_addr,
					cmd.iris_ocp_type & 0xF, cmd.iris_ocp_size);
				/*iris_ocp_bitmask_write(ctrl,cmd.iris_ocp_addr,
					cmd.iris_ocp_size,cmd.iris_ocp_value);*/
		}
	} else
		mdss_dsi_cmdlist_put(ctrl, &req);

	if (ctrl->rx_buf.data)
		memcpy(cmd.response, ctrl->rx_buf.data, sizeof(cmd.response));

	ret = copy_to_user(argp, &cmd, sizeof(cmd));
	if (ret)
		ret = -EPERM;
err:
	kfree(desc.payload);
	if (cmd.dtype == 0x0f)
		kfree(pdesc_muti);
	return ret;
}


static int mdss_mipi_dsi_command(struct msm_fb_data_type *mfd, void __user *argp)
{
	struct mdss_overlay_private *mdp5_data = NULL;

	mdp5_data = mfd_to_mdp5_data(mfd);
	if (!mdp5_data) {
		pr_err("mdp5 data is null\n");
		return -EINVAL;
	}

	return mdss_mipi_dsi_command_t(mdp5_data->ctl->panel_data, argp);
}

int msmfb_iris_operate_tool(struct msm_fb_data_type *mfd,
				void __user *argp)
{
	int ret = -1;
	uint32_t parent_type = 0;
	struct msmfb_iris_operate_value configure;
	struct iris_cfg *pcfg = NULL;

	ret = copy_from_user(&configure, argp, sizeof(configure));
	if (ret) {
		pr_err("1st %s type = %d, value = %d\n",
			__func__, configure.type, configure.count);
		return -EPERM;
	}
	pcfg = iris_get_cfg();
	if (pcfg->valid < 1) {
		return -EPERM;
	}
	pr_err("%s type = %d, value = %d\n",
			__func__, configure.type, configure.count);

	parent_type = configure.type & 0xff;
	switch (parent_type) {
	case IRIS_OPRT_TOOL_DSI:
		ret = mdss_mipi_dsi_command(mfd, configure.values);
		break;
	default:
		pr_err("could not find right opertat type = %d\n", configure.type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int iris_configure(struct msm_fb_data_type *mfd, u32 type, u32 value)
{
	struct quality_setting *pqlt_cur_setting = NULL;
	struct iris_setting_info *piris_setting = NULL;
	struct iris_cfg *pcfg = NULL;

	piris_setting = iris_get_setting();
	pqlt_cur_setting = &piris_setting->quality_cur;
	pcfg = iris_get_cfg();

	pr_debug("%s type=0x%04x, value=%d\n", __func__, type, value);

	if ((type >= IRIS_CONFIG_TYPE_MAX)
			|| (mfd->panel_power_state == MDSS_PANEL_POWER_OFF))
		return -EINVAL;
	if (pcfg->valid < 3) {
		pr_err("%s: iris not ready\n", __func__);
		return -EBUSY;
	}

	switch (type) {
	case IRIS_PEAKING:
		pqlt_cur_setting->pq_setting.peaking = value & 0xf;
		if (pqlt_cur_setting->pq_setting.peaking > 4)
			goto error;

		iris_peaking_level_set(pqlt_cur_setting->pq_setting.peaking);
		break;
	case IRIS_CM_6AXES:
		pqlt_cur_setting->pq_setting.cm6axis = value & 0x3;
		iris_cm_6axis_level_set(pqlt_cur_setting->pq_setting.cm6axis);
		break;
	case IRIS_CM_FTC_ENABLE:
		pqlt_cur_setting->pq_setting.cmftc = value & 0x1;
		iris_cm_ftc_enable_set(pqlt_cur_setting->pq_setting.cmftc);
		break;
	case IRIS_CM_COLOR_TEMP_MODE:
		pqlt_cur_setting->pq_setting.cmcolortempmode = value & 0x3;
		if (pqlt_cur_setting->pq_setting.cmcolortempmode > 2)
			goto error;

		iris_cm_colortemp_mode_set(pqlt_cur_setting->pq_setting.cmcolortempmode);
		break;
	case IRIS_CM_COLOR_GAMUT_PRE:
		iris_cm_color_gamut_pre_set(value & 0x03);
		break;
	case IRIS_CM_COLOR_GAMUT:
		pqlt_cur_setting->pq_setting.cmcolorgamut = value;
		if (pqlt_cur_setting->pq_setting.cmcolorgamut > 6)
			goto error;

		iris_cm_color_gamut_set(pqlt_cur_setting->pq_setting.cmcolorgamut);
		break;
	case IRIS_DBC_LCE_POWER:
		if (0 == value)
			iris_dbclce_power_set(false);
		else if (1 == value)
			iris_dbclce_power_set(true);
		else if (2 == value)
			iris_lce_dynamic_pmu_mask_set(false);
		else if (3 == value)
			iris_lce_dynamic_pmu_mask_set(true);

		break;
	case IRIS_DBC_LCE_DATA_PATH:
		iris_dbclce_datapath_set(value & 0x01);
		break;
	case IRIS_LCE_MODE:
		if (pqlt_cur_setting->pq_setting.lcemode != (value & 0x1)) {
			pqlt_cur_setting->pq_setting.lcemode = value & 0x1;
			iris_lce_mode_set(pqlt_cur_setting->pq_setting.lcemode);
		}
		break;
	case IRIS_LCE_LEVEL:
		if (pqlt_cur_setting->pq_setting.lcelevel != (value & 0x7)) {
			pqlt_cur_setting->pq_setting.lcelevel = value & 0x7;
			if (pqlt_cur_setting->pq_setting.lcelevel > 5)
				goto error;

			iris_lce_level_set(pqlt_cur_setting->pq_setting.lcelevel);
		}
		break;
	case IRIS_GRAPHIC_DET_ENABLE:
		pqlt_cur_setting->pq_setting.graphicdet = value & 0x1;
		iris_lce_graphic_det_set(pqlt_cur_setting->pq_setting.graphicdet);
		break;
	case IRIS_AL_ENABLE:

		if (pqlt_cur_setting->pq_setting.alenable != (value & 0x1)) {
			pqlt_cur_setting->pq_setting.alenable = value & 0x1;

			/*check the case here*/
			if (pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
				iris_lce_al_set(pqlt_cur_setting->pq_setting.alenable);
			else
				iris_ambient_light_lut_set();
		}
		break;
	case IRIS_DBC_LEVEL:
		pqlt_cur_setting->pq_setting.dbc = value & 0x3;
		iris_dbc_level_set(pqlt_cur_setting->pq_setting.dbc);
		break;
	case IRIS_BLC_PWM_ENABLE:
		iris_pwm_enable_set(value & 0x1);
		break;
	case IRIS_DEMO_MODE:
		pqlt_cur_setting->pq_setting.demomode = value & 0x3;
		iris_demo_mode_set(pqlt_cur_setting->pq_setting.demomode);
		break;
	case IRIS_DYNAMIC_POWER_CTRL:
		iris_dynamic_power_set(value & 0x01);
		break;
	case IRIS_DMA_LOAD:
		iris_dma_trigger_load();
		break;
	case IRIS_SDR2HDR:
		iris_set_sdr2hdr_mode((value & 0xf00) >> 8);
		value = value & 0xff;
		if (value/10 == 4) {/*magic code to enable YUV input.*/
			iris_set_yuv_input(true);
			value -= 40;
		} else if (value/10 == 6) {
			iris_set_HDR10_YCoCg(true);
			value -= 60;
		} else if (value == 55) {
			iris_set_yuv_input(true);
			goto EXIT_CONFIG;
		} else if (value == 56) {
			iris_set_yuv_input(false);
			goto EXIT_CONFIG;
		} else {
			iris_set_yuv_input(false);
			iris_set_HDR10_YCoCg(false);
		}

		pqlt_cur_setting->pq_setting.sdr2hdr = value;
		if (pqlt_cur_setting->pq_setting.sdr2hdr > SDR709_2_2020)
			goto error;
		iris_sdr2hdr_level_set(pqlt_cur_setting->pq_setting.sdr2hdr);

		break;
	case IRIS_READING_MODE:
		pqlt_cur_setting->pq_setting.readingmode = value & 0x1;
		iris_reading_mode_set(pqlt_cur_setting->pq_setting.readingmode);
		break;
	case IRIS_COLOR_TEMP_VALUE:
		pqlt_cur_setting->colortempvalue = value;
		if (pqlt_cur_setting->pq_setting.cmcolortempmode == IRIS_COLOR_TEMP_MANUL)
			iris_cm_color_temp_set();
		break;
	case IRIS_CCT_VALUE:
		pqlt_cur_setting->cctvalue = value;
		if (pqlt_cur_setting->pq_setting.cmcolortempmode == IRIS_COLOR_TEMP_AUTO)
			iris_cm_color_temp_set();
		break;
	case IRIS_LUX_VALUE:

		/* move to iris_configure_ex*/
		pqlt_cur_setting->luxvalue = value;
		if (pqlt_cur_setting->pq_setting.alenable == 1) {

			if (pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
				iris_lce_lux_set();
			else
				iris_ambient_light_lut_set();
		}

		break;
	case IRIS_HDR_MAXCLL:
		pqlt_cur_setting->maxcll = value;
		break;
	case IRIS_ANALOG_BYPASS_MODE:
		iris_abypass_switch_proc(mfd, value);
		break;
	case IRIS_HDR_PANEL_NITES_SET:
		if (pqlt_cur_setting->al_bl_ratio != value) {
			pqlt_cur_setting->al_bl_ratio = value;
			iris_panel_nits_set(value, false, pqlt_cur_setting->pq_setting.sdr2hdr);
		}
		break;
	case IRIS_PEAKING_IDLE_CLK_ENABLE:
		iris_peaking_idle_clk_enable(value & 0x01);
		break;
	case IRIS_CM_MAGENTA_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_MAGENTA_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_CM_RED_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_RED_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_CM_YELLOW_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_YELLOW_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_CM_GREEN_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_GREEN_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_CM_BLUE_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_BLUE_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_CM_CYAN_GAIN:
		iris_cm_6axis_seperate_gain(IRIS_CYAN_GAIN_TYPE, value & 0x3f);
		break;
	case IRIS_DBC_LED_GAIN:
		iris_dbc_led0d_gain_set(value & 0x3f);
		break;
	case IRIS_SCALER_FILTER_LEVEL:
		iris_scaler_filter_update(value & 0xf);
		break;
	case IRIS_HDR_PREPARE:
		if (value == 3)
			iris_set_skip_dma(true);
		if (!iris_get_debug_cap())
			iris_hdr_csc_prepare();
		break;
	case IRIS_HDR_COMPLETE:
		if (value == 1 || value == 2)
			iris_set_skip_dma(false);
		iris_init_HDRchange();
		iris_hdr_csc_complete(value);
		if (value == 1 || value == 2) {
			if (pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
				iris_panel_nits_set(0, true, pqlt_cur_setting->pq_setting.sdr2hdr);
			else
				iris_panel_nits_set(PANEL_BL_MAX_RATIO, false, pqlt_cur_setting->pq_setting.sdr2hdr);
		}
		break;
	case IRIS_DEBUG_CAP:
		iris_set_debug_cap(value & 0x01);
		break;
	case IRIS_FW_UPDATE:
		// Need do multi-thread protection.
		if (value == 0) {
			iris_parse_lut_cmds(IRIS_FIRMWARE_NAME);
		} else if (value == 2) {
			iris_parse_lut_cmds(IRIS_FIRMWARE_NAME);
			pcfg->valid = 2; /* minimum light up */
			pcfg->add_last_flag = pcfg->add_cont_last_flag;
			iris_send_cont_splash_pkt(IRIS_CONT_SPLASH_KERNEL);
			pcfg->add_last_flag = pcfg->add_pt_last_flag;
			pcfg->valid = 3; /* full light up */
		}
		break;
	default:
		goto error;
	}
EXIT_CONFIG:
	return 0;

error:
	return -EINVAL;

}

static int iris_configure_t(struct msm_fb_data_type *mfd, u32 type, void __user *argp)
{
	int ret = -1;
	uint32_t value = 0;

	ret = copy_from_user(&value, argp, sizeof(uint32_t));
	if (ret) {
		pr_err("can not copy from user\n");
		return -EPERM;
	}

	ret = iris_configure(mfd, type, value);
	return ret;
}

static int iris_configure_ex(struct msm_fb_data_type *mfd, u32 type, u32 count, u32 *values)
{
	int ret = -1;
	struct iris_setting_info *piris_setting = NULL;
	struct quality_setting *pqlt_cur_setting = NULL;
	struct msmfb_iris_ambient_info iris_ambient;
	struct msmfb_iris_ambient_info *pambient_lut;
	struct msmfb_iris_maxcll_info iris_maxcll;
	struct iris_cfg *pcfg = NULL;

	piris_setting = iris_get_setting();
	pambient_lut = iris_get_ambient_lut();
	pqlt_cur_setting = &piris_setting->quality_cur;
	pcfg = iris_get_cfg();

	pr_debug("%s type=0x%04x, count=%d, value=%d\n", __func__, type, count, values[0]);

	if ((type >= IRIS_CONFIG_TYPE_MAX)
			|| (mfd->panel_power_state == MDSS_PANEL_POWER_OFF))
		return -EINVAL;

	if (pcfg->valid < 3) {
		pr_err("%s: iris not ready\n", __func__);
		return -EBUSY;
	}

	switch (type) {
	case IRIS_LUX_VALUE:

		iris_ambient = *(struct msmfb_iris_ambient_info *)(values);
		pambient_lut->ambient_lux = iris_ambient.ambient_lux;
		pqlt_cur_setting->luxvalue = pambient_lut->ambient_lux;

		if (iris_ambient.lut_lut2_payload != NULL) {
			ret = copy_from_user(pambient_lut->lut_lut2_payload,
				iris_ambient.lut_lut2_payload,
					sizeof(uint32_t) * LUT_LEN);
			if (ret) {
				pr_err("can not copy from user sdr2hdr\n");
				goto error1;
			}
			iris_ambient_lut_update(AMBINET_SDR2HDR_LUT);
		}

		if (pqlt_cur_setting->pq_setting.alenable == 1) {

			if (pqlt_cur_setting->pq_setting.sdr2hdr == SDR2HDR_Bypass)
				iris_lce_lux_set();
			else
				iris_ambient_light_lut_set();
		}
			break;
		case IRIS_HDR_MAXCLL:
			iris_maxcll = *(struct msmfb_iris_maxcll_info *)(values);
			iris_maxcll_lut.mMAXCLL= iris_maxcll.mMAXCLL;

			if (iris_maxcll.lut_luty_payload != NULL) {
				ret = copy_from_user(iris_maxcll_lut.lut_luty_payload, iris_maxcll.lut_luty_payload, sizeof(uint32_t)*LUT_LEN);
				if (ret) {
					pr_err("can not copy lut y from user sdr2hdr\n");
					goto error1;
				}
			}
			if (iris_maxcll.lut_lutuv_payload != NULL) {
				ret = copy_from_user(iris_maxcll_lut.lut_lutuv_payload, iris_maxcll.lut_lutuv_payload, sizeof(uint32_t)*LUT_LEN);
				if (ret) {
					pr_err("can not copy lut uv from user sdr2hdr\n");
					goto error1;
				}
			}
			iris_maxcll_lut_update(AMBINET_HDR_GAIN);
			iris_maxcll_lut_set();
		break;
	case IRIS_CCF1_UPDATE:
		iris_cm_lut_read(values[0], (u8 *)&values[2]);
		if (values[1] == 1)   /*last flag is 1*/
			iris_cm_color_gamut_set(pqlt_cur_setting->pq_setting.cmcolorgamut);
		break;
	case IRIS_CCF2_UPDATE:
		iris_gamma_lut_update(values[0], &values[2]);
		if (values[1] == 1)   /*last flag is 1*/
			iris_dpp_gamma_set();
		break;
	case IRIS_DBG_TARGET_REGADDR_VALUE_SET:
		iris_ocp_write(values[0], values[1]);
		break;
	case IRIS_DBG_TARGET_REGADDR_VALUE_SET2:
		iris_ocp_write2(values[0], values[1], count-2, values+2);
		break;
	default:
		goto error;
	}

	return 0;

error:
	return -EINVAL;
error1:
	return -EPERM;
}

static int iris_configure_ex_t(struct msm_fb_data_type *mfd, uint32_t type,
								uint32_t count, void __user *values)
{
	int ret = -1;
	uint32_t len = 0;
	uint32_t *val = NULL;

	len = sizeof(uint32_t) * count;
	val = kmalloc(len, GFP_KERNEL);
	if (!val)
		return -ENOSPC;

	ret = copy_from_user(val, values, sizeof(uint32_t) * count);
	if (ret) {
		pr_err("can not copy from user\n");
		kfree(val);
		return -EPERM;
	}
	ret = iris_configure_ex(mfd, type, count, val);
	kfree(val);
	return ret;
}

static int iris_configure_get(struct msm_fb_data_type *mfd, u32 type, u32 count, u32 *values)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	struct iris_setting_info *piris_setting = NULL;
	struct quality_setting *pqlt_cur_setting = NULL;

	piris_setting = iris_get_setting();
	pqlt_cur_setting = &piris_setting->quality_cur;

	if (type >= IRIS_CONFIG_TYPE_MAX)
		return -EINVAL;

	switch (type) {
	case IRIS_PEAKING:
		*values = pqlt_cur_setting->pq_setting.peaking;
		break;
	case IRIS_CM_6AXES:
		*values = pqlt_cur_setting->pq_setting.cm6axis;
		break;
	case IRIS_CM_FTC_ENABLE:
		*values = pqlt_cur_setting->pq_setting.cmftc;
		break;
	case IRIS_CM_COLOR_TEMP_MODE:
		*values = pqlt_cur_setting->pq_setting.cmcolortempmode;
		break;
	case IRIS_CM_COLOR_GAMUT:
		*values = pqlt_cur_setting->pq_setting.cmcolorgamut;
		break;
	case IRIS_LCE_MODE:
		*values = pqlt_cur_setting->pq_setting.lcemode;
		break;
	case IRIS_LCE_LEVEL:
		*values = pqlt_cur_setting->pq_setting.lcelevel;
		break;
	case IRIS_GRAPHIC_DET_ENABLE:
		*values = pqlt_cur_setting->pq_setting.graphicdet;
		break;
	case IRIS_AL_ENABLE:
		*values = pqlt_cur_setting->pq_setting.alenable;
		break;
	case IRIS_DBC_LEVEL:
		*values = pqlt_cur_setting->pq_setting.dbc;
		break;
	case IRIS_DEMO_MODE:
		*values = pqlt_cur_setting->pq_setting.demomode;
		break;
	case IRIS_SDR2HDR:
		*values = pqlt_cur_setting->pq_setting.sdr2hdr;
		break;
	case IRIS_LUX_VALUE:
		*values = pqlt_cur_setting->luxvalue;
		break;
	case IRIS_READING_MODE:
		*values = pqlt_cur_setting->pq_setting.readingmode;
		break;
	case IRIS_DYNAMIC_POWER_CTRL:
		*values = iris_dynamic_power_get();
		break;
	case IRIS_HDR_MAXCLL:
		*values = pqlt_cur_setting->maxcll;
		break;
	case IRIS_ANALOG_BYPASS_MODE:
		*values = pcfg->abypss_ctrl.abypass_mode;
		break;
	case IRIS_CM_COLOR_GAMUT_PRE:
		*values = pqlt_cur_setting->source_switch;
		break;
	case IRIS_CCT_VALUE:
		*values = pqlt_cur_setting->cctvalue;
		break;
	case IRIS_COLOR_TEMP_VALUE:
		*values = pqlt_cur_setting->colortempvalue;
		break;
	case IRIS_CHIP_VERSION:
		*values = 2;
		break;
	case IRIS_PANEL_TYPE:
		*values = pcfg->panel_type;
		break;
	case IRIS_PANEL_NITS:
		*values = pcfg->panel_nits;
		break;
	case IRIS_DBG_TARGET_REGADDR_VALUE_GET:
		*values = iris_ocp_read(*values, DSI_HS_MODE);
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static int iris_configure_get_t(struct msm_fb_data_type *mfd,
				uint32_t type, uint32_t count, void __user *values)
{
	int ret = -1;
	uint32_t len = 0;
	uint32_t *val = NULL;

	len = count * sizeof(uint32_t);
	val = kmalloc(len, GFP_KERNEL);
	if (val == NULL) {
		pr_err("could not kmalloc space for func = %s\n", __func__);
		return -ENOSPC;
	}
	ret = copy_from_user(val, values, sizeof(uint32_t) * count);
	if (ret) {
		pr_err("can not copy from user\n");
		kfree(val);
		return -EPERM;
	}
	ret = iris_configure_get(mfd, type, count, val);
	if (ret) {
		pr_err("get error\n");
		kfree(val);
		return ret;
	}
	ret = copy_to_user(values, val, sizeof(uint32_t) * count);
	if (ret) {
		pr_err("copy to user error\n");
		kfree(val);
		return -EPERM;
	}
	kfree(val);
	return ret;
}

int msmfb_iris_operate_conf(struct msm_fb_data_type *mfd,
				void __user *argp)
{
	int ret = -1;
	uint32_t parent_type = 0;
	uint32_t child_type = 0;
	struct msmfb_iris_operate_value configure;
	struct iris_cfg *pcfg = NULL;

	ret = copy_from_user(&configure, argp, sizeof(configure));
	if (ret != 0) {
		pr_err("can not copy from user\n");
		return -EPERM;
	}
	pcfg = iris_get_cfg();
	if (pcfg->valid < 1) {
		return -EPERM;
	}
	pr_debug("%s type=0x%04x, count=%d\n", __func__, configure.type, configure.count);

	parent_type = configure.type & 0xff;
	child_type = (configure.type >> 8) & 0xff;
	switch (parent_type) {
	case IRIS_OPRT_CONFIGURE:
		ret = iris_configure_t(mfd, child_type, configure.values);
		break;
	case IRIS_OPRT_CONFIGURE_NEW:
		ret = iris_configure_ex_t(mfd, child_type, configure.count, configure.values);
		break;
	case IRIS_OPRT_CONFIGURE_NEW_GET:
		ret = iris_configure_get_t(mfd, child_type, configure.count, configure.values);
		break;
	default:
		pr_err("could not find right opertat type = %d\n", configure.type);
		break;
	}

	return ret;
}




static ssize_t iris_pq_config_write(struct file *file, const char __user *buff,
		size_t count, loff_t *ppos)
{
	unsigned long val;
	struct iris_cfg_log {
		uint8_t type;
		char *str;
	};

	struct iris_cfg_log arr[] = {
					{IRIS_PEAKING, "IRIS_PEAKING"},
					{IRIS_DBC_LEVEL, "IRIS_DBC_LEVEL"},
					{IRIS_LCE_LEVEL, "IRIS_LCE_LEVEL"},
					{IRIS_DEMO_MODE, "IRIS_DEMO_MODE"},
					{IRIS_SDR2HDR, "IRIS_SDR2HDR"},
					{IRIS_DYNAMIC_POWER_CTRL, "IRIS_DYNAMIC_POWER_CTRL"},
					{IRIS_LCE_MODE, "IRIS_LCE_MODE"},
					{IRIS_GRAPHIC_DET_ENABLE, "IRIS_GRAPHIC_DET_ENABLE"},
					{IRIS_HDR_MAXCLL, "IRIS_HDR_MAXCLL"},
					{IRIS_ANALOG_BYPASS_MODE, "IRIS_ANALOG_BYPASS_MODE"},
					{IRIS_CM_6AXES, "IRIS_CM_6AXES"},
					{IRIS_CM_FTC_ENABLE, "IRIS_CM_FTC_ENABLE"},
					{IRIS_CM_COLOR_TEMP_MODE, "IRIS_CM_COLOR_TEMP_MODE"},
					{IRIS_CM_COLOR_GAMUT, "IRIS_CM_COLOR_GAMUT"},
					{IRIS_CM_COLOR_GAMUT_PRE, "IRIS_CM_COLOR_GAMUT_PRE"},
					{IRIS_CCT_VALUE, "IRIS_CCT_VALUE"},
					{IRIS_COLOR_TEMP_VALUE, "IRIS_COLOR_TEMP_VALUE"},
					{IRIS_AL_ENABLE, "IRIS_AL_ENABLE"},
					{IRIS_LUX_VALUE, "IRIS_LUX_VALUE"},
					{IRIS_READING_MODE, "IRIS_READING_MODE"},
					{IRIS_CHIP_VERSION, "IRIS_CHIP_VERSION"},
					{IRIS_PANEL_TYPE, "IRIS_PANEL_TYPE"},
				};

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	if (val) {
		int i = 0;
		uint32_t cfg_val;
		int len = sizeof(arr) / sizeof(arr[0]);

		for (i = 0; i < len; i++) {
			iris_configure_get(NULL, arr[i].type, 1, &cfg_val);
			pr_err("%s: %d\n", arr[i].str, cfg_val);
		}
	}
	return count;
}

static const struct file_operations iris_pq_config_fops = {
	.open = simple_open,
	.write = iris_pq_config_write,
};

static unsigned char g_golden_mcf[] = {
204,
12,
128,
45,
10,
32,
13,
147,
172,
83,
82,
250,
67,
26,
176,
128,
38,
227,
14,
208,
72,
159,
77,
210,
118,
5,
119,
229,
120,
19,
126,
155,
125,
59,
127,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
73,
55,
86,
54,
254,
56,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
32,
242,
191,
129,
11,
64,
95,
240,
195,
23,
96,
164,
112,
133,
27,
160,
101,
88,
130,
14,
192,
16,
24,
192,
5,
224,
31,
168,
1,
11,
0,
0,
0,
0,
0,
};

static ssize_t iris_mcf_read(struct file *file, char __user *buff,
                size_t count, loff_t *ppos)
{
	int tot = 0;
	char bp[512];
	int i, j;
	if (*ppos)
		return 0;

	for (i = 0; i < 100; i++) {
		j = scnprintf(bp + tot, sizeof(bp), "%d ", g_golden_mcf[i]);
		tot += j;
	}
	j = scnprintf(bp + tot, sizeof(bp), "\n");
	tot += j;
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;
	*ppos += tot;

	return tot;
}

static const struct file_operations iris_mcf_fops = {
	.open = simple_open,
	.read = iris_mcf_read,
};

int iris_pq_debugfs_init(struct msm_fb_data_type *mfd)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();
	if (mfd->index != 0)
		return 0;
	if (!(mfd->panel.type == MIPI_VIDEO_PANEL
		|| mfd->panel.type == MIPI_CMD_PANEL))
		return 0;

	if (pcfg->dbg_root == NULL) {
		pcfg->dbg_root = debugfs_create_dir("iris", NULL);
		if (IS_ERR_OR_NULL(pcfg->dbg_root)) {
			pr_err("debugfs_create_dir for iris_debug failed, error %ld\n",
			   PTR_ERR(pcfg->dbg_root));
			return -ENODEV;
		}
	}
	if (debugfs_create_file("iris_pq_config", 0644, pcfg->dbg_root, mfd,
				&iris_pq_config_fops) == NULL) {
		pr_err("%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return -EFAULT;
	}
	if (debugfs_create_file("mcf", 0644, pcfg->dbg_root, mfd,
				&iris_mcf_fops) == NULL) {
		pr_err("%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return -EFAULT;
	}
	return 0;
}
