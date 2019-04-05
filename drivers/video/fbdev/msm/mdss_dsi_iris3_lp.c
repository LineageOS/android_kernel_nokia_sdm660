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
#include "mdss_fb.h"
#include "mdss_dsi.h"
#include "mdss_dsi_iris3_lp.h"
#include "mdss_dsi_iris3_lightup.h"
#include "mdss_dsi_iris3_def.h"
#include "mdss_dsi_iris3_lightup_ocp.h"
#include "mdss_dsi_iris3_pq.h"

static int gpio_pulse_delay = 16 * 16 * 4 * 10 / POR_CLOCK;
static int gpio_cmd_delay = 10;

static struct msm_fb_data_type *g_debug_mfd;
static int debug_lp_opt;

// abyp light up option (need panel off/on to take effect)
// 	0: panel on with pt;
// 	1: panel on with abyp;
// 	2: panel on with abyp & low power(need connect wakeup pin)
static int debug_abyp_opt = 0;

static bool iris_lce_power;

/* set iris low power */
void iris_lp_set(void)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();
	pcfg->abypss_ctrl.abypass_mode = PASS_THROUGH_MODE;
	iris_one_wired_cmd_init(pcfg->ctrl);
}

/* dynamic power gating set */
void iris_dynamic_power_set(bool enable)
{
	struct iris_update_ipopt popt[IP_OPT_MAX];
	struct iris_cfg *pcfg;
	int len;

	pcfg = iris_get_cfg();

	iris_init_ipopt_ip(popt, IP_OPT_MAX);

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SYS,
			enable ? 0xf0 : 0xf1, 0x1);

	/* 0xf0: read; 0xf1: non-read */
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_DMA,
			enable ? 0xf0 : 0xf1, 0x0);

	iris_update_pq_opt(popt, len);

	pcfg->dynamic_power = enable;
	pr_debug("%s: %d\n", __func__, enable);
}

/* trigger DMA to load */
void iris_dma_trigger_load(void)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();
	/* only effective when dynamic power gating off */
	if (!pcfg->dynamic_power) {
		if (iris_lce_power_status_get())
			iris_send_ipopt_cmds(IRIS_IP_DMA, 0xe1);
		else
			iris_send_ipopt_cmds(IRIS_IP_DMA, 0xe5);
	}
}

/* dynamic power gating get */
bool iris_dynamic_power_get(void)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();

	return pcfg->dynamic_power;
}

/* TE delay or EVS delay select.
   0: TE delay; 1: EVS delay */
void iris_te_select(int sel)
{
	struct iris_update_ipopt popt[IP_OPT_MAX];
	int len;

	iris_init_ipopt_ip(popt, IP_OPT_MAX);

	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_SYS,
			sel ? 0xe1 : 0xe0, 0x1);
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_TX,
			sel ? 0xe1 : 0xe0, 0x1);
	len = iris_update_ip_opt(popt, IP_OPT_MAX, IRIS_IP_DTG,
			sel ? 0xe1 : 0xe0, 0x0);
	iris_update_pq_opt(popt, len);

	pr_debug("%s: %s\n", __func__, (sel ? "EVS delay" : "TE delay"));
}

/* power on/off core domain via PMU */
void iris_pmu_core_set(bool enable)
{
	struct iris_update_regval regval;
	struct iris_update_ipopt popt;

	regval.ip = IRIS_IP_SYS;
	regval.opt_id = 0xfc;
	regval.mask = 0x00000004;
	regval.value = (enable ? 0x4 : 0x0);
	iris_update_bitmask_regval_nonread(&regval, false);
	iris_init_update_ipopt(&popt, IRIS_IP_SYS, 0xfc, 0xfc, 0);
	iris_update_pq_opt(&popt, 1);

	pr_debug("%s: %d\n", __func__, enable);
}

/* power on/off lce domain via PMU */
void iris_pmu_lce_set(bool enable)
{
	struct iris_update_regval regval;
	struct iris_update_ipopt popt;

	regval.ip = IRIS_IP_SYS;
	regval.opt_id = 0x00fc;
	regval.mask = 0x00000020;
	regval.value = (enable ? 0x20 : 0x0);
	iris_update_bitmask_regval_nonread(&regval, false);
	iris_init_update_ipopt(&popt, IRIS_IP_SYS, 0xfc, 0xfc, 0);
	iris_update_pq_opt(&popt, 1);

	iris_lce_power_status_set(enable);

	pr_debug("%s: %d\n", __func__, enable);
}

