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
#include "mdss_dsi_iris3_lightup.h"


#define IRIS_TX_HV_PAYLOAD_LEN   120
#define IRIS_TX_PAYLOAD_LEN 124
#define IRIS_PT_RD_CMD_NUM 3
#define IRIS_RD_PACKET_DATA  0xF0C1C018

static char iris_read_cmd_rbuf[16];
static struct iris_ocp_cmd ocp_cmd;
static struct iris_ocp_cmd ocp_test_cmd[DSI_CMD_CNT];
static struct dsi_cmd_desc iris_test_cmd[DSI_CMD_CNT];

/*static char test_value_char[16]=
	{0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};*/

static void iris_cmd_reg_add(
		struct iris_ocp_cmd *pcmd, u32 addr, u32 val)
{
	*(u32 *)(pcmd->cmd + pcmd->cmd_len) = cpu_to_le32(addr);
	*(u32 *)(pcmd->cmd + pcmd->cmd_len + 4) = cpu_to_le32(val);
	pcmd->cmd_len += 8;
}

static void iris_cmd_add(struct iris_ocp_cmd *pcmd, u32 payload)
{
	*(u32 *)(pcmd->cmd + pcmd->cmd_len) = cpu_to_le32(payload);
	pcmd->cmd_len += 4;
}

void iris_ocp_write(u32 address, u32 value)
{
	struct dsi_panel_cmds panel_cmds;
	struct iris_ocp_cmd ocp_cmd;
	struct iris_cfg *pcfg = iris_get_cfg();
	struct dsi_cmd_desc iris_ocp_cmd[] = {
		{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, CMD_PKT_SIZE}, ocp_cmd.cmd}, };

	memset(&ocp_cmd, 0, sizeof(ocp_cmd));

	iris_cmd_add(&ocp_cmd, 0xFFFFFFF0 | OCP_SINGLE_WRITE_BYTEMASK);
	iris_cmd_reg_add(&ocp_cmd, address, value);
	iris_ocp_cmd[0].dchdr.dlen = ocp_cmd.cmd_len;
	/* TODO: use pr_debug after confirm features.*/
	pr_err("[ocp][write]addr=0x%08x, value=0x%08x\n",
			address, value);

	panel_cmds.cmds = iris_ocp_cmd;
	panel_cmds.link_state = DSI_HS_MODE;
	panel_cmds.cmd_cnt = 1;
	iris_dsi_cmds_send(pcfg->ctrl, &panel_cmds);
}

void iris_ocp_write2(u32 header, u32 address, u32 size, u32 *pvalues)
{
	struct dsi_panel_cmds panel_cmds;
	struct iris_ocp_cmd ocp_cmd;
	struct iris_cfg *pcfg = iris_get_cfg();
	struct dsi_cmd_desc iris_ocp_cmd[] = {
		{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, CMD_PKT_SIZE}, ocp_cmd.cmd}, };
	u32 max_size = CMD_PKT_SIZE / 4 - 2;
	u32 i;

	while (size > 0) {
		memset(&ocp_cmd, 0, sizeof(ocp_cmd));

		iris_cmd_add(&ocp_cmd, header);
		iris_cmd_add(&ocp_cmd, address);
		if (size < max_size) {
			for (i = 0; i < size; i++)
				iris_cmd_add(&ocp_cmd, pvalues[i]);

			size = 0;
		} else {
			for (i = 0; i < max_size; i++)
				iris_cmd_add(&ocp_cmd, pvalues[i]);

			address += max_size * 4;
			pvalues += max_size;
			size -= max_size;
		}
		iris_ocp_cmd[0].dchdr.dlen = ocp_cmd.cmd_len;

		/* TODO: use pr_debug after confirm features. */
		pr_err("[ocp][write2]header=0x%08x, addr=0x%08x, dlen=%d\n",
				header, address, iris_ocp_cmd[0].dchdr.dlen);

		panel_cmds.cmds = iris_ocp_cmd;
		panel_cmds.link_state = DSI_HS_MODE;
		panel_cmds.cmd_cnt = 1;
		iris_dsi_cmds_send(pcfg->ctrl, &panel_cmds);
	}
}

