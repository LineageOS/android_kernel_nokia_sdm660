#define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
//#include <fih/hwid.h>
#include <linux/uaccess.h>
#include "fih_lcm.h"

extern int CE_enable;
extern int CT_enable;
extern int CABC_enable;
extern int AIE_enable;

#if 0
extern int vddio_enable;	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int avdd_enable;		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int avee_enable;		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
#endif

extern int fih_get_ce (void);
extern int fih_set_ce (int ce);
extern int fih_get_ct (void);
extern int fih_set_ct (int ct);
extern int fih_get_cabc (void);
extern int fih_set_cabc (int cabc);
extern int fih_get_aie (void);
extern int fih_set_aie (int aie);

#if 0
extern int fih_get_vddio (void);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_vddio (int vddio);	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_get_avdd (void);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_avdd (int avdd);	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_get_avee (void);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_avee (int avee);	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_get_reset (void);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_reset (int reset);	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_get_init (void);			//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_init (int reset);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_get_ldos (void);		//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
extern int fih_set_ldos (int reset);	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+_20150519
#endif
extern void fih_get_read_reg (char *reg_val);	//SW4-HL-Display-DynamicReadWriteRegister-00+_20160729
extern void fih_set_read_reg (unsigned int reg, unsigned int reg_len);	//SW4-HL-Display-DynamicReadWriteRegister-00+_20160729
extern void fih_set_write_reg (unsigned int len, char *data);	//SW4-HL-Display-DynamicReadWriteRegister-00+_20160729

//SW4-HL-Display-HDR-Ping-00+{_20180323
//HDR Ping
extern int HDR_enable;
extern int fih_hdr_ping (void);
//SW4-HL-Display-HDR-Ping-00+}_20180323

//SW4-HL-Display-HDR-SetFsCurr-00+{_20180515
extern int fih_get_fs_curr (void);
extern int fih_set_fs_curr (int fs_curr);
//SW4-HL-Display-HDR-SetFsCurr-00+}_20180515

//SW4-HL-Display-GlanceMode-00+{_20170524
#ifdef CONFIG_AOD_FEATURE
#include <linux/slab.h>
extern int fih_get_aod(void);
extern unsigned int fih_get_panel_id(void);
extern int fih_set_aod(int enable);
extern int fih_set_low_power_mode(int enable);
extern int fih_get_low_power_mode(void);
#endif
//SW4-HL-Display-GlanceMode-00+}_20170524

char fih_awer_cnt[32] = "unknown";
void fih_awer_cnt_set(char *info)
{
	strcpy(fih_awer_cnt, info);
}

static int fih_awer_cnt_read_proc(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_awer_cnt);
	return 0;
}

static int fih_awer_cnt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_awer_cnt_read_proc, NULL);
}

static struct file_operations awer_cnt_operations = {
	.open		= fih_awer_cnt_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

char fih_awer_status[32] = "unknown";
void fih_awer_status_set(char *info)
{
	strcpy(fih_awer_status, info);
}

static int fih_awer_status_read_proc(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_awer_status);
	return 0;
}

static int fih_awer_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_awer_status_read_proc, NULL);
}

static struct file_operations awer_status_operations = {
	.open		= fih_awer_status_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int fih_lcm_read_color_mode(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_ce());

	return 0;
}

static ssize_t fih_lcm_write_color_mode(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*{_20150428
	if(strstr(saved_command_line, "androidboot.fihmode=0") != NULL ||
	   strstr(saved_command_line, "androidboot.fihmode=3") != NULL)
	{
		res = fih_set_ce(simple_strtoull(tmp, NULL, 0));
		if (res < 0)
		{
			return res;
		}
	}
	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*}_20150428

	return size;
}

static int fih_lcm_color_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_color_mode, NULL);
};

static struct file_operations cm_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_color_mode_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_color_mode,
	.llseek  = seq_lseek,
	.release = single_release
};


static int fih_lcm_read_color_temperature(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_ct());

	return 0;
}

static ssize_t fih_lcm_write_color_temperature(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[5];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*{_20150428
	if(strstr(saved_command_line, "androidboot.fihmode=0") != NULL ||
	   strstr(saved_command_line, "androidboot.fihmode=3") != NULL)
	{
		res = fih_set_ct(simple_strtoull(tmp, NULL, 0));
		if (res < 0)
		{
			return res;
		}
	}
	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*}_20150428

	return size;
}

