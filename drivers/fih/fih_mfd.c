#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/uaccess.h>

#define FIH_MFD_DATA_LEN   64

#define FIH_MFD_NAME_PID       "productid"
#define FIH_MFD_NAME_BT_MAC    "bt_mac"
#define FIH_MFD_NAME_WIFI_MAC  "wifi_mac"


static char productid[FIH_MFD_DATA_LEN];
static char bt_mac[FIH_MFD_DATA_LEN];
static char wifi_mac[FIH_MFD_DATA_LEN];

static int fih_mfd_proc_read_productid(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", productid);
	return 0;
}

static int fih_mfd_proc_open_productid(struct inode *inode, struct file *file)
{
	return single_open(file, fih_mfd_proc_read_productid, NULL);
}

static const struct file_operations fih_mfd_fops_productid = {
	.open    = fih_mfd_proc_open_productid,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_mfd_proc_read_bt_mac(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", bt_mac);
	return 0;
}

static int fih_mfd_proc_open_bt_mac(struct inode *inode, struct file *file)
{
	return single_open(file, fih_mfd_proc_read_bt_mac, NULL);
}

static const struct file_operations fih_mfd_fops_bt_mac = {
	.open    = fih_mfd_proc_open_bt_mac,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_mfd_proc_read_wifi_mac(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", wifi_mac);
	return 0;
}

static int fih_mfd_proc_open_wifi_mac(struct inode *inode, struct file *file)
{
	return single_open(file, fih_mfd_proc_read_wifi_mac, NULL);
}

static const struct file_operations fih_mfd_fops_wifi_mac = {
	.open    = fih_mfd_proc_open_wifi_mac,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int fih_mfd_property(struct platform_device *pdev)
{
	int rc = 0;
	static const char *p_chr;

	p_chr = of_get_property(pdev->dev.of_node, "fih-mfd,productid", NULL);
	if (!p_chr) {
		pr_info("%s:%d, productid not specified\n", __func__, __LINE__);
	} else {
		strlcpy(productid, p_chr, sizeof(productid));
		pr_info("%s: productid = %s\n", __func__, productid);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-mfd,bt_mac", NULL);
	if (!p_chr) {
		pr_info("%s:%d, bt_mac not specified\n", __func__, __LINE__);
	} else {
		strlcpy(bt_mac, p_chr, sizeof(bt_mac));
		pr_info("%s: bt_mac = %s\n", __func__, bt_mac);
	}

	p_chr = of_get_property(pdev->dev.of_node, "fih-mfd,wifi_mac", NULL);
	if (!p_chr) {
		pr_info("%s:%d, wifi_mac not specified\n", __func__, __LINE__);
	} else {
		strlcpy(wifi_mac, p_chr, sizeof(wifi_mac));
		pr_info("%s: wifi_mac = %s\n", __func__, wifi_mac);
	}

	return rc;
}

static int fih_mfd_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: Unable to load device node\n", __func__);
		return -ENOTSUPP;
	}

	rc = fih_mfd_property(pdev);
	if (rc) {
		pr_err("%s Unable to set property\n", __func__);
		return rc;
	}

	proc_create(FIH_MFD_NAME_PID, 0, NULL, &fih_mfd_fops_productid);
	proc_create(FIH_MFD_NAME_BT_MAC, 0, NULL, &fih_mfd_fops_bt_mac);
	proc_create(FIH_MFD_NAME_WIFI_MAC, 0, NULL, &fih_mfd_fops_wifi_mac);

	return rc;
}

static int fih_mfd_remove(struct platform_device *pdev)
{
	remove_proc_entry(FIH_MFD_NAME_PID, NULL);
	remove_proc_entry(FIH_MFD_NAME_BT_MAC, NULL);
	remove_proc_entry(FIH_MFD_NAME_WIFI_MAC, NULL);

	return 0;
}

static const struct of_device_id fih_mfd_dt_match[] = {
	{.compatible = "fih_mfd"},
	{}
};
MODULE_DEVICE_TABLE(of, fih_mfd_dt_match);

static struct platform_driver fih_mfd_driver = {
	.probe    = fih_mfd_probe,
	.remove   = fih_mfd_remove,
	.driver   = {
		.name           = "fih_mfd",
		.of_match_table = fih_mfd_dt_match,
	},
};

static int __init fih_mfd_init(void)
{
	int ret;

	ret = platform_driver_register(&fih_mfd_driver);
	if (ret) {
		pr_err("%s: failed!\n", __func__);
		return ret;
	}

	return ret;
}
module_init(fih_mfd_init);

static void __exit fih_mfd_exit(void)
{
	platform_driver_unregister(&fih_mfd_driver);
}
module_exit(fih_mfd_exit);
