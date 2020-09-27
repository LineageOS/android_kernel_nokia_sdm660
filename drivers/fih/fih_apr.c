#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/uaccess.h>
	
#include "fih_pon.h"
#include "fih_poff.h"
#include "fih_rere.h"

static char pon[16];
static char poff[16];
static char rere[16];
	
ssize_t fih_apr_proc_write_pon(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[16];
	unsigned int size;
	
	size = (count > sizeof(tmp))? sizeof(tmp):count;
	
	if (copy_from_user(tmp, buffer, size)) {
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	memset(pon, 0, sizeof(pon));
	snprintf(pon, sizeof(pon), "%s", tmp);
	pon[sizeof(pon)-1] = 0x0;
	
	return size;
}
	
static int fih_apr_proc_read_pon(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", pon);
	return 0;
}
	
static int fih_apr_proc_open_pon(struct inode *inode, struct file *file)
{
	return single_open(file, fih_apr_proc_read_pon, NULL);
}
	
static const struct file_operations fih_apr_fops_pon = {
	.open    = fih_apr_proc_open_pon,
	.read    = seq_read,
	.write   = fih_apr_proc_write_pon,
	.llseek  = seq_lseek,
	.release = single_release,
};
	
static int fih_apr_proc_read_poff(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", poff);
	return 0;
}
	
static int fih_apr_proc_open_poff(struct inode *inode, struct file *file)
{
	return single_open(file, fih_apr_proc_read_poff, NULL);
}
	
static const struct file_operations fih_apr_fops_poff = {
	.open    = fih_apr_proc_open_poff,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
	
static int fih_apr_proc_read_rere(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", rere);
	return 0;
}
	
static int fih_apr_proc_open_rere(struct inode *inode, struct file *file)
{
	return single_open(file, fih_apr_proc_read_rere, NULL);
}
	
static const struct file_operations fih_apr_fops_rere = {
	.open    = fih_apr_proc_open_rere,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
	
static int fih_apr_property(struct platform_device *pdev)
{
	int rc = 0;
	static const char *p_chr;
	
	p_chr = of_get_property(pdev->dev.of_node, "fih-apr,poweroncause", NULL);
	if (!p_chr) {
		pr_info("%s:%d, poweroncause not specified\n", __func__, __LINE__);
	} else {
		strlcpy(pon, p_chr, sizeof(pon));
		pr_info("%s: poweroncause = %s\n", __func__, pon);
	}
	
	p_chr = of_get_property(pdev->dev.of_node, "fih-apr,poweroffcause", NULL);
	if (!p_chr) {
		pr_info("%s:%d, poweroffcause not specified\n", __func__, __LINE__);
	} else {
		strlcpy(poff, p_chr, sizeof(poff));
		pr_info("%s: poweroffcause = %s\n", __func__, poff);
	}
	
	p_chr = of_get_property(pdev->dev.of_node, "fih-apr,rebootreason", NULL);
	if (!p_chr) {
		pr_info("%s:%d, rebootreason not specified\n", __func__, __LINE__);
	} else {
		strlcpy(rere, p_chr, sizeof(rere));
		pr_info("%s: rebootreason = %s\n", __func__, rere);
	}
	
	return rc;
}
	
static void fih_apr_bbslog(void)
{
	unsigned long pn = simple_strtoul(pon,  NULL, 0);
	unsigned long pf = simple_strtoul(poff, NULL, 0);
	unsigned long re = simple_strtoul(rere, NULL, 0);

	pr_info("%s: pon_=0x%08lx\n", __func__, pn);
	pr_info("%s: poff=0x%08lx\n", __func__, pf);
	pr_info("%s: rere=0x%08lx\n", __func__, re);

	if (pf & FIH_POFF_UVLO)
		printk("BBox::UEC;40::16;UNDER_VOLAGE\n");
	if (pf & FIH_POFF_OVLO)
		printk("BBox::UEC;40::17;OVER_VOLAGE\n");
	if ((pf & FIH_POFF_OTST3)||(re == FIH_RERE_OVER_TAMPERATURE))
		printk("BBox::UEC;40::18;OVER_TEMPERATURE\n");
	if (!(pn & FIH_PON_FIH_APR_MASK))
		printk("BBox::UEC;40::19;SHDN_USER\n");
	if (pn & FIH_PON_APR_SYSTEM_CRASH)
		printk("BBox::UEC;40::20;SYSTEM_CRASH\n");
	if ((pn & FIH_PON_PMIC_C_KPDPWR)&&(pf & FIH_POFF_S3_RESET))
		printk("BBox::UEC;40::21;HARD_RESET\n");
	if (pn & FIH_PON_PMIC_C_SMPL)
		printk("BBox::UEC;40::22;SMPL\n");
	if (pf & FIH_POFF_UVLO)
		printk("BBox::UEC;40::23;UVLO\n");
	if (pn & FIH_PON_PMIC_W_GP2)
		printk("BBox::UEC;40::24;GP2\n");
	if (pn & FIH_PON_APR_KERNEL_PANIC)
		printk("BBox::UEC;40::25;KERNEL_PANIC\n");
	if (pn & FIH_PON_APR_UNKNOWN_RESET)
		printk("BBox::UEC;40::26;UNKNOWN_RESET\n");
	if (pn & FIH_PON_APR_MODEM_FATAL)
		printk("BBox::UEC;40::27;MODEM_FATAL\n");
}

static int fih_apr_probe(struct platform_device *pdev)
{
	int rc = 0;
	
	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: Unable to load device node\n", __func__);
		return -ENOTSUPP;
	}
	
	rc = fih_apr_property(pdev);
	if (rc) {
		pr_err("%s Unable to set property\n", __func__);
		return rc;
	}
	
	fih_apr_bbslog();

	proc_create("poweroncause", 0, NULL, &fih_apr_fops_pon);
	proc_create("poweroffcause", 0, NULL, &fih_apr_fops_poff);
	proc_create("rebootreason", 0, NULL, &fih_apr_fops_rere);
	
	return rc;
}
	
static int fih_apr_remove(struct platform_device *pdev)
{
	remove_proc_entry ("poweroncause", NULL);
	remove_proc_entry ("poweroffcause", NULL);
	remove_proc_entry ("rebootreason", NULL);
	
	return 0;
}
	
static const struct of_device_id fih_apr_dt_match[] = {
	{.compatible = "fih_apr"},
	{}
};
MODULE_DEVICE_TABLE(of, fih_apr_dt_match);
	
static struct platform_driver fih_apr_driver = {
	.probe = fih_apr_probe,
	.remove = fih_apr_remove,
	.shutdown = NULL,
	.driver = {
		.name = "fih_apr",
		.of_match_table = fih_apr_dt_match,
	},
};
	
static int __init fih_apr_init(void)
{
	int ret;
	
	ret = platform_driver_register(&fih_apr_driver);
	if (ret) {
		pr_err("%s: failed!\n", __func__);
		return ret;
	}
	
	return ret;
}
module_init(fih_apr_init);
	
static void __exit fih_apr_exit(void)
{
	platform_driver_unregister(&fih_apr_driver);
}
module_exit(fih_apr_exit);
	
	