static int fih_lcm_color_temperature_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_color_temperature, NULL);
};

static struct file_operations ct_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_color_temperature_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_color_temperature,
	.llseek  = seq_lseek,
	.release = single_release
};


static int fih_lcm_read_cabc_settings(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_cabc());

	return 0;
}

static ssize_t fih_lcm_write_cabc_settings(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*{_20150428
	if(strstr(saved_command_line, "androidboot.fihmode=0") != NULL ||
	   strstr(saved_command_line, "androidboot.fihmode=3") != NULL)
	{
		res = fih_set_cabc(simple_strtoull(tmp, NULL, 0));
		if (res < 0)
		{
			return res;
		}
	}
	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*}_20150428

	return size;
}

static int fih_lcm_cabc_settings_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_cabc_settings, NULL);
};

static struct file_operations cabc_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_cabc_settings_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_cabc_settings,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_aie_settings(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_aie());

	return 0;
}

static ssize_t fih_lcm_write_aie_settings(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	if(strstr(saved_command_line, "androidboot.fihmode=0") != NULL ||
	   strstr(saved_command_line, "androidboot.fihmode=3") != NULL)
	{
		res = fih_set_aie(simple_strtoull(tmp, NULL, 0));
		if (res < 0)
		{
			return res;
		}
	}

	return size;
}

static int fih_lcm_aie_settings_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_aie_settings, NULL);
};

static struct file_operations aie_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_aie_settings_open,
	.read    = seq_read,
	.write   = fih_lcm_write_aie_settings,
	.llseek  = seq_lseek,
	.release = single_release
};

#if 0
//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+{_20150519
static int fih_lcm_read_vddio(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_vddio());

	return 0;
}

static ssize_t fih_lcm_write_vddio(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_vddio(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_vddio_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_vddio, NULL);
};

static struct file_operations vddio_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_vddio_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_vddio,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_avdd(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_avdd());

	return 0;
}

static ssize_t fih_lcm_write_avdd(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_avdd(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_avdd_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_avdd, NULL);
};

static struct file_operations avdd_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_avdd_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_avdd,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_avee(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_avee());

	return 0;
}

static ssize_t fih_lcm_write_avee(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_avee(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_avee_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_avee, NULL);
};

static struct file_operations avee_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_avee_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_avee,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_reset(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_reset());

	return 0;
}

static ssize_t fih_lcm_write_reset(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_reset(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_reset_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_reset, NULL);
};

static struct file_operations reset_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_reset_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_reset,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_init(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_init());

	return 0;
}

static ssize_t fih_lcm_write_init(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_init(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_init_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_init, NULL);
};

static struct file_operations init_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_init_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_init,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_read_ldos(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_ldos());

	return 0;
}

static ssize_t fih_lcm_write_ldos(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	res = fih_set_ldos(simple_strtoull(tmp, NULL, 0));
	if (res < 0)
	{
		return res;
	}

	return size;
}

static int fih_lcm_ldos_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_ldos, NULL);
};

static struct file_operations ldos_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_ldos_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_ldos,
	.llseek  = seq_lseek,
	.release = single_release
};
//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+}_20150519
#endif

//SW4-HL-Display-DynamicReadWriteRegister-00+{_20160729
static int fih_lcm_read_reg(struct seq_file *m, void *v)
{
	char reg_value[20]={0};

	fih_get_read_reg(reg_value);

	seq_printf(m, "%s", reg_value);

	return 0;
}

static ssize_t fih_lcm_write_reg(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[4];
	unsigned int size;
	unsigned int reg;
	unsigned int len;

	if (count < 4)
	{
		//pr_err("%s: Length of input parameter is less than 4\n", __func__);
		return -EFAULT;
	}

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}
	pr_err("[HL]%s: tmp = (%s)\n", __func__, tmp);

	sscanf(tmp, "%x,%d", &reg, &len);

	pr_err("[HL]%s: reg = 0x%x, len = %d\n", __func__, reg, len);

	fih_set_read_reg(reg, len);

	return size;
}

static int fih_lcm_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_reg, NULL);
};

static struct file_operations reg_read_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_reg_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_reg,
	.llseek  = seq_lseek,
	.release = single_release
};

static int fih_lcm_write_reg_read(struct seq_file *m, void *v)
{
	pr_err("[HL]%s <-- START\n", __func__);
	return 0;
}

