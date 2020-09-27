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
#include "../mdss.h"
#include "../mdss_panel.h"
#include "../mdss_dsi.h"
#include "../mdss_fb.h"

extern int aod_en;
extern int aod_feature;
extern int blank_mode;
extern int wled_aod_set;


extern void fih_mdss_dsi_aod_panel_init(struct platform_device *pdev,struct mdss_dsi_ctrl_pdata *ctrl_pdata,u32 ctrl_id);
extern int mdss_dsi_panel_LH530QH1_low_power_config(struct mdss_panel_data *pdata,int enable);
extern void fih_mdss_dsi_panel_config_aod_parse_dt(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata);
extern void fih_mdss_dsi_panel_config_aod_res_properties(struct device_node *np, struct dsi_panel_timing *pt);
extern void fih_mdss_lp_config(struct mdss_panel_data *pdata,int enable,int ndx);
extern void fih_mdss_dsi_panel_aod_exit_register(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_timing *pt);
extern int fih_get_low_power_mode(void);
extern int fih_set_low_power_mode(int enable);

extern unsigned int fih_get_panel_id(void);
extern int fih_set_aod(int enable);
extern int fih_get_aod(void);
extern int fih_get_blank_mode(void);
extern int fih_set_blank_mode(int mode);
extern int fih_get_aod_wled_state(void);
extern int fih_set_aod_wled_state(int enable);

extern void mdss_lp_early_config(struct mdss_panel_data *pdata);
extern void mdss_aod_resume_config(struct mdss_panel_data *pdata);
extern void fih_mdss_fb_aod_register(struct platform_device *pdev,struct msm_fb_data_type *mfd);
extern void fih_mdss_fb_get_data(struct msm_fb_data_type *mfd);
