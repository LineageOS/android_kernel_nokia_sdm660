#include <linux/io.h>
#include <soc/qcom/restart.h>

unsigned int fih_rere_rd_imem(void)
{
	return restart_reason_rd();
}

void fih_rere_wt_imem(unsigned int rere)
{
	restart_reason_wt(rere);
}