static ssize_t fih_lcm_write_reg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[512];
	unsigned int size;
	unsigned int len;
	unsigned char data[512];

	pr_err("\n\n*** [HL] %s <-- START ***\n\n", __func__);

	if (count < 8)
	{
		pr_err("%s: Length of input parameter is less than 8\n", __func__);
		return -EFAULT;
	}

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}
	pr_err("[HL]%s: tmp = (%s)\n", __func__, tmp);

	sscanf(tmp, "%d,%s,", &len, data);

	pr_err("[HL]%s: len = %d, data = (%s)\n", __func__, len, data);

	fih_set_write_reg(len, data);

	pr_err("\n\n*** [HL] %s <-- END ***\n\n", __func__);

	return count;
}

static int fih_lcm_write_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_write_reg_read, NULL);
};

static struct file_operations reg_write_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_write_reg_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_reg_write,
	.llseek  = seq_lseek,
	.release = single_release
};
//SW4-HL-Display-DynamicReadWriteRegister-00+}_20160729

//SW4-HL-Display-GlanceMode-00+{_20170524
#ifdef CONFIG_AOD_FEATURE
static int fih_lcm_show_aod_lp_settings(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", fih_get_low_power_mode());

    return 0;
}

static int fih_lcm_open_aod_lp_settings(struct inode *inode, struct  file *file)
{
    return single_open(file, fih_lcm_show_aod_lp_settings, NULL);
}


static ssize_t fih_lcm_write_aod_lp_settings(struct file *file, const char __user *buffer,
                    size_t count, loff_t *offp)
{
    char *buf;
    unsigned int res;

    if (count < 1)
        return -EINVAL;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;

	pr_err("fih_lcm_write_aod_lp_settings\n");

    res = fih_set_low_power_mode(simple_strtoull(buf, NULL, 0));

    if (res < 0)
    {
        kfree(buf);
        return res;
    }

    kfree(buf);

    /* claim that we wrote everything */
    return count;
}

static struct file_operations aod_lp_file_ops = {
    .owner   = THIS_MODULE,
    .write   = fih_lcm_write_aod_lp_settings,
    .read    = seq_read,
    .open    = fih_lcm_open_aod_lp_settings,
    .release = single_release
};

static int fih_lcm_show_aod_settings(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", fih_get_aod());

    return 0;
}

static int fih_lcm_open_aod_settings(struct inode *inode, struct  file *file)
{
    return single_open(file, fih_lcm_show_aod_settings, NULL);
}


static ssize_t fih_lcm_write_aod_settings(struct file *file, const char __user *buffer,
                    size_t count, loff_t *offp)
{
    char *buf;
    unsigned int res;

    if (count < 1)
        return -EINVAL;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;

    res = fih_set_aod(simple_strtoull(buf, NULL, 0));

    if (res < 0)
    {
        kfree(buf);
        return res;
    }

    kfree(buf);

    /* claim that we wrote everything */
    return count;
}

static struct file_operations aod_file_ops = {
    .owner   = THIS_MODULE,
    .write   = fih_lcm_write_aod_settings,
    .read    = seq_read,
    .open    = fih_lcm_open_aod_settings,
    .release = single_release
};
#endif
//SW4-HL-Display-GlanceMode-00+}_20170524

//SW4-HL-Display-HDR-Ping-00+{_20180323
//**********************************************
//* HDR Ping
//**********************************************
static int fih_hdr_chip_info_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_hdr_ping());

	return 0;
}

static int fih_hdr_chip_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_hdr_chip_info_read, NULL);
};

static struct file_operations hdr_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_hdr_chip_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
//SW4-HL-Display-HDR-Ping-00+}_20180323

//SW4-HL-Display-HDR-SetFsCurr-00+{_20180515
static int fih_lcm_read_fs_curr_settings(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", fih_get_fs_curr());

	return 0;
}

static ssize_t fih_lcm_write_fs_curr_settings(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	unsigned char tmp[2];
	unsigned int size;
	unsigned int res;

	size = (count > sizeof(tmp))? sizeof(tmp):count;

	if (copy_from_user(tmp, buffer, size))
	{
		pr_err("%s: copy_from_user fail\n", __func__);
		return -EFAULT;
	}

	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*{_20150428
	if(strstr(saved_command_line, "androidboot.fihmode=0") != NULL ||
	   strstr(saved_command_line, "androidboot.fihmode=3") != NULL)
	{
		res = fih_set_fs_curr(simple_strtoull(tmp, NULL, 0));
		if (res < 0)
		{
			return res;
		}
	}
	//SW4-HL-Display-CE&CTWillNotBeExecutedUntilPanelInitIsDone-01*}_20150428

	return size;
}

static int fih_lcm_fs_curr_settings_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_lcm_read_fs_curr_settings, NULL);
};

