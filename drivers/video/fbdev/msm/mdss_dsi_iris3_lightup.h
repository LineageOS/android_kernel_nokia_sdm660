#ifndef MDSS_DSI_IRIS_LIGHTUP_H
#define MDSS_DSI_IRIS_LIGHTUP_H


#include <linux/completion.h>
#include "mdss_dsi_iris3.h"


#define IRIS_IP_OPT_CNT   15
#define IRIS_IP_CNT      19

#define IRIS_CHIP_CNT   2
/*workaround for checking rx error*/
#define IRIS_CHECK_DPHY_STATE
#define IRIS3_MIPI_TEST

/*use to parse dtsi cmd list*/
struct iris_parsed_hdr {
	uint32_t dtype;  /* dsi command type 0x23 0x29*/
	uint32_t last; /*last in chain*/
	uint32_t wait; /*wait time*/
	uint32_t ip; /*ip type*/
	uint32_t opt; /*ip option and lp or hs*/
	uint32_t dlen; /*payload len*/
};


/* iris ip option, it will create according to opt_id.
*  link_state will be create according to the last cmds
*/
struct iris_ip_opt {
	uint8_t opt_id; /*option identifier*/
	uint8_t len; /*option length*/
	uint8_t link_state; /*high speed or low power*/
	struct dsi_cmd_desc *cmd; /*the first cmd of desc*/
};

/*ip search index*/
struct iris_ip_index {
	int32_t opt_cnt; /*ip option number*/
	struct iris_ip_opt *opt; /*option array*/
};

struct iris_pq_ipopt_val {
	int32_t opt_cnt;
	uint8_t ip;
	uint8_t *popt;
};

struct iris_pq_init_val {
	int32_t ip_cnt;
	struct iris_pq_ipopt_val  *val;
};

struct iris_cmd_statics {
	int cnt;
	int len;
};

/*used to control iris_ctrl opt sequence*/
struct iris_ctrl_opt {
	uint8_t ip;
	uint8_t opt_id;
	uint8_t skip_last;
	uint8_t reserved;
};

struct iris_ctrl_seq {
	int32_t cnt;
	struct iris_ctrl_opt *ctrl_opt;
};

struct iris_update_ipopt {
	uint8_t ip;
	uint8_t opt_old;
	uint8_t opt_new;
	uint8_t skip_last;
};

struct iris_update_regval {
	uint8_t ip;
	uint8_t opt_id;
	uint16_t reserved;
	uint32_t mask;
	uint32_t value;
};

struct iris_abypass_ctrl {
	bool analog_bypass_disable;
	uint8_t abypass_mode;
};

/*will pack all the commands here*/
struct iris_out_cmds {
	/* will be used before cmds sent out */
	struct dsi_cmd_desc *iris_cmds_buf;
	u32 cmds_index;
};

/*iris lightup configure commands*/
struct iris_cfg {
	bool dynamic_power;
	bool cont_splash_enabled;
	uint8_t panel_type;
	uint16_t panel_nits;
	uint8_t chip_id;
	uint8_t power_mode;
	uint8_t valid;			/* 0: none, 1: parse ok, 2: minimum light up, 3. full light up */
	uint8_t lut_mode;
	uint8_t fs_curr;
	uint32_t add_last_flag;
	uint32_t add_on_last_flag;	/* panel on packet size */
	uint32_t add_cont_last_flag;	/* cont-splash packet size */
	uint32_t add_pt_last_flag;		/* panel on finished or cont-splash finished packet size */
	uint32_t split_pkt_size;
	uint32_t lut_cmds_cnt;
	uint32_t none_lut_cmds_cnt;
	uint32_t panel_dimming_brightness;
	int32_t vblank_line;
	spinlock_t iris_lock;
	struct mutex mutex;
	uint8_t name[MDSS_MAX_PANEL_LEN];
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct led_trigger *bl_led;
	struct dentry *dbg_root;
	struct iris_ip_index  ip_index_arr[IRIS_IP_CNT];
	struct dsi_panel_cmds  cmds;
	struct dsi_panel_cmds abyp_panel_cmds;
	struct iris_ctrl_seq   ctrl_seq[IRIS_CHIP_CNT];
	struct iris_ctrl_seq   ctrl_seq_cs[IRIS_CHIP_CNT];
	struct iris_pq_init_val  pq_init_val;
	struct iris_abypass_ctrl abypss_ctrl;
	struct iris_out_cmds iris_cmds;
	uint32_t min_color_temp;
	uint32_t max_color_temp;
	struct work_struct cont_splash_work;
	struct completion frame_ready_completion;
};

struct iris_cmd_comp {
	int32_t link_state;
	int32_t cnt;
	struct dsi_cmd_desc *cmd;
};

struct iris_cmd_priv {
	int32_t cmd_num;
	struct iris_cmd_comp cmd_comp[IRIS_IP_OPT_CNT];
};

typedef int (*iris_parse_dcs_cmds_cb)(
		struct device_node *np,
		struct dsi_panel_cmds *pcmds,
		char *cmd_key, char *link_key);

struct iris_cfg *iris_get_cfg(void);

void iris_parse_params(
		struct device_node *np,
		struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		iris_parse_dcs_cmds_cb func);

void iris_out_cmds_buf_reset(void);

void iris_lightup(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *on_cmds);

void iris_lightoff(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *off_cmds);

void iris_send_ipopt_cmds(int32_t ip, int32_t opt_id);

int32_t iris_dsi_get_cmd_comp(
		struct iris_cfg *plgtup_cfg, int32_t ip,
		int32_t opt_index, struct iris_cmd_priv *pcmd_priv);

int32_t  iris_dsi_send_cmds(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct iris_cmd_priv *pcmd_priv);

int32_t iris_dump_cmd_payload(struct iris_cmd_priv  *pcmd_priv);

void iris_update_pq_opt(struct iris_update_ipopt *popt, int len);

void iris_update_bitmask_regval(
		struct iris_update_regval *pregval, bool is_commit);

void iris_update_bitmask_regval_nonread(
		struct iris_update_regval *pregval, bool is_commit);


void iris_init_update_ipopt(
		struct iris_update_ipopt *popt,
		uint8_t ip, uint8_t opt_old,
		uint8_t opt_new, uint8_t skip_last);

struct iris_pq_ipopt_val *iris_get_cur_ipopt_val(uint8_t ip);

int iris_init_update_ipopt_t(
		struct iris_update_ipopt *popt,  int len,
		uint8_t ip, uint8_t opt_old,
		uint8_t opt_new, uint8_t skip_last);

/*
* @description  get assigned position data of ip opt
* @param ip       ip sign
* @param opt_id   option id of ip
* @param pos      the position of option payload
* @return   fail NULL/success payload data of position
*/
uint32_t  *iris_get_ipopt_payload_data(
		uint8_t ip, uint8_t opt_id, int32_t pos);

/* Use Iris3 Analog bypass mode to light up panel */
void iris_abyp_lightup(struct mdss_dsi_ctrl_pdata *ctrl);

/* Get Iris3 abyp lightup opt */
int iris_abyp_lightup_get(void);

/* Exit Iris3 Analog bypass mode*/
void iris_abyp_lightup_exit(
		struct mdss_dsi_ctrl_pdata *ctrl, bool one_wire);

#if defined(IRIS_CHECK_DPHY_STATE)
void iris_write_dphy_err_proc(struct mdss_dsi_ctrl_pdata *ctrl);
#endif

void iris_read_chip_id(void);
void iris_read_power_mode(struct mdss_dsi_ctrl_pdata *ctrl);

#endif
