#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <mach/msm_iomap.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "fih_ramtable.h"

#define FIH_RF_NV_ST_ADDR FIH_MODEM_RF_NV_BASE
#define FIH_RF_NV_ST_SIZE FIH_MODEM_RF_NV_SIZE
#define FIH_CUST_NV_ST_ADDR FIH_MODEM_CUST_NV_BASE
#define FIH_CUST_NV_ST_SIZE FIH_MODEM_CUST_NV_SIZE

#define CUST_BIN_OFFSET 256
#define PATH_XML "/mnt/vendor/persist/SKUID_profileInfo.xml"

#define RF_PARTITIOM_OFFSET 0
#define CUST_PARTITIOM_OFFSET SZ_2M

#define DEVELOP_STAGE_SKU 0

static char ftm_xml[]="<Profile-Info>\n\
    <Brand>FTM</Brand>\n\
    <Locale>FTM</Locale>\n\
    <Carrier>FTM</Carrier>\n\
</Profile-Info>\n";

static char aos_xml[]="<Profile-Info>\n\
    <Brand>FIH Generic</Brand>\n\
    <Locale>FIH Generic</Locale>\n\
    <Carrier>FIH Generic</Carrier>\n\
</Profile-Info>\n";

#if DEVELOP_STAGE_SKU
#define BACKUP_FILE_rf "/data/brf.bin"
#define BACKUP_FILE_cust "/data/bcust.bin"

static int fih_backup_rf(void)
{
	struct file *fp = NULL;
	mm_segment_t oldfs;
	char *path = NULL;
	int fsize = SZ_2M;
	int output_size = 0;
	int ret = 0;
	loff_t pos = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(BACKUP_FILE_rf, O_CREAT|O_RDWR, 0644);
	if (IS_ERR(fp)) {
		pr_err("[%s] Fail to open %s\n", __func__, BACKUP_FILE_rf);
		goto exit_fs;
	}

	path = (char *)ioremap(FIH_RF_NV_ST_ADDR, FIH_RF_NV_ST_SIZE);
	if (path == NULL) {
		pr_err("[%s] ioremap fail\n", __func__);
		goto exit_fp;
	}

	//output_size = fp->f_op->write(fp, path, fsize, &fp->f_pos);
  pos = fp->f_pos;
  len = vfs_write(fp, path, fsize, &pos);
	
	if (output_size < fsize) {
		pr_err("[%s] write error\n", __func__);
		goto exit_io;
	}

	pr_info("[%s] done\n", __func__);
	ret = 1;

exit_io:
	iounmap(path);
exit_fp:
	filp_close(fp, NULL);
exit_fs:
	set_fs(oldfs);

	return ret;
}

static int fih_backup_cust(void)
{
	struct file *fp = NULL;
	mm_segment_t oldfs;
	char *path = NULL;
	int fsize = SZ_2M;
	int output_size = 0;
	int ret = 0;
  loff_t pos = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(BACKUP_FILE_cust, O_CREAT|O_RDWR, 0644);
	if (IS_ERR(fp)) {
		pr_err("[%s] Fail to open %s\n", __func__, BACKUP_FILE_cust);
		goto exit_fs;
	}

	path = (char *)ioremap(FIH_CUST_NV_ST_ADDR, FIH_CUST_NV_ST_SIZE);
	if (path == NULL) {
		pr_err("[%s] ioremap fail\n", __func__);
		goto exit_fp;
	}

	//output_size = fp->f_op->write(fp, path, fsize, &fp->f_pos);
  pos = fp->f_pos;
  len = vfs_write(fp, path, fsize, &pos);
	if (output_size < fsize) {
		pr_err("[%s] write error\n", __func__);
		goto exit_io;
	}

	pr_info("[%s] done\n", __func__);
	ret = 1;

exit_io:
	iounmap(path);
exit_fp:
	filp_close(fp, NULL);
exit_fs:
	set_fs(oldfs);

	return ret;
}
#endif