/* lce dynamic pmu mask enable */
void iris_lce_dynamic_pmu_mask_set(bool enable)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();

	if (pcfg->dynamic_power) {
		struct iris_update_regval regval;
		struct iris_update_ipopt popt;

		regval.ip = IRIS_IP_SYS;
		regval.opt_id = 0xf0;
		regval.mask = 0x00000080;
		regval.value = (enable ? 0x80 : 0x0);
		iris_update_bitmask_regval_nonread(&regval, false);
		iris_init_update_ipopt(&popt, IRIS_IP_SYS,
				regval.opt_id, regval.opt_id, 0);
		iris_update_pq_opt(&popt, 1);
		pr_info("%s: %d\n", __func__, enable);
	} else
		pr_err("%s: %d. Dynmaic power is off!\n", __func__, enable);
}

int iris_one_wired_cmd_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int one_wired_gpio = 0;
	struct iris_cfg *pcfg = NULL;

	if (!ctrl)
		return -EINVAL;

	pcfg = iris_get_cfg();
	one_wired_gpio = ctrl->hdr_wakeup_gpio;

	pr_info("%s: %d\n", __func__, __LINE__);

	if (!gpio_is_valid(one_wired_gpio)) {
		pcfg->abypss_ctrl.analog_bypass_disable = true;

		pr_err("%s:%d, one wired GPIO not configured\n",
			   __func__, __LINE__);
		return 0;
	}

	gpio_direction_output(one_wired_gpio, 0);

	return 0;
}

/* send one wired commands via GPIO */
void iris_one_wired_cmd_send(
		struct mdss_dsi_ctrl_pdata *ctrl, int cmd)
{
	int cnt = 0;
	u32 start_end_delay = 0, pulse_delay = 0;
	unsigned long flags;
	struct iris_cfg *pcfg;
	int one_wired_gpio = ctrl->hdr_wakeup_gpio;

	pcfg = iris_get_cfg();

	if (!gpio_is_valid(one_wired_gpio)) {
		pr_err("%s:%d, one wired GPIO not configured\n",
			   __func__, __LINE__);
		return;
	}

	start_end_delay = 16 * 16 * 16 * 10 / POR_CLOCK;  /*us*/
	pulse_delay = gpio_pulse_delay;  /*us*/

	pr_info("cmd:%d, pulse:%d, delay:%d\n",
			cmd, pulse_delay, gpio_cmd_delay);
	if (1 == cmd)
		pr_info("POWER_UP_SYS\n");
	else if (2 == cmd)
		pr_info("ENTER_ANALOG_BYPASS\n");
	else if (3 == cmd)
		pr_info("EXIT_ANALOG_BYPASS\n");
	else if (4 == cmd)
		pr_info("POWER_DOWN_SYS\n");

	spin_lock_irqsave(&pcfg->iris_lock, flags);
	for (cnt = 0; cnt < cmd; cnt++) {
		gpio_set_value(one_wired_gpio, 1);
		udelay(pulse_delay);
		gpio_set_value(one_wired_gpio, 0);
		udelay(pulse_delay);
	}
	udelay(gpio_cmd_delay);
	spin_unlock_irqrestore(&pcfg->iris_lock, flags);
	/*end*/
	udelay(start_end_delay);
}

static ssize_t iris_abyp_dbg_write(struct file *file,
	const char __user *buff, size_t count, loff_t *ppos)
{
	unsigned long val;
	struct iris_cfg *pcfg;
	static int cnt;

	pcfg = iris_get_cfg();

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	if (0 == val) {
		iris_abypass_switch_proc(g_debug_mfd, PASS_THROUGH_MODE);
		pr_info("analog bypass->pt, %d\n", cnt);
	} else if (1 == val) {
		iris_abypass_switch_proc(g_debug_mfd, ANALOG_BYPASS_MODE);
		pr_info("pt->analog bypass, %d\n", cnt);
	} else if (11 <= val && val <= 18) {
		pr_info("%s one wired %d\n", __func__, (int)(val - 10));
		iris_one_wired_cmd_send(pcfg->ctrl, (int)(val - 10));
	} else if (20 == val) {
		iris_send_ipopt_cmds(IRIS_IP_SYS, 5);
		pr_info("miniPMU analog bypass->pt\n");
	} else if (21 == val) {
		iris_send_ipopt_cmds(IRIS_IP_SYS, 4);
		pr_info("miniPMU pt->analog bypass\n");
	} else if (22 == val) {
		iris_send_ipopt_cmds(IRIS_IP_TX, 4);
		pr_info("Enable Tx\n");
	} else if (23 == val) {
		mutex_lock(&g_debug_mfd->switch_lock);
		iris_lightup(pcfg->ctrl, &pcfg->abyp_panel_cmds);
		mutex_unlock(&g_debug_mfd->switch_lock);
		pr_info("lightup Iris abyp_panel_cmds\n");
	}

	return count;
}