void iris_ocp_write_address(u32 address, u32 mode)
{
	struct iris_ocp_cmd ocp_cmd;
	struct dsi_panel_cmds panel_cmds;
	struct iris_cfg *pcfg = iris_get_cfg();
	struct dsi_cmd_desc iris_ocp_cmd[] = {
		{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, CMD_PKT_SIZE}, ocp_cmd.cmd}, };

	/* Send OCP command.*/
	memset(&ocp_cmd, 0, sizeof(ocp_cmd));
	iris_cmd_add(&ocp_cmd, OCP_SINGLE_READ);
	iris_cmd_add(&ocp_cmd, address);
	iris_ocp_cmd[0].dchdr.dlen = ocp_cmd.cmd_len;

	panel_cmds.cmds = iris_ocp_cmd;
	panel_cmds.link_state = mode;
	panel_cmds.cmd_cnt = 1;
	iris_dsi_cmds_send(pcfg->ctrl, &panel_cmds);
}

u32 iris_ocp_read_value(u32 mode)
{
	struct dsi_panel_cmds panel_cmds;
	struct iris_cfg *pcfg = iris_get_cfg();

	char pi_read[1] = {0x00};
	struct dsi_cmd_desc pi_read_cmd[] = {
		{{DTYPE_GEN_READ1, 1, 0, 1, 0, sizeof(pi_read)}, pi_read}, };

	/* Read response.*/
	panel_cmds.cmds = pi_read_cmd;
	panel_cmds.link_state = mode;
	panel_cmds.cmd_cnt = 1;

	return iris_dsi_cmds_read_send(pcfg->ctrl, &panel_cmds);
}

u32 iris_ocp_read(u32 address, u32 mode)
{
	u32 value = 0;

	iris_ocp_write_address(address, mode);

	value = iris_ocp_read_value(mode);

	/*TODO: use pr_debug after confirm features.*/
	pr_debug("[ocp][read]addr=0x%x, value=0x%x\n", address, value);
	return value;
}

void iris_write_test(
		struct mdss_dsi_ctrl_pdata *ctrl, u32 iris_addr,
		int ocp_type, u32 pkt_size)
{
	union iris_ocp_cmd_header ocp_header;
	struct dsi_panel_cmds pcmds;
	struct dsi_cmd_desc iris_cmd = {
		{DTYPE_GEN_LWRITE, 1, 0, 0, 0, CMD_PKT_SIZE}, ocp_cmd.cmd};
	u32 test_value = 0xFFFF0000;

	memset(&ocp_header, 0, sizeof(ocp_header));
	ocp_header.header32 = 0xFFFFFFF0 | ocp_type;

	memset(&ocp_cmd, 0, sizeof(ocp_cmd));
	memcpy(ocp_cmd.cmd, &ocp_header.header32, OCP_HEADER);
	ocp_cmd.cmd_len = OCP_HEADER;

	switch (ocp_type) {
	case OCP_SINGLE_WRITE_BYTEMASK:
	case OCP_SINGLE_WRITE_BITMASK:
		for (; ocp_cmd.cmd_len <= (pkt_size - 8); ) {
			iris_cmd_reg_add(&ocp_cmd, iris_addr, test_value);
			test_value++;
			}
		break;

	case OCP_BURST_WRITE:
		test_value = 0xFFFF0000;
		iris_cmd_reg_add(&ocp_cmd, iris_addr, test_value);
		if (pkt_size <= ocp_cmd.cmd_len)
			break;
		test_value++;
		for (; ocp_cmd.cmd_len <= pkt_size - 4;) {
			iris_cmd_add(&ocp_cmd, test_value);
			test_value++;
		}
		break;
	default:
		break;
	}

	pr_info("len=0x%x iris_addr=0x%x  test_value=0x%x\n",
			ocp_cmd.cmd_len, iris_addr, test_value);

	iris_cmd.dchdr.dlen = ocp_cmd.cmd_len;
	pcmds.cmds = &iris_cmd;
	pcmds.blen = ocp_cmd.cmd_len;
	pcmds.link_state = DSI_HS_MODE;
	pcmds.cmd_cnt = 1;

	iris_dsi_cmds_send(ctrl, &pcmds);

	iris_dump_packet(ocp_cmd.cmd, ocp_cmd.cmd_len);

}


