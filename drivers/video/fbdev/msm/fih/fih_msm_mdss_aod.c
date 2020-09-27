/* Copyright (c) 2017, FIH Mobile Limited Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/string.h>

#include "../mdss_dsi.h"
#include "../mdss_mdp.h"
#include "../mdss_fb.h"

int aod_en=0;
int previous_blank = 0;
int aod_feature = 0;
int blank_mode=0;
int wled_aod_set=0;
static struct mdss_dsi_ctrl_pdata *gpdata  = NULL;
static struct mdss_dsi_ctrl_pdata *spdata  = NULL;
#define VSYNC_DELAY msecs_to_jiffies(17)
static bool aod_ready=0;
extern int  mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds, u32 flags);
extern int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);

extern struct mdss_dsi_ctrl_pdata *mdss_dsi_get_ctrl(u32 ctrl_id);
extern struct msm_fb_data_type *mfd_glance;     //added for glance by yanjun
//struct msm_fb_data_type *mfd;

extern int mdss_fb_mode_switch(struct msm_fb_data_type *mfd, u32 mode);
extern void fih_tp_lcm_resume(void);
extern void fih_tp_lcm_suspend(void);

/*************************************************
 * Api check yanjun
 *             check ok ---- pass
 *4-7  check ok --- pass
 ************************************************/
int fih_get_blank_mode(void)
{
	pr_err("[AOD]***%s: Blank mode(%d)***\n", __func__,blank_mode);
	return blank_mode;
}

int fih_set_blank_mode(int mode)
{
	blank_mode = mode;
	return 0;
}

int fih_get_aod_wled_state(void)
{
	pr_debug("***%s: Is AOD enabled(%d)***\n", __func__,wled_aod_set);
	return wled_aod_set;
}

int fih_set_aod_wled_state(int enable)
{
	wled_aod_set = enable;
	return 0;
}

int fih_get_aod(void)
{
	pr_debug("***%s: Is AOD enabled(%d)***\n", __func__,aod_en);
	return aod_en;
}

int fih_set_aod(int enable)
{
	aod_en = enable;
	pr_err("[AOD]***%s: AOD enabled(%d)***\n", __func__,aod_en);
	return 0;
}

unsigned int fih_get_panel_id(void)
{
	struct mdss_panel_info *pinfo;
	pinfo = &gpdata->panel_data.panel_info;

	return pinfo->panel_id;
}

bool fih_get_aod_ready(void)
{
	struct mdss_panel_info *gpinfo,*spinfo;
	gpinfo = &gpdata->panel_data.panel_info;
	spinfo = &spdata->panel_data.panel_info;

	aod_ready = gpinfo->aod_ready_on|spinfo->aod_ready_on;
	return aod_ready;

}


EXPORT_SYMBOL(fih_set_aod);
EXPORT_SYMBOL(fih_get_aod);
EXPORT_SYMBOL(fih_get_panel_id);
EXPORT_SYMBOL(fih_set_aod_wled_state);
EXPORT_SYMBOL(fih_get_aod_wled_state);
EXPORT_SYMBOL(fih_set_blank_mode);
EXPORT_SYMBOL(fih_get_blank_mode);
EXPORT_SYMBOL(fih_get_aod_ready);

void fih_mdss_dsi_panel_config_aod_res_properties(struct device_node *np, struct dsi_panel_timing *pt)
{
	mdss_dsi_parse_dcs_cmds(np, &pt->aod_resume_cmds,
		"qcom,mdss-dsi-aod-resume-command",
		"qcom,mdss-dsi-aod-resume-command-state");

	mdss_dsi_parse_dcs_cmds(np, &pt->aod_8color_exit_cmds,
		"qcom,mdss-dsi-aod-8color-exit-command",
		"qcom,mdss-dsi-aod-8color-exit-command-state");

	return;
}