static struct file_operations fs_curr_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_lcm_fs_curr_settings_open,
	.read    = seq_read,
	.write	 = fih_lcm_write_fs_curr_settings,
	.llseek  = seq_lseek,
	.release = single_release
};
//SW4-HL-Display-HDR-SetFsCurr-00+}_20180515

struct {
	char *name;
	struct file_operations *ops;
} *p, LCM0_cm[] = {
	{"AllHWList/LCM0/color_mode", &cm_file_ops},
	{NULL}, },
	LCM0_ct[] = {
	{"AllHWList/LCM0/color_temperature", &ct_file_ops},
	{NULL}, },
	LCM0_cabc[] = {
	{"AllHWList/LCM0/CABC_settings", &cabc_file_ops},
	{NULL}, },
	LCM0_aie[] = {
	{"AllHWList/LCM0/AIE_settings", &aie_file_ops},
	{NULL}, },
	LCM0_awer_cnt[] = {
	{"AllHWList/LCM0/awer_cnt", &awer_cnt_operations},
	{NULL}, },
	LCM0_awer_status[] = {
	{"AllHWList/LCM0/awer_status", &awer_status_operations},
	{NULL}, },
	LCM0_reg_read[] = {									//SW4-HL-Display-DynamicReadWriteRegister-00+_20160729
	{"AllHWList/LCM0/reg_read", &reg_read_file_ops},
	{NULL}, },
	LCM0_reg_write[] = {								//SW4-HL-Display-DynamicReadWriteRegister-00+_20160729
	{"AllHWList/LCM0/reg_write", &reg_write_file_ops},
	{NULL}, },
	LCM0_hdr[] = {								//SW4-HL-Display-HDR-Ping-00+_20180323
	{"AllHWList/LCM0/hdr_ping", &hdr_file_ops},	
	{NULL}, },
	LCM0_fs_curr[] = {								//SW4-HL-Display-HDR-SetFsCurr-00+_20180515
	{"AllHWList/LCM0/fs_curr", &fs_curr_file_ops},			
	#ifdef CONFIG_AOD_FEATURE	//SW4-HL-Display-GlanceMode-00+{_20170524
	{NULL}, },
	LCM0_lp_set[] = {
    {"AllHWList/LCM0/setlp", &aod_lp_file_ops},
    {NULL}, },
	LCM0_aod[] = {
    {"AllHWList/LCM0/AOD", &aod_file_ops},
	#endif						//SW4-HL-Display-GlanceMode-00+}_20170524
	#if 0
	{NULL}, },
	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+{_20150519
	LCM0_vddio[] = {
	{"AllHWList/LCM0/vddio", &vddio_file_ops},
	{NULL}, },
	LCM0_avdd[] = {
	{"AllHWList/LCM0/avdd", &avdd_file_ops},
	{NULL}, },
	LCM0_avee[] = {
	{"AllHWList/LCM0/avee", &avee_file_ops},
	{NULL}, },
	LCM0_reset[] = {
	{"AllHWList/LCM0/reset", &reset_file_ops},
	{NULL}, },
	LCM0_init[] = {
	{"AllHWList/LCM0/init", &init_file_ops},
	{NULL}, },
	LCM0_ldos[] = {
	{"AllHWList/LCM0/ldos", &ldos_file_ops},
	{NULL},
	};
	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+}_20150519
	#else
	{NULL},
	};
	#endif