void iris_write_test_muti_pkt(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct iris_ocp_dsi_tool_input *ocp_input)
{
	union iris_ocp_cmd_header ocp_header;
	struct dsi_panel_cmds pcmds;

	u32 test_value = 0xFF000000;
	int iCnt = 0;

	u32 iris_addr, ocp_type, pkt_size, totalCnt;

	ocp_type = ocp_input->iris_ocp_type;
	test_value = ocp_input->iris_ocp_value;
	iris_addr = ocp_input->iris_ocp_addr;
	totalCnt = ocp_input->iris_ocp_cnt;
	pkt_size = ocp_input->iris_ocp_size;

	memset(iris_test_cmd, 0, sizeof(iris_test_cmd));
	memset(ocp_test_cmd, 0, sizeof(ocp_test_cmd));

	memset(&ocp_header, 0, sizeof(ocp_header));
	ocp_header.header32 = 0xFFFFFFF0 | ocp_type;

	switch (ocp_type) {
	case OCP_SINGLE_WRITE_BYTEMASK:
	case OCP_SINGLE_WRITE_BITMASK:

		for (iCnt = 0; iCnt < totalCnt; iCnt++) {

			memcpy(ocp_test_cmd[iCnt].cmd,
					&ocp_header.header32, OCP_HEADER);
			ocp_test_cmd[iCnt].cmd_len = OCP_HEADER;

			test_value = 0xFF000000;
			test_value = 0xFF000000 | (iCnt << 16);
			while (ocp_test_cmd[iCnt].cmd_len <= (pkt_size - 8)) {
				iris_cmd_reg_add(&ocp_test_cmd[iCnt],
					(iris_addr + iCnt*4), test_value);
				test_value++;
			}

			iris_test_cmd[iCnt].dchdr.dtype = DTYPE_GEN_LWRITE;
			iris_test_cmd[iCnt].dchdr.dlen = ocp_test_cmd[iCnt].cmd_len;
			iris_test_cmd[iCnt].payload = ocp_test_cmd[iCnt].cmd;
		}
		iris_test_cmd[totalCnt - 1].dchdr.last = true;
		break;

	case OCP_BURST_WRITE:
		for (iCnt = 0; iCnt < totalCnt; iCnt++) {
			memcpy(ocp_test_cmd[iCnt].cmd,
					&ocp_header.header32, OCP_HEADER);
			ocp_test_cmd[iCnt].cmd_len = OCP_HEADER;
			test_value = 0xFF000000;
			test_value = 0xFF000000 | (iCnt << 16);

			iris_cmd_reg_add(&ocp_test_cmd[iCnt],
					(iris_addr + iCnt*4), test_value);
			/*if(pkt_size <= ocp_test_cmd[iCnt].cmd_len)
				break;*/
			test_value++;
			while (ocp_test_cmd[iCnt].cmd_len <= pkt_size - 4) {
				iris_cmd_add(&ocp_test_cmd[iCnt], test_value);
				test_value++;
			}

			iris_test_cmd[iCnt].dchdr.dtype = DTYPE_GEN_LWRITE;
			iris_test_cmd[iCnt].dchdr.dlen = ocp_test_cmd[iCnt].cmd_len;
			iris_test_cmd[iCnt].payload = ocp_test_cmd[iCnt].cmd;

		}
		iris_test_cmd[totalCnt - 1].dchdr.last = true;
		break;
	default:
		break;

	}

	pr_info("%s totalCnt=0x%x iris_addr=0x%x  test_value=0x%x\n",
		__func__, totalCnt, iris_addr, test_value);

	pcmds.cmds = iris_test_cmd;
	pcmds.link_state = DSI_HS_MODE;
	pcmds.cmd_cnt = totalCnt;

	iris_dsi_cmds_send(ctrl, &pcmds);

	for (iCnt = 0; iCnt < totalCnt; iCnt++)
		iris_dump_packet(ocp_test_cmd[iCnt].cmd,
				ocp_test_cmd[iCnt].cmd_len);

}