void fih_mdss_dsi_panel_config_aod_parse_dt(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->aod_suspend_cmds,
		"qcom,mdss-dsi-aod-suspend-command", "qcom,mdss-dsi-aod-suspend-command-state");


	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->aod_8color_enter_cmds,
		"qcom,mdss-dsi-aod-8color-enter-command", "qcom,mdss-dsi-aod-8color-enter-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->post_aod_8color_enter_cmds,
		"qcom,mdss-dsi-post-aod-8color-enter-command", "qcom,mdss-dsi-post-aod-8color-enter-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->panel_on_cmds,
		"qcom,mdss-dsi-panel-on-command", "qcom,mdss-dsi-panel-on-command-state");

	ctrl_pdata->cmd_early_lp_exit= of_property_read_bool(np,"fih,aod-early-lp-exit");
	if(!ctrl_pdata->cmd_early_lp_exit){
		pr_debug("%s: Always on display exit by framework\n", __func__);
	}else{
		pr_debug("%s: Always on display exit by driver\n", __func__);
	}

	return;
}

void fih_mdss_dsi_panel_aod_exit_register(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_timing *pt)
{
	ctrl->aod_resume_cmds= pt->aod_resume_cmds;
	ctrl->aod_8color_exit_cmds= pt->aod_8color_exit_cmds;
	//wake_lock_destroy(&gpdata->panel_data.panel_info.aod_wake_lock);
	return;
}

//ili7807E
static int old_blank = 0;
int mdss_dsi_panel_ili7807E_low_power_config(struct mdss_panel_data *pdata,int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	struct dsi_panel_cmds *on_cmds;
	//struct msm_fb_data_type *mfd = NULL;
	//struct platform_device *pdev = NULL;
	int blank=0;
	int len = 1;

	pr_err("[AOD][HL]%s: enable = %d <-- START\n", __func__, enable);

	if (pdata == NULL) {
		pr_err("[AOD]%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	//mfd = container_of(pdata->panel_info, struct msm_fb_data_type, panel_info);

	blank = fih_get_blank_mode();
	pr_err("[AOD]%s: enter blank = %d\n", __func__, blank);

	if(enable)
	{
		if(pinfo->aod_ready_on)
		{
			pr_err("[AOD]%s: BossVee Already enter Glance mode\n", __func__);
			msleep(200);
			goto end;
		}

		pr_err("[AOD]%s: Always on display is enable\n", __func__);

		if (ctrl->aod_suspend_cmds.cmd_cnt)
		{
			len = mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_suspend_cmds, CMD_REQ_COMMIT);
			pr_err("[AOD][HL]%s: mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_suspend_cmds...)\n", __func__);
		}
		if(!len)
		{
			pr_err("[AOD]%s: BossVee enter IDLE fail\n", __func__);
		}
		else
		{
			mutex_lock(&mfd_glance->update.lock);
			mdss_fb_mode_switch(mfd_glance, 1);
			mutex_unlock(&mfd_glance->update.lock);
			//pr_err("[AOD]%s: BossVee switch CMD success\n", __func__);
		}

		fih_tp_lcm_suspend();

		//msleep(200);

		mutex_lock(&mfd_glance->no_update.lock);

		if (ctrl->panel_on_cmds.cmd_cnt)
		{
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->panel_on_cmds, CMD_REQ_COMMIT);
			pr_err("[AOD][HL]%s: mdss_dsi_panel_cmds_send(ctrl, &ctrl->panel_on_cmds...)\n", __func__);
		}
		else
		{
			pr_err("[AOD][HL]%s: NOOOOOOOOOOOOOOOOOOOOOO mdss_dsi_panel_cmds_send(ctrl, &ctrl->panel_on_cmds...)\n", __func__);
		}

		mutex_unlock(&mfd_glance->no_update.lock);

       	pinfo->aod_ready_on = 1;

		pr_err("[AOD]%s: BossVee enable glance success\n", __func__);
	}
	else
	{
		if (old_blank != FB_BLANK_POWERDOWN)
		{
			if (!pinfo->aod_ready_on )
			{
				pr_err("[AOD]%s: BossVee aod_ready-on false, quit!\n", __func__);
				goto end;
			}
		}

		if ((blank!=FB_BLANK_UNBLANK) && (blank != FB_BLANK_POWERDOWN) )
		{
			pr_err("[AOD]%s: Still in low power mode blank is %d\n", __func__, blank);
			msleep(100);
			goto end;
		}

		pr_err("[AOD]%s: Always on display is disable\n", __func__);

		on_cmds = &ctrl->aod_resume_cmds;

		if (ctrl->aod_resume_cmds.cmd_cnt)
		{
			len = mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_resume_cmds, CMD_REQ_COMMIT);
			if(!len)
			{
				pr_err("[AOD]%s: BossVee quit IDLE fail\n", __func__);
			}
			else
			{
				mutex_lock(&mfd_glance->update.lock);
				mdss_fb_mode_switch(mfd_glance, 0);
				mutex_unlock(&mfd_glance->update.lock);
				pr_err("[AOD]%s: BossVee switch VIDEO success\n", __func__);
			}
		}

		if(blank != FB_BLANK_POWERDOWN)
		{
			fih_tp_lcm_resume();
		}

		mdss_dsi_panel_cmds_send(ctrl, &ctrl->panel_on_cmds, CMD_REQ_COMMIT);

		pinfo->aod_ready_on = 0;

		pr_err("[AOD]%s: BossVee disable glance success\n", __func__);
	}

	old_blank = blank;

	/* Any panel specific low power commands/config */
	pr_err("[AOD][HL]%s: <-- END\n", __func__);

