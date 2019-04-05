#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static char devmodel[8];
static char baseband[8];
static char bandinfo[256];
static char hwmodel[8];
static char simslot[3];
static char fqcxmlpath[64];

static int fih_info_proc_show_devmodel(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", devmodel);
	return 0;
}

static int fih_info_proc_open_devmodel(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_devmodel, NULL);
}

static const struct file_operations fih_info_fops_devmodel = {
	.open    = fih_info_proc_open_devmodel,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_proc_show_baseband(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", baseband);
	return 0;
}

static int fih_info_proc_open_baseband(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_baseband, NULL);
}

static const struct file_operations fih_info_fops_baseband = {
	.open    = fih_info_proc_open_baseband,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_proc_show_bandinfo(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bandinfo);
	return 0;
}

static int fih_info_proc_open_bandinfo(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_bandinfo, NULL);
}

static const struct file_operations fih_info_fops_bandinfo = {
	.open    = fih_info_proc_open_bandinfo,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_proc_show_hwmodel(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", hwmodel);
	return 0;
}

static int fih_info_proc_open_hwmodel(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_hwmodel, NULL);
}

static const struct file_operations fih_info_fops_hwmodel = {
	.open    = fih_info_proc_open_hwmodel,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_proc_show_simslot(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", simslot);
	return 0;
}

static int fih_info_proc_open_simslot(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_simslot, NULL);
}

static const struct file_operations fih_info_fops_simslot = {
	.open    = fih_info_proc_open_simslot,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_proc_show_fqcxmlpath(struct seq_file *m, void *v)
{
	seq_printf(m, "/system/etc/%s\n", fqcxmlpath);
	return 0;
}

static int fih_info_proc_open_fqcxmlpath(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_show_fqcxmlpath, NULL);
}

static const struct file_operations fih_info_fops_fqcxmlpath = {
	.open    = fih_info_proc_open_fqcxmlpath,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_info_property(struct platform_device *pdev)
{
	int rc = 0;
	static const char *p_chr;

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,devmodel", NULL);
	if (!p_chr) {
		pr_info("%s:%d, devmodel not specified\n", __func__, __LINE__);
	} else {
		strlcpy(devmodel, p_chr, sizeof(devmodel));
		pr_info("%s: devmodel = %s\n", __func__, devmodel);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,baseband", NULL);
	if (!p_chr) {
		pr_info("%s:%d, baseband not specified\n", __func__, __LINE__);
	} else {
		strlcpy(baseband, p_chr, sizeof(baseband));
		pr_info("%s: baseband = %s\n", __func__, baseband);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,bandinfo", NULL);
	if (!p_chr) {
		pr_info("%s:%d, bandinfo not specified\n", __func__, __LINE__);
	} else {
		strlcpy(bandinfo, p_chr, sizeof(bandinfo));
		pr_info("%s: bandinfo = %s\n", __func__, bandinfo);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,hwmodel", NULL);
	if (!p_chr) {
		pr_info("%s:%d, hwmodel not specified\n", __func__, __LINE__);
	} else {
		strlcpy(hwmodel, p_chr, sizeof(hwmodel));
		pr_info("%s: hwmodel = %s\n", __func__, hwmodel);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,simslot", NULL);
	if (!p_chr) {
		pr_info("%s:%d, simslot not specified\n", __func__, __LINE__);
	} else {
		strlcpy(simslot, p_chr, sizeof(simslot));
		pr_info("%s: simslot = %s\n", __func__, simslot);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-info,fqcxmlpath", NULL);
	if (!p_chr) {
		pr_info("%s:%d, fqcxmlpath not specified\n", __func__, __LINE__);
	} else {
		strlcpy(fqcxmlpath, p_chr, sizeof(fqcxmlpath));
		pr_info("%s: fqcxmlpath = %s\n", __func__, fqcxmlpath);
	}

	return rc;
}

static int fih_info_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: Unable to load device node\n", __func__);
		return -ENOTSUPP;
	}

	rc = fih_info_property(pdev);
	if (rc) {
		pr_err("%s Unable to set property\n", __func__);
		return rc;
	}

	proc_create("devmodel", 0, NULL, &fih_info_fops_devmodel);
	proc_create("baseband", 0, NULL, &fih_info_fops_baseband);
	proc_create("bandinfo", 0, NULL, &fih_info_fops_bandinfo);
	proc_create("hwmodel",  0, NULL, &fih_info_fops_hwmodel);
	proc_create("SIMSlot",  0, NULL, &fih_info_fops_simslot);
	proc_create("fqc_xml",  0, NULL, &fih_info_fops_fqcxmlpath);

	return rc;
}

static int fih_info_remove(struct platform_device *pdev)
{
	remove_proc_entry ("bandinfo", NULL);
	remove_proc_entry ("baseband", NULL);
	remove_proc_entry ("devmodel", NULL);
	remove_proc_entry ("hwmodel",  NULL);
	remove_proc_entry ("SIMSlot",  NULL);
	remove_proc_entry ("fqc_xml",  NULL);

	return 0;
}

static const struct of_device_id fih_info_dt_match[] = {
	{.compatible = "fih_info"},
	{}
};
MODULE_DEVICE_TABLE(of, fih_info_dt_match);

static struct platform_driver fih_info_driver = {
	.probe = fih_info_probe,
	.remove = fih_info_remove,
	.shutdown = NULL,
	.driver = {
		.name = "fih_info",
		.of_match_table = fih_info_dt_match,
	},
};

static int __init fih_info_init(void)
{
	int ret;

	ret = platform_driver_register(&fih_info_driver);
	if (ret) {
		pr_err("%s: failed!\n", __func__);
		return ret;
	}

	return ret;
}
module_init(fih_info_init);

static void __exit fih_info_exit(void)
{
	platform_driver_unregister(&fih_info_driver);
}
module_exit(fih_info_exit);