u32  iris_dsi_cmds_read_send(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_panel_cmds *pcmds)
{
	int ret = 0;
	struct dcs_cmd_req cmdreq;
	u32 response_value;

	memset(iris_read_cmd_rbuf, 0, sizeof(iris_read_cmd_rbuf));

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_LP_MODE | CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 4;
	cmdreq.rbuf = iris_read_cmd_rbuf;
	cmdreq.cb = NULL;

	/*SOC default is LP mode*/
	if (pcmds->link_state == DSI_HS_MODE)
		cmdreq.flags |= CMD_REQ_HS_MODE;

	/*to make it easy, only enable TBG when cmd_cnt =1.*/
	if ((pcmds->cmd_cnt == 1)
		&& (pcmds->cmds->dchdr.dlen < DMA_TPG_FIFO_LEN)
		&& (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_103))
		cmdreq.flags |= CMD_REQ_DMA_TPG;

	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	pr_debug("read register %02x %02x %02x  %02x\n",
		ctrl->rx_buf.data[0], ctrl->rx_buf.data[1],
		ctrl->rx_buf.data[2], ctrl->rx_buf.data[3]);

	response_value = ctrl->rx_buf.data[0]
			| (ctrl->rx_buf.data[1] << 8)
			| (ctrl->rx_buf.data[2] << 16)
			| (ctrl->rx_buf.data[3] << 24);

#ifdef PXLW_IRIS3_FPGA
	iris_dump_packet(pcmds->cmds->payload, pcmds->cmds->dchdr.dlen);
#endif
	return response_value;
}

int  iris_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	int ret = 0;
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*SOC default is LP mode*/
	if (pcmds->link_state == DSI_HS_MODE)
		cmdreq.flags |= CMD_REQ_HS_MODE;

	/*to make it easy, only enable TBG when cmd_cnt =1.*/
	if ((pcmds->cmd_cnt == 1)
		&& (pcmds->cmds->dchdr.dlen < DMA_TPG_FIFO_LEN)
		&& (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_103))
		cmdreq.flags |= CMD_REQ_DMA_TPG;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);

#ifdef PXLW_IRIS3_FPGA
	iris_dump_packet(pcmds->cmds->payload, pcmds->cmds->dchdr.dlen);
#endif
	return ret;
}

static u32 iris_pt_split_pkt_cnt(int dlen)
{
	u32 sum = 1;

	if (dlen > IRIS_TX_HV_PAYLOAD_LEN)
		sum = (dlen - IRIS_TX_HV_PAYLOAD_LEN
			+ IRIS_TX_PAYLOAD_LEN - 1) / IRIS_TX_PAYLOAD_LEN + 1;
	return sum;
}


/*
* @Description: use to do statitics for cmds which should not less than 252
*    if the payload is out of 252, it will change to more than one cmds
the first payload need to be
	4 (ocp_header) + 8 (tx_addr_header + tx_val_header)
	+ 2* payload_len (TX_payloadaddr + payload_len)<= 252
the sequence payloader need to be
	4 (ocp_header) + 2* payload_len (TX_payloadaddr + payload_len)<= 252
	so the first payload should be no more than 120
	the second and sequence need to be no more than 124

* @Param: cmdreq  cmds request
* @return: the cmds number need to split
*/
static u32 iris_pt_calc_cmds_num(struct dcs_cmd_req *cmdreq)
{
	u32 i = 0;
	u32 sum = 0;
	u32 dlen = 0;

	for (i = 0; i < cmdreq->cmds_cnt; i++) {
		dlen = cmdreq->cmds[i].dchdr.dlen;

		sum += iris_pt_split_pkt_cnt(dlen);
	}
	return sum;
}

static u32 iris_pt_alloc_cmds_space(
		struct dcs_cmd_req *cmdreq, struct dsi_cmd_desc **ptx_cmds,
		struct iris_ocp_cmd **pocp_cmds)
{
	u32 len = 0;
	u32 cmds_cnt = 0;

	cmds_cnt = iris_pt_calc_cmds_num(cmdreq);

	len = cmds_cnt * sizeof(**ptx_cmds);
	*ptx_cmds = kmalloc(len, GFP_KERNEL);
	if (!(*ptx_cmds)) {
		pr_err("can not kmalloc len = %lu\n",
				cmds_cnt * sizeof(**ptx_cmds));
		return -ENOMEM;
	}

	len = cmds_cnt * sizeof(**pocp_cmds);
	*pocp_cmds = kmalloc(len, GFP_KERNEL);
	if (!(*pocp_cmds)) {
		pr_err("can not kmalloc pocp cmds\n");
		kfree(*ptx_cmds);
		*ptx_cmds = NULL;
		return -ENOMEM;
	}
	return cmds_cnt;
}