end:
	pr_err("[AOD][HL]%s: end: <-- END\n", __func__);

	previous_blank = blank;

	return 0;
}
/*JDI Panel*/
#define PANEL_REG_ADDR_LEN 8
static char aod_reg[2] = {0x56, 0x00};
static struct dsi_cmd_desc aod_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(aod_read_cmd)},
	aod_reg
};

int oem_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return -EINVAL;
	}

	aod_reg[0] = cmd0;
	aod_reg[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &aod_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	/*
	 * blocked here, until call back called
	 */

	return mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

void fih_mdss_lp_config(struct mdss_panel_data *pdata,int enable,int ndx)
{
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	pinfo = &pdata->panel_info;

	pr_err("[AOD][HL]%s: <-- START\n", __func__);

    ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	//pr_err("[AOD]%s, begin Bus %d\n",__func__,ndx);

	if(!pinfo->aod_ready_on)
	{
		//If it is not FIH AP trigger Doze low power mode reject initial code setting
		//if(!fih_get_aod()){
		//	return;
		//}
	}

	if(ctrl->panel_data.panel_info.panel_id == FIH_ILI7807E_1080P_VIDEO_PANEL)
	{
	    mdss_dsi_panel_ili7807E_low_power_config(pdata, enable);
	}
	else
	{
		pr_err("[AOD]%s, the panel don't support Doze mode\n",__func__);
	}

	pr_err("[AOD][HL]%s: <-- END\n", __func__);

	return;
}

void mdss_lp_early_config(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pinfo = &pdata->panel_info;

	if (ctrl_pdata->low_power_config&& ctrl_pdata->cmd_early_lp_exit){
		pr_err("[AOD]%s: dsi_unblank with panel always on\n", __func__);
		ctrl_pdata->low_power_config(pdata, false);
	}
	return;
}

void mdss_aod_resume_config(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	int len = 1;

	pr_err("[AOD][HL]%s: <-- START\n", __func__);

	if (pdata == NULL)
	{
		pr_err("[AOD]%s: Invalid input data\n", __func__);
		return;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if(pinfo->aod_ready_on)
	{
		pr_err("[AOD]%s, aod_ready_on is true, will exit glance\n", __func__);

		mutex_lock(&mfd_glance->update.lock);
			if (ctrl->aod_resume_cmds.cmd_cnt){
				len = mdss_dsi_panel_cmds_send(ctrl, &ctrl->aod_resume_cmds, CMD_REQ_COMMIT);
				if(!len)
					pr_err("[AOD]%s: BossVee quit IDLE fail\n", __func__);
				else{
					mdss_fb_mode_switch(mfd_glance, 0);
					pr_err("[AOD]%s: BossVee switch VIDEO success\n", __func__);
				}
			}
		mutex_unlock(&mfd_glance->update.lock);
			//mdss_dsi_panel_cmds_send(ctrl, &ctrl->panel_on_cmds, CMD_REQ_COMMIT);
			pinfo->aod_ready_on = 0;
		pr_err("[AOD]%s, BossVee exit glance success\n", __func__);
	}
	else
	{
		pr_err("[AOD]%s: BossVee aod_ready_on is false, do nothing\n", __func__);
	}

	pr_err("[AOD][HL]%s: <-- END\n", __func__);

	return;
}


int fih_set_low_power_mode(int enable)
{
	struct mdss_panel_data *pdata = &gpdata->panel_data;
	//struct mdss_panel_data *sdata = &spdata->panel_data;
	//struct mdss_panel_data *adata;
//	struct mdss_panel_info *pinfo;
	//int i =0;

	pr_err("[AOD][HL]%s: <-- START\n", __func__);

	if(!aod_feature)
	{
		pr_err("[AOD]Not support AOD feature\n");
		return 0;
	}

	//If it is not FIH AP trigger Doze low power mode reject initial code setting
	if(!fih_get_aod())
	{
		pr_err("[AOD]AOD is NOT enabled! Do nothing and return 0\n");
		return 0;
	}
	else
	{
		pr_err("[AOD]AOD is enabled! Keep going!\n");
	}

	pr_err("[AOD]%s, the panel %s Doze mode\n",__func__,enable?"enter":"leave");

	if(!enable)
	{
		pdata->set_backlight(pdata, 0);
		pr_err("[AOD][HL]%s: pdata->set_backlight(pdata, 0)\n", __func__);
	}

	mdss_dsi_panel_ili7807E_low_power_config(pdata, enable);

	pr_err("[AOD][HL]%s: <-- END\n", __func__);

	return 0;
}
EXPORT_SYMBOL(fih_set_low_power_mode);

int fih_get_low_power_mode(void)
{
	struct mdss_panel_info *pinfo;

	pr_err("[AOD][HL]%s: <-- START\n", __func__);

	pinfo = &gpdata->panel_data.panel_info;

	if(!aod_feature)
	{
		pr_debug("Not support AOD feature");
		return 0;
	}

	pr_err("[AOD][HL]%s: return pinfo->aod_ready_on <-- END\n", __func__);

	return pinfo->aod_ready_on;

}
EXPORT_SYMBOL(fih_get_low_power_mode);

void fih_mdss_dsi_aod_panel_init(struct platform_device *pdev,struct mdss_dsi_ctrl_pdata *ctrl_pdata,u32 ctrl_id)
{
	pr_err("[AOD][HL]%s: <-- START\n", __func__);

	if(ctrl_id==DSI_CTRL_0)
	{
		gpdata = devm_kzalloc(&pdev->dev,
					  sizeof(struct mdss_dsi_ctrl_pdata),
					  GFP_KERNEL);
		if (!gpdata) {
			pr_err("[AOD]%s: FAILED: cannot alloc dsi ctr - gpdata\n",__func__);
			goto error_no_mem;
		}
		gpdata = mdss_dsi_get_ctrl(ctrl_id);
		platform_set_drvdata(pdev, gpdata);


	}else if(ctrl_id==DSI_CTRL_1)
	{
		spdata = devm_kzalloc(&pdev->dev,
					  sizeof(struct mdss_dsi_ctrl_pdata),
					  GFP_KERNEL);
		if (!spdata) {
			pr_err("[AOD]%s: FAILED: cannot alloc dsi ctr - spdata\n",__func__);
			goto error_no_mem;
		}
		spdata = mdss_dsi_get_ctrl(ctrl_id);
		platform_set_drvdata(pdev, spdata);

	}else
	{
		pr_err("[AOD]%s: Out of bus number(%d)\n",__func__,ctrl_id);
		return;
	}

	pr_debug("\n\n***%s, aod_feature = %d ***\n\n", __func__, aod_feature);
	if(aod_feature>=DSI_CTRL_MAX){
		pr_debug("\n\n***%s, aod_feature, probe ok return  ***\n\n", __func__);
		return;
	}

	//mfd = platform_get_drvdata(pdev);

	if(((ctrl_pdata->aod_resume_cmds.cmd_cnt) &&
		(ctrl_pdata->aod_suspend_cmds.cmd_cnt))){
		aod_feature++;
	}

	pr_debug("\n\n***%s, aod_feature = %d ***\n\n", __func__, aod_feature);
	pr_debug("\n\n***%s, probe pass return \n\n", __func__);

	pr_err("[AOD][HL]%s: return; <-- END\n", __func__);

	return;
error_no_mem:
	pr_err("[AOD][HL]%s: error_no_mem, return; <-- END\n", __func__);

	if(ctrl_id==DSI_CTRL_0)
		devm_kfree(&pdev->dev, gpdata);
	else if(ctrl_id==DSI_CTRL_1)
		devm_kfree(&pdev->dev, spdata);
	return;
}

