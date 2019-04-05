#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>

#include "fih_ramtable.h"
#include "fih_swid.h"

/* conserve current hwid table in kernel use */
struct st_swid_table fih_swid_table;

struct st_swid_table def_swid_table = {
	.hwid    = "0",
	.project = FIH_SWID_PRJ_S2Plus,
	.hw_rev  = FIH_SWID_REV_EVB,
	.rf      = 0,
	//.module  = FIH_MODULE_DEFAULT,
        .bsp     = 0,
	.dtm     = FIH_SWID_REV_EVB,
	.dtn     = 0,
	.sim     = FIH_SWID_SIM_DUAL,
	.project_name	= "SS2",
	.factory_name = "SS2",
	.phase_sw = "EVB",
	.phase_hw = "EVB",
	.sku_name = "CN",
	.rf_band = "NA",
};

void fih_swid_setup(void)
{
	struct st_swid_table *mem_swid_table = (struct st_swid_table *)ioremap(FIH_HWCFG_MEM_ADDR, sizeof(struct st_swid_table));
	if (mem_swid_table == NULL) {
		pr_err("%s: setup swid table by default coded because load fail\n", __func__);
		memcpy(&fih_swid_table, &def_swid_table, sizeof(struct st_swid_table));
		return;
	}

	pr_info("%s: read swid table from memory\n", __func__);
	memcpy(&fih_swid_table, mem_swid_table, sizeof(struct st_swid_table));

	fih_swid_print(&fih_swid_table);
}

void fih_swid_print(struct st_swid_table *tb)
{
	pr_info("fih_swid_print: begin\n");

	if (NULL == tb) {
		pr_err("fih_swid_print: tb = NULL\n");
		return;
	}

	/* mpp */
	pr_info("hwid = 0x%lu\n", (unsigned long) tb->hwid);
	pr_info("project = %d\n", tb->project);
	pr_info("hw_rev = %d\n", tb->hw_rev);
	pr_info("rf = %d\n", tb->rf);
	pr_info("bsp = %d\n", tb->bsp);/*modify from module to bsp*/
	/* device tree */
	pr_info("DT_MAJOR = %d\n", tb->dtm);
	pr_info("DT_MINOR = %d\n", tb->dtn);
	pr_info("sim = %d\n", tb->sim);
	/* info */
	pr_info("project_name = %s\n", tb->project_name);
	pr_info("factory_name = %s\n", tb->factory_name);
	pr_info("phase_sw = %s\n", tb->phase_sw);
	pr_info("phase_hw = %s\n", tb->phase_hw);
	pr_info("sku_name = %s\n", tb->sku_name);
	pr_info("rf_band = %s\n", tb->rf_band);
	pr_info("fih_hwid_print: end\n");
}

void fih_swid_read(struct st_swid_table *tb)
{
	memcpy(tb, &fih_swid_table, sizeof(struct st_swid_table));
}

void* fih_swid_fetch(int idx)
{
	void* ret = NULL;
	struct st_swid_table *tb = &fih_swid_table;

	switch (idx) {
		/* hwcfg int value  */
		case FIH_SWID_PROJECT: ret = (void *)&tb->project; break;
		case FIH_SWID_HWREV: ret = (void *)&tb->hw_rev; break;
		case FIH_SWID_RF: ret = (void *)&tb->rf; break;
		//case FIH_SWID_MODULE: ret = (void *)&tb->module; break;/*no use*/
		case FIH_SWID_DTM: ret = (void *)&tb->dtm; break;
		case FIH_SWID_DTN: ret = (void *)&tb->dtn; break;
		case FIH_SWID_HWID: ret = (void *)&tb->hwid; break;
		case FIH_SWID_SIM: ret = (void *)&tb->sim; break;
		case FIH_SWID_PROJECT_NAME: ret = (void *)tb->project_name; break;
		case FIH_SWID_FACTORY_NAME: ret = (void *)tb->factory_name; break;
		case FIH_SWID_PHASE_SW: ret = (void *)tb->phase_sw; break;
		case FIH_SWID_PHASE_HW: ret = (void *)tb->phase_hw; break;
		case FIH_SWID_SKU_NAME: ret = (void *)tb->sku_name; break;
		case FIH_SWID_RF_BAND: ret = (void *)tb->rf_band; break;
		case FIH_SWID_PCBA_DESCRIPTION: ret = (void *)tb->pcba_description; break;
		default:
			pr_err("fih_swid_fetch: unsupport idx = %d\n", idx);
			ret = NULL;
			break;
	}

	return ret;
}