static void iris_pt_init_tx_cmd_header(
		struct dcs_cmd_req *cmdreq, struct dsi_cmd_desc *dsi_cmd,
		union iris_mipi_tx_cmd_header *header)
{
	u8 dtype = 0;

	dtype = dsi_cmd->dchdr.dtype;

	memset(header, 0x00, sizeof(*header));
	header->stHdr.dtype = dtype;
	header->stHdr.linkState = !!(cmdreq->flags & CMD_REQ_LP_MODE);
}


static void iris_pt_set_cmd_header(
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd, bool is_write)
{
	u32 dlen = 0;

	if (dsi_cmd == NULL)
		return;

	dlen = dsi_cmd->dchdr.dlen;

	if (is_write)
		pheader->stHdr.writeFlag = 0x01;
	else
		pheader->stHdr.writeFlag = 0x00;

	if (pheader->stHdr.longCmdFlag == 0) {
		if (dlen == 1)
			pheader->stHdr.len[0] = dsi_cmd->payload[0];
		else if (dlen == 2) {
			pheader->stHdr.len[0] = dsi_cmd->payload[0];
			pheader->stHdr.len[1] = dsi_cmd->payload[1];
		}
	} else {
		pheader->stHdr.len[0] = dlen & 0xff;
		pheader->stHdr.len[1] = (dlen >> 8) & 0xff;
	}
}

static void iris_pt_set_wrcmd_header(
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd)
{
	iris_pt_set_cmd_header(pheader, dsi_cmd, true);
}


static void iris_pt_set_rdcmd_header(
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd)
{
	iris_pt_set_cmd_header(pheader, dsi_cmd, false);
}

static void iris_pt_init_ocp_cmd(struct iris_ocp_cmd *pocp_cmd)
{
	union iris_ocp_cmd_header ocp_header;

	if (!pocp_cmd) {
		pr_err("pocp_cmd is null\n");
		return;
	}

	memset(pocp_cmd, 0x00, sizeof(*pocp_cmd));
	ocp_header.header32 = 0xfffffff0 | OCP_SINGLE_WRITE_BYTEMASK;
	memcpy(pocp_cmd->cmd, &ocp_header.header32, OCP_HEADER);
	pocp_cmd->cmd_len = OCP_HEADER;
}

static void iris_add_tx_cmds(
		struct dsi_cmd_desc *ptx_cmd,
		struct iris_ocp_cmd *pocp_cmd, u8 wait)
{
	struct dsi_cmd_desc  desc_init_val = {
		{DTYPE_GEN_LWRITE, 1, 0, 0, 0, CMD_PKT_SIZE}, NULL};

	memcpy(ptx_cmd, &desc_init_val, sizeof(struct dsi_cmd_desc));
	ptx_cmd->payload = pocp_cmd->cmd;
	ptx_cmd->dchdr.dlen = pocp_cmd->cmd_len;
	ptx_cmd->dchdr.wait = wait;
}

static u32 iris_pt_short_write(
		struct iris_ocp_cmd *pocp_cmd,
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd)
{
	u32 sum = 1;

	pheader->stHdr.longCmdFlag = 0x00;

	iris_pt_set_wrcmd_header(pheader, dsi_cmd);

	pr_debug("%s, line%d, header=0x%4x\n",
			__func__, __LINE__, pheader->hdr32);
	iris_cmd_reg_add(pocp_cmd, IRIS_MIPI_TX_HEADER_ADDR, pheader->hdr32);

	return sum;
}

static u32 iris_pt_short_read(
		struct iris_ocp_cmd *pocp_cmd,
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd)
{
	u32 sum = 1;

	pheader->stHdr.longCmdFlag = 0x00;
	iris_pt_set_rdcmd_header(pheader, dsi_cmd);

	pr_debug("%s, line%d, header=0x%4x\n",
			__func__, __LINE__, pheader->hdr32);
	iris_cmd_reg_add(pocp_cmd, IRIS_MIPI_TX_HEADER_ADDR, pheader->hdr32);

	return sum;
}


static u32 iris_pt_split_pkt_len(u16 dlen, int sum, int k)
{
	u16 split_len = 0;

	if (k == 0)
		split_len = dlen <  IRIS_TX_HV_PAYLOAD_LEN
					? dlen : IRIS_TX_HV_PAYLOAD_LEN;
	else if (k == sum - 1)
		split_len = dlen - IRIS_TX_HV_PAYLOAD_LEN
				- (k - 1) * IRIS_TX_PAYLOAD_LEN;
	else
		split_len = IRIS_TX_PAYLOAD_LEN;

	return split_len;
}