static int fih_ftm_sku_to_cust(void)
{
	char *path = NULL;
	int fsize = 0;

	path = (char *)ioremap(FIH_CUST_NV_ST_ADDR, FIH_CUST_NV_ST_SIZE);
	if (path == NULL) {
		pr_err("[%s] ioremap fail\n", __func__);
		return 0;
	}
  
	fsize = sizeof(ftm_xml);
	if (fsize > CUST_BIN_OFFSET) {
		pr_err("[%s] warning: fsize (%d > %d)\n", __func__, fsize, CUST_BIN_OFFSET);
		fsize = CUST_BIN_OFFSET;
	}
	pr_info("[%s] ftm_xml=%s\n", __func__, ftm_xml);
	memcpy((path + (SZ_2M - CUST_BIN_OFFSET)), ftm_xml, fsize);
	path[fsize] = '\0';
	iounmap(path);

	pr_info("[%s] done\n", __func__);
	return 3;
}

static int fih_aos_sku_to_cust(void)
{
	char *path = NULL;
	int fsize = 0;
	char *xml = NULL;

	path = (char *)ioremap(FIH_CUST_NV_ST_ADDR, FIH_CUST_NV_ST_SIZE);
	if (path == NULL) {
		pr_err("[%s] ioremap fail\n", __func__);
		return 0;
	}

	fsize = sizeof(aos_xml);
	xml = aos_xml;
	pr_info("[%s] fsize = %d\n", __func__, fsize);
	pr_info("[%s] xml = (%s)\n", __func__, xml);

	if (fsize > CUST_BIN_OFFSET) {
		pr_err("[%s] warning: fsize (%d > %d)\n", __func__, fsize, CUST_BIN_OFFSET);
		fsize = CUST_BIN_OFFSET;
	}

	memcpy((path + (SZ_2M - CUST_BIN_OFFSET)), xml, fsize);
	path[fsize] = '\0';
	iounmap(path);

#if DEVELOP_STAGE_SKU
	fih_backup_cust();
	fih_backup_rf();
#endif

	pr_info("[%s] done\n", __func__);
	return 2;
}

int fih_update_sku_to_cust(void)
{
	struct file *fp = NULL;
	mm_segment_t oldfs;
	char *buf_manu = NULL;
	char *path = NULL;
	int fsize = 0;
	int output_size = 0;
  loff_t pos = 0;

	if(strstr(saved_command_line,"androidboot.mode=2")!=NULL){ //for FTM mode
		pr_info("[%s] ftm\n", __func__);
		return fih_ftm_sku_to_cust();
	}

#if DEVELOP_STAGE_SKU
	fih_backup_cust();
	fih_backup_rf();
#endif

	pr_info("[%s] aos\n", __func__);

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(PATH_XML, O_RDONLY, 0);
	if (IS_ERR(fp)){
		pr_err("[%s] fail to open %s\n", __func__, PATH_XML);
		pr_err("Open skuid profile.xml failed. ERROR 1\n");
		set_fs(oldfs);
		return fih_aos_sku_to_cust();
	}

	fsize = fp->f_op->llseek(fp, 0, SEEK_END);
	fp->f_op->llseek(fp, 0, SEEK_SET);

	pr_info("[%s] fsize = %d\n", __func__, fsize);
	buf_manu = kzalloc(fsize, GFP_KERNEL);
	if (buf_manu == NULL) {
		pr_err("[%s] buf_manu fail\n", __func__);
		pr_err("Open skuid profile.xml failed. ERROR 2\n");
		filp_close(fp,NULL);
		set_fs(oldfs);
		return fih_aos_sku_to_cust();
	}

	//output_size = fp->f_op->read(fp, buf_manu, fsize, &fp->f_pos);
  pos = fp->f_pos;
	output_size = vfs_read(fp, buf_manu, fsize, &pos);
	if (output_size < fsize) {
		pr_err("[%s] fail to read %s\n", __func__, PATH_XML);
		pr_err("Open skuid profile.xml failed. ERROR 3\n");
		kfree(buf_manu);
		filp_close(fp,NULL);
		set_fs(oldfs);
		return fih_aos_sku_to_cust();
	}

	filp_close(fp, NULL);
	set_fs(oldfs);

	if (fsize > CUST_BIN_OFFSET) {
		fsize = CUST_BIN_OFFSET;
		pr_err("[%s] warning: fsize (%d > %d)\n", __func__, fsize, CUST_BIN_OFFSET);
	}

	pr_info("[%s] xml = (%s)\n", __func__, buf_manu);
	path = (char *)ioremap(FIH_CUST_NV_ST_ADDR, FIH_CUST_NV_ST_SIZE);
	memcpy((path + (SZ_2M - CUST_BIN_OFFSET)), buf_manu, fsize);
	path[fsize] = '\0';
	iounmap(path);
	kfree(buf_manu);

#if DEVELOP_STAGE_SKU
	fih_backup_cust();
	fih_backup_rf();
#endif

	pr_info("[%s] done\n", __func__);
	return 1;
}
EXPORT_SYMBOL(fih_update_sku_to_cust);