static int __init fih_lcm_init(void)
{
	struct proc_dir_entry *lcm0_dir;
	struct proc_dir_entry *ent;

	pr_debug("\n\n*** [HL] %s, AAA CE_enable = %d ***\n\n", __func__, CE_enable);
	pr_debug("\n\n*** [HL] %s, AAA CT_enable = %d ***\n\n", __func__, CT_enable);
	pr_debug("\n\n*** [HL] %s, AAA CABC_enable = %d ***\n\n", __func__, CABC_enable);
	pr_debug("\n\n*** [HL] %s, AAA AIE_enable = %d ***\n\n", __func__, AIE_enable);
	pr_debug("\n\n*** [HL] %s, AAA HDR_enable = %d ***\n\n", __func__, HDR_enable);	//HL+_20180316
	
	//SW4-HL-Display-AddCTCPanelHX8394FInsideSupport-01*_20150522
	lcm0_dir = proc_mkdir("AllHWList/LCM0", NULL);

	ent = proc_create((LCM0_awer_cnt->name) + 15, 0, lcm0_dir, LCM0_awer_cnt->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_awer_cnt->name);
	}
	pr_debug("\n\n*** [LCM] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_awer_cnt->name);

	ent = proc_create((LCM0_awer_status->name) + 15, 0, lcm0_dir, LCM0_awer_status->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_awer_status->name);
	}
	pr_err("\n\n*** [LCM] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_awer_status->name);

	if ((CE_enable) || (CT_enable) || (CABC_enable) || (AIE_enable))
	{
		if (CE_enable)
		{
			ent = proc_create((LCM0_cm->name) + 15, 0, lcm0_dir, LCM0_cm->ops);
			if (ent == NULL)
			{
				pr_err("\n\nUnable to create /proc/%s", LCM0_cm->name);
			}
			pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_cm->name);
		}

		if (CT_enable)
		{
			ent = proc_create((LCM0_ct->name) + 15, 0, lcm0_dir, LCM0_ct->ops);
			if (ent == NULL)
			{
				pr_err("\n\nUnable to create /proc/%s", LCM0_ct->name);
			}
			pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_ct->name);
		}

		if (CABC_enable)
		{
			ent = proc_create((LCM0_cabc->name) + 15, 0, lcm0_dir, LCM0_cabc->ops);
			if (ent == NULL)
			{
				pr_err("\n\nUnable to create /proc/%s", LCM0_cabc->name);
			}
			pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_cabc->name);
		}
		if (AIE_enable)
		{
			ent = proc_create((LCM0_aie->name) + 15, 0, lcm0_dir, LCM0_aie->ops);
			if (ent == NULL)
			{
				pr_err("\n\nUnable to create /proc/%s", LCM0_aie->name);
			}
			pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_aie->name);
		}
	}

	#if 0
	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+{_20150519
	if (vddio_enable)
	{
		ent = proc_create((LCM0_vddio->name) + 15, 0, lcm0_dir, LCM0_vddio->ops);
		if (ent == NULL)
		{
			pr_err("\n\nUnable to create /proc/%s", LCM0_vddio->name);
		}
		pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_vddio->name);
	}

	if (avdd_enable)
	{
		ent = proc_create((LCM0_avdd->name) + 15, 0, lcm0_dir, LCM0_avdd->ops);
		if (ent == NULL)
		{
			pr_err("\n\nUnable to create /proc/%s", LCM0_avdd->name);
		}
		pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_avdd->name);
	}

	if (avee_enable)
	{
		ent = proc_create((LCM0_avee->name) + 15, 0, lcm0_dir, LCM0_avee->ops);
		if (ent == NULL)
		{
			pr_err("\n\nUnable to create /proc/%s", LCM0_avee->name);
		}
		pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_avee->name);
	}

	ent = proc_create((LCM0_reset->name) + 15, 0, lcm0_dir, LCM0_reset->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_reset->name);
	}
	pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_reset->name);

	ent = proc_create((LCM0_init->name) + 15, 0, lcm0_dir, LCM0_init->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_init->name);
	}
	pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_init->name);

	ent = proc_create((LCM0_ldos->name) + 15, 0, lcm0_dir, LCM0_ldos->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_ldos->name);
	}
	pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_ldos->name);
	//SW4-HL-Display-PowerPinControlPinAndInitCodeAPI-00+}_20150519
	#endif

	//SW4-HL-Display-DynamicReadWriteRegister-00+{_20160729
	ent = proc_create((LCM0_reg_read->name) + 15, 0, lcm0_dir, LCM0_reg_read->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_reg_read->name);
	}
	pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_reg_read->name);

	ent = proc_create((LCM0_reg_write->name) + 15, 0, lcm0_dir, LCM0_reg_write->ops);
	if (ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_reg_write->name);
	}
	pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_reg_write->name);
	//SW4-HL-Display-DynamicReadWriteRegister-00+}_20160729

	//SW4-HL-Display-GlanceMode-00+{_20170524
	#ifdef CONFIG_AOD_FEATURE
	ent = proc_create((LCM0_aod->name) + 15, 0, lcm0_dir, LCM0_aod->ops);
	if(ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_aod->name);
	}
	pr_err("\n\n*** [LCM] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_aod->name);

	ent = proc_create((LCM0_lp_set->name) + 15, 0, lcm0_dir, LCM0_lp_set->ops);
	if(ent == NULL)
	{
		pr_err("\n\nUnable to create /proc/%s", LCM0_lp_set->name);
	}
	pr_err("\n\n*** [LCM] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_lp_set->name);
	#endif
	//SW4-HL-Display-GlanceMode-00+}_20170524
	
	//SW4-HL-Display-HDR-Ping-00+{_20180323
	//HDR Ping
	if (HDR_enable)
	{
		ent = proc_create((LCM0_hdr->name) + 15, 0, lcm0_dir, LCM0_hdr->ops);
		if (ent == NULL)
		{
			pr_err("\n\nUnable to create /proc/%s", LCM0_hdr->name);
		}
		pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_hdr->name);

		//SW4-HL-Display-HDR-SetFsCurr-00+{_20180515
		ent = proc_create((LCM0_fs_curr->name) + 15, 0, lcm0_dir, LCM0_fs_curr->ops);
		if (ent == NULL)
		{
			pr_err("\n\nUnable to create /proc/%s", LCM0_fs_curr->name);
		}
		pr_debug("\n\n*** [HL] %s, succeed to create proc/%s ***\n\n", __func__, LCM0_fs_curr->name);
		//SW4-HL-Display-HDR-SetFsCurr-00+}_20180515
	}
	//SW4-HL-Display-HDR-Ping-00+}_20180323

	return (0);
}