static void iris_pt_add_split_pkt_payload(
		struct iris_ocp_cmd *pocp_cmd, u8 *ptr, u16 split_len)
{
	u32 i = 0;
	union iris_mipi_tx_cmd_payload payload;

	memset(&payload, 0x00, sizeof(payload));
	for (i = 0; i < split_len; i += 4, ptr += 4) {
		if (i + 4 > split_len) {
			payload.pld32 = 0;
			memcpy(payload.p, ptr, split_len - i);
		} else
			payload.pld32 = *(u32 *)ptr;

		pr_debug("payload=0x%x\n", payload.pld32);
		iris_cmd_reg_add(pocp_cmd, IRIS_MIPI_TX_PAYLOAD_ADDR,
				payload.pld32);
	}
}

static u32 iris_pt_long_write(
		struct iris_ocp_cmd *pocp_cmd,
		union iris_mipi_tx_cmd_header *pheader,
		struct dsi_cmd_desc *dsi_cmd)
{
	u8 *ptr = NULL;
	u32 i = 0;
	u32 sum = 0;
	u16 dlen = 0;
	u32 split_len = 0;

	dlen = dsi_cmd->dchdr.dlen;

	pheader->stHdr.longCmdFlag = 0x1;
	iris_pt_set_wrcmd_header(pheader, dsi_cmd);

	pr_debug("%s, line%d, header=0x%x\n",
			__func__, __LINE__, pheader->hdr32);
	iris_cmd_reg_add(pocp_cmd, IRIS_MIPI_TX_HEADER_ADDR,
			pheader->hdr32);

	ptr = dsi_cmd->payload;
	sum = iris_pt_split_pkt_cnt(dlen);

	while (i < sum) {
		ptr += split_len;

		split_len = iris_pt_split_pkt_len(dlen, sum, i);

		iris_pt_add_split_pkt_payload(pocp_cmd + i, ptr, split_len);

		i++;
		if (i < sum)
			iris_pt_init_ocp_cmd(pocp_cmd + i);
	}
	return sum;
}

static u32 iris_pt_add_cmd(
		struct dsi_cmd_desc *ptx_cmd, struct iris_ocp_cmd *pocp_cmd,
		struct dsi_cmd_desc *dsi_cmd, struct dcs_cmd_req *cmdreq)
{
	u32 i = 0;
	u16 dtype = 0;
	u32 sum = 0;
	u8 wait = 0;
	union iris_mipi_tx_cmd_header header;

	iris_pt_init_tx_cmd_header(cmdreq, dsi_cmd, &header);

	dtype = dsi_cmd->dchdr.dtype;
	switch (dtype) {
	case DTYPE_GEN_READ:
	case DTYPE_DCS_READ:
	case DTYPE_GEN_READ1:
	case DTYPE_GEN_READ2:
		sum = iris_pt_short_read(pocp_cmd, &header, dsi_cmd);
		break;
	case DTYPE_DCS_WRITE:
	case DTYPE_DCS_WRITE1:
	case DTYPE_GEN_WRITE1:
	case DTYPE_GEN_WRITE2:
	case DTYPE_COMPRESSION_MODE:
	case DTYPE_MAX_PKTSIZE:
		sum = iris_pt_short_write(pocp_cmd, &header, dsi_cmd);
		break;
	case DTYPE_GEN_LWRITE:
	case DTYPE_DCS_LWRITE:
	case DTYPE_PPS:
		sum = iris_pt_long_write(pocp_cmd, &header, dsi_cmd);
		break;
	default:
		pr_err("could not identify the type = %0x\n",
				dsi_cmd->dchdr.dtype);
		break;
	}

	for (i = 0; i < sum; i++) {
		wait = (i == sum - 1) ? dsi_cmd->dchdr.wait : 0;
		iris_add_tx_cmds(ptx_cmd + i, pocp_cmd + i, wait);
	}
	return sum;
}