static char *my_proc_name = "mdm_sku_xml";
static unsigned int my_proc_addr = FIH_CUST_NV_ST_ADDR;
static unsigned int my_proc_size = FIH_CUST_NV_ST_SIZE;
static unsigned int my_proc_len = 512;

/* This function is called at the beginning of a sequence.
 * ie, when:
 * - the /proc file is read (first time)
 * - after the function stop (end of sequence)
 */
static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
	if (((*pos)*PAGE_SIZE) >= my_proc_len) return NULL;
	return (void *)((unsigned long) *pos+1);
}

/* This function is called after the beginning of a sequence.
 * It's called untill the return is NULL (this ends the sequence).
 */
static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	++*pos;
	return my_seq_start(s, pos);
}

/* This function is called at the end of a sequence
 */
static void my_seq_stop(struct seq_file *s, void *v)
{
	/* nothing to do, we use a static value in start() */
}

/* This function is called for each "step" of a sequence
 */
static int my_seq_show(struct seq_file *s, void *v)
{
/*	int n = (int)v-1;*/
	long n = (long)v-1;
	char *buf = (char *)ioremap(my_proc_addr, my_proc_size);
	char *tmp = NULL;
	unsigned int i;

	if (buf == NULL) {
		return 0;
	}

	tmp = buf +(SZ_2M - CUST_BIN_OFFSET);
	for (i = 0; i < my_proc_size; i++ ) {
		if (tmp[i] == 0x00) break;
	}
	my_proc_len = i;

	if (my_proc_len < (PAGE_SIZE*(n+1))) {
		seq_write(s, (tmp+(PAGE_SIZE*n)), (my_proc_len - (PAGE_SIZE*n)));
	} else {
		seq_write(s, (tmp+(PAGE_SIZE*n)), PAGE_SIZE);
	}

	iounmap(buf);

	return 0;
}

/* This structure gather "function" to manage the sequence
 */
static struct seq_operations my_seq_ops = {
	.start = my_seq_start,
	.next  = my_seq_next,
	.stop  = my_seq_stop,
	.show  = my_seq_show
};

/* This function is called when the /proc file is open.
 */
static int my_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &my_seq_ops);
};

/* This structure gather "function" that manage the /proc file
 */
static struct file_operations my_file_ops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

/* This function is called when the module is loaded
 */
static int __init my_module_init(void)
{
	if (proc_create(my_proc_name, 0, NULL, &my_file_ops) == NULL) {
		pr_err("fail to create proc/%s\n", my_proc_name);
		return (1);
	}

	return 0;
}

/* This function is called when the module is unloaded.
 */
static void __exit my_module_exit(void)
{
	remove_proc_entry(my_proc_name, NULL);
}

module_init(my_module_init);
module_exit(my_module_exit);