static ssize_t iris_lp_dbg_write(struct file *file,
	const char __user *buff, size_t count, loff_t *ppos)
{
	unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	if (0 == val) {
		iris_dynamic_power_set(false);
		udelay(100);
		iris_dma_trigger_load();
	} else if (1 == val)
		iris_dynamic_power_set(true);
	else if (2 == val)
		pr_err("%s: dynamic power -- %d\n", __func__, iris_dynamic_power_get());
	else if (3 == val)
		iris_te_select(0);
	else if (4 == val)
		iris_te_select(1);
	 else if (5 == val)
		/* for debug */
		iris_pmu_core_set(true);
	 else if (6 == val)
		/* for debug */
		iris_pmu_core_set(false);
	 else if (7 == val)
		/* for debug */
		iris_pmu_lce_set(true);
	 else if (8 == val)
		/* for debug */
		iris_pmu_lce_set(false);

	return count;
}

static const struct file_operations iris_abyp_dbg_fops = {
	.open = simple_open,
	.write = iris_abyp_dbg_write,
};

static const struct file_operations iris_lp_dbg_fops = {
	.open = simple_open,
	.write = iris_lp_dbg_write,
};

int iris3_debugfs_init(struct msm_fb_data_type *mfd)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();
	if (mfd->index != 0)
		return 0;
	if (!(mfd->panel.type == MIPI_VIDEO_PANEL
		|| mfd->panel.type == MIPI_CMD_PANEL))
		return 0;

	pr_info("%s mfd %p\n", __func__, mfd);

	g_debug_mfd = mfd;

	pcfg->dbg_root = debugfs_create_dir("iris", NULL);
	if (IS_ERR_OR_NULL(pcfg->dbg_root)) {
		pr_err("debugfs_create_dir for iris_debug failed, error %ld\n",
		       PTR_ERR(pcfg->dbg_root));
		return -ENODEV;
	}

	debugfs_create_u32("pulse_delay", 0644, pcfg->dbg_root,
		(u32 *)&gpio_pulse_delay);

	debugfs_create_u32("cmd_delay", 0644, pcfg->dbg_root,
		(u32 *)&gpio_cmd_delay);

	debugfs_create_u32("lp_opt", 0644, pcfg->dbg_root,
		(u32 *)&debug_lp_opt);

	debugfs_create_u32("on_abyp", 0644, pcfg->dbg_root,
		(u32 *)&debug_abyp_opt);

	if (debugfs_create_file("abyp", 0644, pcfg->dbg_root, mfd,
				&iris_abyp_dbg_fops) == NULL) {
		pr_err("%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return -EFAULT;
	}

	if (debugfs_create_file("lp", 0644, pcfg->dbg_root, mfd,
				&iris_lp_dbg_fops) == NULL) {
		pr_err("%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

bool iris_pt_to_abypass_switch(struct msm_fb_data_type *mfd)
{
	u32 frame = 0;
	struct iris_cfg *pcfg;
	struct mdss_mdp_ctl *ctl;

	pcfg = iris_get_cfg();

	frame = mdss_panel_get_framerate
			(&(pcfg->ctrl->panel_data.panel_info));
	if (!(frame >= 24 && frame <= 240))
		frame = 24;
	frame = ((1000/frame) + 1);

	ctl = mfd_to_ctl(mfd); /* returns master ctl */
	mutex_lock(&ctl->lock);
	mutex_lock(&ctl->flush_lock);
	mdss_mdp_display_wait4pingpong(ctl, false);
	mdss_dsi_cmd_mdp_busy(pcfg->ctrl);
	mdss_dsi_wait_for_lane_idle(pcfg->ctrl);

	/* enter analog bypass */
	iris_one_wired_cmd_send(pcfg->ctrl, ENTER_ANALOG_BYPASS);
	pr_info("enter analog bypass switch\n");

	udelay(frame*1000); /* wait for 1 frames */

	/* power down sys */
	iris_one_wired_cmd_send(pcfg->ctrl, POWER_DOWN_SYS);

	mutex_unlock(&ctl->flush_lock);
	mutex_unlock(&ctl->lock);

	pr_info("sys power down\n");
	pcfg->abypss_ctrl.abypass_mode = ANALOG_BYPASS_MODE;

	pr_err("Enter analog bypass mode\n");

	return false;
}

bool iris_abypass_to_pt_switch(struct msm_fb_data_type *mfd)
{
	u32 frame = 0;
	struct iris_cfg *pcfg;
	struct mdss_mdp_ctl *ctl;

	pcfg = iris_get_cfg();

	frame = mdss_panel_get_framerate
			(&(pcfg->ctrl->panel_data.panel_info));
	if (!(frame >= 24 && frame <= 240))
		frame = 24;
	frame = ((1000/frame) + 1);

	pr_info("%s:%d\n", __func__, __LINE__);
	ctl = mfd_to_ctl(mfd); /* returns master ctl */
	mutex_lock(&ctl->lock);
	mutex_lock(&ctl->flush_lock);
	mdss_mdp_display_wait4pingpong(ctl, false);
	mdss_dsi_cmd_mdp_busy(pcfg->ctrl);
	mdss_dsi_wait_for_lane_idle(pcfg->ctrl);

	/* power up sys */
	iris_one_wired_cmd_send(pcfg->ctrl, POWER_UP_SYS);
	pr_info("power up sys\n");

	udelay(10 * 1000); /* wait for 10ms */

#if defined(IRIS_CHECK_DPHY_STATE)
	iris_write_dphy_err_proc(pcfg->ctrl);
	iris_write_dphy_err_proc(pcfg->ctrl);
	iris_write_dphy_err_proc(pcfg->ctrl);
#endif

	pr_info("dphy_err_proc\n");

	iris_send_ipopt_cmds(IRIS_IP_TX, 4); /* enable mipi Tx (LP) */

	pr_info("enable mipi tx\n");

	/* exit analog bypass */
	iris_one_wired_cmd_send(pcfg->ctrl, EXIT_ANALOG_BYPASS);
	pr_err("exit analog bypass\n");

	udelay(frame*1000); /* wait for 1 frame */

	/* light up iris */
	if ((debug_lp_opt & 0x2) == 0) {
		if (!pcfg->abyp_panel_cmds.cmds)
			iris_lightup(pcfg->ctrl, &pcfg->abyp_panel_cmds);
		else
			iris_lightup(pcfg->ctrl, NULL);
		pr_err("light up iris\n");
	}

	mutex_unlock(&ctl->flush_lock);
	mutex_unlock(&ctl->lock);

	pcfg->abypss_ctrl.abypass_mode = PASS_THROUGH_MODE;

	return false;
}

void iris_abypass_switch_proc(struct msm_fb_data_type *mfd, int mode)
{
	struct iris_cfg *pcfg;

	pcfg = iris_get_cfg();

	if (pcfg->abypss_ctrl.analog_bypass_disable) {
		pr_err("gpio is not setting for abypass\n");
		return;
	}

	if (mode == pcfg->abypss_ctrl.abypass_mode)
		return;

	if (mode == ANALOG_BYPASS_MODE)
		iris_pt_to_abypass_switch(mfd);
	else if (mode == PASS_THROUGH_MODE)
		iris_abypass_to_pt_switch(mfd);
	else
		pr_err("%s: switch mode: %d not supported!\n", __func__, mode);
}

/* Use Iris3 Analog bypass mode to light up panel */
void iris_abyp_lightup(struct mdss_dsi_ctrl_pdata *ctrl)
{
#ifdef IRIS3_MIPI_TEST
	iris_read_chip_id();
#endif

	/* enable mipi Tx */
	iris_send_ipopt_cmds(IRIS_IP_TX, 4);

	/* enter analog bypass */
	/*iris_one_wired_cmd_send(ctrl, ENTER_ANALOG_BYPASS);*/
	iris_send_ipopt_cmds(IRIS_IP_SYS, 4);

	/* delay for Iris to enter bypass mode */
	udelay(10 * 1000);

#ifdef IRIS3_MIPI_TEST
	iris_read_power_mode(ctrl);
#endif

	pr_info("%s\n", __func__);

	if (debug_abyp_opt == 2) {
		//power down sys
		iris_one_wired_cmd_send(ctrl, POWER_DOWN_SYS);
		pr_info("%s power down sys\n", __func__);
	}
}

/* Exit Iris3 Analog bypass mode*/
void iris_abyp_lightup_exit(struct mdss_dsi_ctrl_pdata *ctrl, bool one_wire)
{
	/* exit analog bypass */
	if (one_wire)
		iris_one_wired_cmd_send(ctrl, EXIT_ANALOG_BYPASS);
	else
		iris_send_ipopt_cmds(IRIS_IP_SYS, 5);

	/* delay for Iris to exit bypass mode */
	udelay(10 * 1000);
	udelay(10 * 1000);

	pr_info("%s\n", __func__);
}

void iris_lce_power_status_set(bool enable)
{
	iris_lce_power = enable;

	pr_info("%s: %d\n", __func__, enable);
}

bool iris_lce_power_status_get(void)
{
	return iris_lce_power;
}

int iris_abyp_lightup_get(void)
{
	return debug_abyp_opt;
}