static void iris_pt_send_cmds(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *ptx_cmds, u32 cmds_cnt)
{
	struct dsi_panel_cmds panel_cmds;

	memset(&panel_cmds, 0x00, sizeof(panel_cmds));

	panel_cmds.cmds = ptx_cmds;
	panel_cmds.cmd_cnt = cmds_cnt;
	panel_cmds.link_state = DSI_HS_MODE;
	iris_dsi_cmds_send(ctrl, &panel_cmds);

	if (IRIS_CONT_SPLASH_LK == iris_get_cont_splash_type()) {
		pr_debug("link_state = %s\n",
			(panel_cmds.link_state) ? "high speed" : "low power");
		iris_print_cmds(panel_cmds.cmds, panel_cmds.cmd_cnt);
	}
}


void iris_panel_cmd_passthrough_wr(
		struct mdss_dsi_ctrl_pdata *ctrl, struct dcs_cmd_req *cmdreq)
{
	u32 i = 0;
	u32 j = 0;
	u32 cmds_cnt = 0;
	u32 offset = 0;
	struct iris_ocp_cmd *pocp_cmds = NULL;
	struct dsi_cmd_desc *ptx_cmds = NULL;
	struct dsi_cmd_desc *dsi_cmds = NULL;

	if (!ctrl || !cmdreq) {
		pr_err("cmdreq is null\n");
		return;
	}

	cmds_cnt = iris_pt_alloc_cmds_space(cmdreq, &ptx_cmds, &pocp_cmds);

	for (i = 0; i < cmdreq->cmds_cnt; i++) {
		/*initial val*/
		dsi_cmds = cmdreq->cmds + i;

		iris_pt_init_ocp_cmd(pocp_cmds + j);

		offset = iris_pt_add_cmd(
				ptx_cmds + j, pocp_cmds + j, dsi_cmds, cmdreq);
		j += offset;
	}

	if (j != cmds_cnt)
		pr_err("cmds cnt is not right real cmds_cnt = %d j = %d\n",
				cmds_cnt, j);
	else
		iris_pt_send_cmds(ctrl, ptx_cmds, cmds_cnt);

	kfree(pocp_cmds);
	kfree(ptx_cmds);
	pocp_cmds = NULL;
	ptx_cmds = NULL;
}

static void iris_pt_switch_cmd(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dcs_cmd_req *cmdreq,
		struct dsi_cmd_desc *dsi_cmd)
{
	if (!cmdreq || !ctrl || !dsi_cmd) {
		pr_err("%s there have null pointer\n", __func__);
		return;
	}

	cmdreq->cmds = dsi_cmd;
	cmdreq->cmds_cnt = 1;
	cmdreq->flags |= CMD_REQ_COMMIT;

	if ((dsi_cmd->dchdr.dlen < DMA_TPG_FIFO_LEN)
		&& (ctrl->shared_data->hw_rev >= MDSS_DSI_HW_REV_103))
		cmdreq->flags |= CMD_REQ_DMA_TPG;
}

static int iris_pt_write_max_ptksize(
		struct mdss_dsi_ctrl_pdata *ctrl,
		struct dcs_cmd_req *cmdreq)
{
	u32 rlen = 0;
	struct dcs_cmd_req local_cmdreq;
	static char max_pktsize[2] = {0x00, 0x00}; /* LSB tx first, 10 bytes */
	static struct dsi_cmd_desc pkt_size_cmd = {
		{DTYPE_MAX_PKTSIZE, 1, 0, 1, 0, sizeof(max_pktsize)},
		max_pktsize,
	};

	rlen = cmdreq->rlen;
	if (rlen > 128) {
		pr_err("dlen = %d  > 128\n", rlen);
		return -EINVAL;
	}

	max_pktsize[0] = (rlen & 0xFF);

	memset(&local_cmdreq, 0x00, sizeof(local_cmdreq));
	local_cmdreq.flags = ((cmdreq->flags & CMD_REQ_HS_MODE)
			? CMD_REQ_HS_MODE : CMD_REQ_LP_MODE);

	iris_pt_switch_cmd(ctrl, &local_cmdreq, &pkt_size_cmd);

	iris_panel_cmd_passthrough_wr(ctrl, &local_cmdreq);

	return 0;
}


static void iris_pt_write_rdcmd_to_panel(
		struct mdss_dsi_ctrl_pdata *ctrl, struct dcs_cmd_req *cmdreq)
{
	struct dcs_cmd_req local_cmdreq;
	struct dsi_cmd_desc *dsi_cmd = NULL;

	dsi_cmd = cmdreq->cmds;

