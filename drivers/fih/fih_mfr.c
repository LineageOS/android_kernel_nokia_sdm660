#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/uaccess.h>

#define FIH_APR_PROC_MFR_NAME  "modemfailurereason"
#define MAX_SSR_REASON_LEN 81U
extern char fih_failure_reason[MAX_SSR_REASON_LEN];
static struct proc_dir_entry *entry_apr_mfr;


/******************************************************************************
*     Modem_Failure_Reason
*****************************************************************************/
static int fih_apr_proc_rd_mfr(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_failure_reason);
	//format the failure reasen buffer after reading it every time
	fih_failure_reason[0]='\0';

	return 0;
}


static int modemfailurereason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_apr_proc_rd_mfr, NULL);
}


static struct file_operations modemfailurereason_file_ops = {
	.owner   = THIS_MODULE,
	.open    = modemfailurereason_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};


static int __init fih_apr_init(void)
{
	entry_apr_mfr = proc_create(FIH_APR_PROC_MFR_NAME, 0444, NULL, &modemfailurereason_file_ops);

  if (entry_apr_mfr == NULL) {
		pr_err("%s: Fail to create %s\n", __func__, FIH_APR_PROC_MFR_NAME);
		goto exit_mfr;
	}

	return (0);

exit_mfr:
	remove_proc_entry(FIH_APR_PROC_MFR_NAME, NULL);

	return (1);
}


static void __exit fih_apr_exit(void)
{
	remove_proc_entry(FIH_APR_PROC_MFR_NAME, NULL);
}


module_init(fih_apr_init);
module_exit(fih_apr_exit);
