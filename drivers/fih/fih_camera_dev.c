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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "../../include/soc/qcom/camera2.h"

#undef pr_info
#define pr_info(fmt, args...)  pr_err("%s:%d" fmt"\n", __func__, __LINE__, ##args)

struct fih_camera_sensor_info {
	char sensor_name[32];
	enum camb_position_t position;
};

// back:0, front:1, aux:2
struct fih_camera_sensor_info cam_info[4];

//main camera
static int fih_camera_dev_back_show(struct seq_file *m, void *v)
{
	pr_err("%s : %d", cam_info[0].sensor_name, cam_info[0].position);
	seq_printf(m, "%s : %d\n", cam_info[0].sensor_name, cam_info[0].position);
	return 0;
}

static int fih_camera_dev_proc_back_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_camera_dev_back_show, NULL);
};

static struct file_operations back_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_camera_dev_proc_back_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

//front camera
static int fih_camera_dev_front_show(struct seq_file *m, void *v)
{
	pr_err("%s : %d\n", cam_info[1].sensor_name, cam_info[1].position);
	seq_printf(m, "%s : %d\n", cam_info[1].sensor_name, cam_info[1].position);
	return 0;
}

static int fih_camera_dev_proc_front_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_camera_dev_front_show, NULL);
};

static struct file_operations front_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_camera_dev_proc_front_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

//aux camera
static int fih_camera_dev_aux_show(struct seq_file *m, void *v)
{
	pr_info("%s : %d\n", cam_info[2].sensor_name, cam_info[2].position);
	seq_printf(m, "%s : %d\n", cam_info[2].sensor_name, cam_info[2].position);
	return 0;
}

static int fih_camera_dev_wide_camera_show(struct seq_file *m, void *v)
{
	pr_info("%s : %d\n", cam_info[3].sensor_name, cam_info[3].position);
	seq_printf(m, "%s : %d\n", cam_info[3].sensor_name, cam_info[3].position);
	return 0;
}


static int fih_camera_dev_proc_aux_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_camera_dev_aux_show, NULL);
};


static int fih_camera_dev_proc_wide_camera_open(struct inode *inode, struct file *file)
{
	return single_open(file, fih_camera_dev_wide_camera_show, NULL);
};


static struct file_operations aux_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_camera_dev_proc_aux_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static struct file_operations wide_camera_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_camera_dev_proc_wide_camera_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};


int fih_camera_dev_init(struct msm_camera_sensor_slave_info *slave_info)
{
	struct proc_dir_entry *entry = 0;
	char my_proc_name[16];
	int position = slave_info->sensor_init_params.position;
	pr_err("%s, position: %d\n", __func__, position);

	switch(position){
		case BACK_CAMERA_B:
			#if 0
			//SW-NJ-implement-TAS camera add main2 proc interface*{_20181207 */
			if(!strcmp(slave_info->sensor_name,"s5kgm1sp")){
			sprintf(my_proc_name,"cam_back");
			//set cam info
			sprintf(cam_info[0].sensor_name, slave_info->sensor_name);
			cam_info[0].position = position;
			entry = proc_create(my_proc_name, 0444, NULL, &back_file_ops);
			}
			else if(!strcmp(slave_info->sensor_name,"hi846")){
				sprintf(my_proc_name,"cam_wide");
				//set cam info
				sprintf(cam_info[3].sensor_name, slave_info->sensor_name);
				cam_info[3].position = position;
				entry = proc_create(my_proc_name, 0444, NULL, &wide_camera_file_ops);

			}
			else
			//SW-NJ-implement-TAS camera add main2 proc interface*}_20181207 */
			#endif
			{
				sprintf(my_proc_name,"cam_back");
				//set cam info
				sprintf(cam_info[0].sensor_name, slave_info->sensor_name);
				cam_info[0].position = position;
				entry = proc_create(my_proc_name, 0444, NULL, &back_file_ops);

			}
				
			break;

		case FRONT_CAMERA_B:
			sprintf(my_proc_name,"cam_front");
			//set cam info
			sprintf(cam_info[1].sensor_name, slave_info->sensor_name);
			cam_info[1].position = position;
			entry = proc_create(my_proc_name, 0444, NULL, &front_file_ops);
			break;

		case AUX_CAMERA_B:
			#if 1
			//SW-NJ-implement-TAS camera add main2 proc interface*{_20181207 */
			if(!strcmp(slave_info->sensor_name,"s5k5e9yu05")){
			sprintf(my_proc_name,"cam_aux");
			//set cam info
			sprintf(cam_info[2].sensor_name, slave_info->sensor_name);
			cam_info[2].position = position;
			entry = proc_create(my_proc_name, 0444, NULL, &aux_file_ops);
			}
			else if(!strcmp(slave_info->sensor_name,"hi846")){
				sprintf(my_proc_name,"cam_wide");
				//set cam info
				sprintf(cam_info[3].sensor_name, slave_info->sensor_name);
				cam_info[3].position = position;
				entry = proc_create(my_proc_name, 0444, NULL, &wide_camera_file_ops);

			}
			else
			//SW-NJ-implement-TAS camera add main2 proc interface*}_20181207 */
		   #endif
			{
			sprintf(my_proc_name,"cam_aux");
			//set cam info
			sprintf(cam_info[2].sensor_name, slave_info->sensor_name);
			cam_info[2].position = position;
			entry = proc_create(my_proc_name, 0444, NULL, &aux_file_ops);
			}
			break;

		default:
			//sprintf(my_proc_name,"cam_%d", position);
			//entry = proc_create(my_proc_name, 0444, NULL, &my_file_ops);
			pr_err("invalid position %d", position);
			break;
	}
	if (!entry) {
		pr_err("%s: fail create %s proc\n", my_proc_name, my_proc_name);
	}
	return 0;
}