	memset(&local_cmdreq, 0x00, sizeof(local_cmdreq));
	local_cmdreq.flags = ((cmdreq->flags & CMD_REQ_HS_MODE)
			? CMD_REQ_HS_MODE : CMD_REQ_LP_MODE);


	iris_pt_switch_cmd(ctrl, &local_cmdreq, dsi_cmd);

	/*passthrough write to panel*/
	iris_panel_cmd_passthrough_wr(ctrl, &local_cmdreq);
}

static int iris_pt_remove_resp_header(char *ptr, int *offset)
{
	int rc = 0;
	char cmd;

	if (!ptr)
		return -EINVAL;

	cmd = ptr[0];
	switch (cmd) {
	case DTYPE_ACK_ERR_RESP:
		pr_debug("%s: rx ACK_ERR_REPORT\n", __func__);
		rc = -EINVAL;
		break;
	case DTYPE_GEN_READ1_RESP:
	case DTYPE_DCS_READ1_RESP:
		*offset = 1;
		rc = 1;
		break;
	case DTYPE_GEN_READ2_RESP:
	case DTYPE_DCS_READ2_RESP:
		*offset = 1;
		rc = 2;
		break;
	case DTYPE_GEN_LREAD_RESP:
	case DTYPE_DCS_LREAD_RESP:
		*offset = 4;
		rc = ptr[1];
		break;
	default:
		rc = 0;
	}

	return rc;
}


static void iris_pt_read_value(struct dcs_cmd_req *cmdreq)
{
	u32 i = 0;
	u32 rlen = 0;
	u32 offset = 0;
	union iris_mipi_tx_cmd_payload val;
	u8 *rbuf = NULL;

	rbuf = cmdreq->rbuf;
	rlen = cmdreq->rlen;

	if (!rbuf || rlen <= 0) {
		pr_err("rbuf %p  rlen =%d\n", rbuf, rlen);
		return;
	}

	/*read iris for data*/
	val.pld32 = iris_ocp_read(IRIS_RD_PACKET_DATA,
			(cmdreq->flags & CMD_REQ_HS_MODE)
				? DSI_HS_MODE : DSI_LP_MODE);

	rlen = iris_pt_remove_resp_header(val.p, &offset);

	if (rlen <= 0) {
		pr_err("do not return value\n");
		return;
	}

	if (rlen <= 2) {
		for (i = 0; i < rlen; i++)
			rbuf[i] = val.p[offset + i];
	} else {
		int j = 0;
		int len = 0;
		int num = (rlen + 3) / 4;

		for (i = 0; i < num; i++) {
			len = (i == num - 1) ? rlen - 4 * i : 4;

			val.pld32 = iris_ocp_read(IRIS_RD_PACKET_DATA, DSI_HS_MODE);
			for (j = 0; j < len; j++)
				rbuf[i * 4 + j] = val.p[j];
		}
	}
}


void iris_panel_cmd_passthrough_rd(
		struct mdss_dsi_ctrl_pdata *ctrl, struct dcs_cmd_req *cmdreq)
{
	pr_debug("enter rd commands");

	if (!ctrl || !cmdreq || cmdreq->cmds_cnt != 1) {
		pr_err("cmdreq is error cmdreq = %p\n", cmdreq);
		return;
	}

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	/*step1  write max packket size*/
	iris_pt_write_max_ptksize(ctrl, cmdreq);

	/*step2 write read cmd to panel*/
	iris_pt_write_rdcmd_to_panel(ctrl, cmdreq);

	/*step3 read panel data*/
	iris_pt_read_value(cmdreq);

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
}


void iris_panel_cmd_passthrough(
		struct mdss_dsi_ctrl_pdata *ctrl, struct dcs_cmd_req *cmdreq)
{
	struct iris_cfg *pcfg = NULL;

	if (!cmdreq || !ctrl) {
		pr_err("cmdreq = %p  ctrl = %p\n", cmdreq, ctrl);
		return;
	}

	pcfg = iris_get_cfg();
	mutex_lock(&pcfg->mutex);
	if (cmdreq->flags  & CMD_REQ_RX)
		iris_panel_cmd_passthrough_rd(ctrl, cmdreq);
	else
		iris_panel_cmd_passthrough_wr(ctrl, cmdreq);
	mutex_unlock(&pcfg->mutex);
}
