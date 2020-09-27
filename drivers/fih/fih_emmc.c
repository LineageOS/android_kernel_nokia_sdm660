#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include "fih_emmc.h"
#include "fih_ramtable.h"

#define FIH_PROC_DIR   "AllHWList"
#define FIH_PROC_PATH  "AllHWList/emmcinfo"
#define FIH_PROC_SIZE  FIH_EMMC_SIZE

#define FIH_EMMC_DR52  0x44523532  //DR52
#define FIH_EMMC_HS20  0x48533230  //HS20
#define FIH_EMMC_HS40  0x48533430  //HS40
static unsigned int emmc_ramtable_addr = (FIH_MEM_MEM_ADDR+0x18);
static unsigned int emmc_ramtable_size = 4;

static char fih_proc_data[FIH_PROC_SIZE] = "unknown";

static int fih_emmc_bbs(void)
{
  int *buf = (int *)ioremap(emmc_ramtable_addr, emmc_ramtable_size);

	if (buf == NULL) {
		return 0;
	}

  if(*buf == FIH_EMMC_DR52)
  {
    printk ("BBox::UEC;6::5\n");
  }
  iounmap(buf);

  return 0;
}

void fih_emmc_setup(char *info)
{
	snprintf(fih_proc_data, sizeof(fih_proc_data), "%s", info);
}

static int fih_proc_read_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fih_proc_data);
	return 0;
}

static int emmcinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_proc_read_show, NULL);
};

static struct file_operations emmcinfo_file_ops = {
	.owner   = THIS_MODULE,
	.open    = emmcinfo_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_proc_init(void)
{
  fih_emmc_bbs(); //Check t BBS

	if (proc_create(FIH_PROC_PATH, 0, NULL, &emmcinfo_file_ops) == NULL)
        {
                proc_mkdir(FIH_PROC_DIR, NULL);
                if (proc_create(FIH_PROC_PATH, 0, NULL, &emmcinfo_file_ops) == NULL)
                {
                        pr_err("fail to create proc/%s\n", FIH_PROC_PATH);
                        return (1);
                }
        }

	return (0);
}

static void __exit fih_proc_exit(void)
{
	remove_proc_entry(FIH_PROC_PATH, NULL);
}

module_init(fih_proc_init);
module_exit(fih_proc_exit);
