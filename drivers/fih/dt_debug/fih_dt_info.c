/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
	
#include <linux/kernel.h>
#include <linux/debugfs.h>
	
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define NODE_NAME	"fih_info_management"
#define PP_NAME_SIZE	125
char pp_name[PP_NAME_SIZE];
struct device_node *fih_dt_node = NULL;

static int fih_dt_proc_open(struct seq_file *m, void *v)
{
	const char *ret_data;

	if(strlen(pp_name)<=1)
		seq_printf(m, "%s\n", "error pp");
	else
	{
		ret_data = of_get_property(fih_dt_node, pp_name, NULL);
		if(ret_data == NULL)
			seq_printf(m, "%s\n", "error pp");
		else
			seq_printf(m, "%s=%s\n", pp_name, ret_data);
	}
	return 0;
}


ssize_t fih_dt_proc_write(struct file *file, const char  __user *buffer,
					size_t count, loff_t *data)
{
	const char *ret_data;

	if(count <=2)
	{
		pr_info("input node is NULL \n"); 
		return -EFAULT;
	}

	if ( copy_from_user(pp_name, buffer, count) ) 
	{
		pr_info("copy_from_user fail\n");
		   return -EFAULT;
	}

	pp_name[count-1] = 0;
	ret_data = of_get_property(fih_dt_node, pp_name, NULL);

	if(ret_data==NULL)
		pr_err("value = NULL \n");
	else
		pr_err("value = %s \n", ret_data); 

	return count;
}

static int fih_dt_seq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_dt_proc_open, NULL);
};

static struct file_operations dtinfo_new_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_dt_seq_proc_open,
	.read    = seq_read,
	.write   = fih_dt_proc_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_dt_init(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("dt_info", 0644, NULL, &dtinfo_new_file_ops);
	if (entry == NULL)
	{
		pr_info("fih_dt_init : unable to create /proc entry\n");
		return -ENOMEM;
	}

	pr_info("fih_dt loaded\n");

	fih_dt_node = of_find_node_by_name(of_find_node_by_path("/"), NODE_NAME);
	if(fih_dt_node == NULL)
	{
		pr_info("fih_dt_node is NULL \n");
		return -ENOMEM;
	}

	memset(pp_name, 0, PP_NAME_SIZE);
	return 0;
}

static void __exit fih_dt_exit(void)
{
	remove_proc_entry("dt_info", NULL);	
	pr_info("dt_info module is unloaded\n");
}

late_initcall(fih_dt_init);
module_exit(fih_dt_exit);
MODULE_AUTHOR("IdaChiang");
MODULE_DESCRIPTION("FIH Device Tree Information");