static void __exit fih_lcm_exit(void)
{
	remove_proc_entry(LCM0_awer_cnt->name, NULL);
	remove_proc_entry(LCM0_awer_status->name, NULL);
	if (CE_enable)
	{
		remove_proc_entry(LCM0_cm->name, NULL);
	}

	if (CT_enable)
	{
		remove_proc_entry(LCM0_ct->name, NULL);
	}

	if (CABC_enable)
	{
		remove_proc_entry(LCM0_cabc->name, NULL);
	}

	if (AIE_enable)
	{
		remove_proc_entry(LCM0_aie->name, NULL);
	}

	#if 0
	//SW4-HL-Display-AddCTCPanelHX8394FInsideSupport-01+{_20150522
	if (vddio_enable)
	{
		remove_proc_entry(LCM0_vddio->name, NULL);
	}

	if (avdd_enable)
	{
		remove_proc_entry(LCM0_avdd->name, NULL);
	}

	if (avee_enable)
	{
		remove_proc_entry(LCM0_avee->name, NULL);
	}

	remove_proc_entry(LCM0_reset->name, NULL);
	remove_proc_entry(LCM0_init->name, NULL);
	remove_proc_entry(LCM0_ldos->name, NULL);
	//SW4-HL-Display-AddCTCPanelHX8394FInsideSupport-01+}_20150522
	#endif

	//SW4-HL-Display-DynamicReadWriteRegister-00+{_20160729
	remove_proc_entry(LCM0_reg_read->name, NULL);
	remove_proc_entry(LCM0_reg_write->name, NULL);
	//SW4-HL-Display-DynamicReadWriteRegister-00+}_20160729

	//SW4-HL-Display-GlanceMode-00+{_20170524
	#ifdef CONFIG_AOD_FEATURE
	remove_proc_entry(LCM0_aod->name, NULL);
	remove_proc_entry(LCM0_lp_set->name, NULL);
	#endif
	//SW4-HL-Display-GlanceMode-00+}_20170524
	
	//SW4-HL-Display-HDR-Ping-00+{_20180323
	//HDR Ping
	if (HDR_enable)
	{
		remove_proc_entry(LCM0_hdr->name, NULL);
		remove_proc_entry(LCM0_fs_curr->name, NULL);	//SW4-HL-Display-HDR-SetFsCurr-00+_20180515
	}	
	//SW4-HL-Display-HDR-Ping-00+}_20180323
}

late_initcall(fih_lcm_init);
module_exit(fih_lcm_exit);
