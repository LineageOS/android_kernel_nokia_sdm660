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
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"
#include "fih_camera_bbs.h"

#undef SERR
#define SERR(fmt, args...)  pr_err("%s:%d " fmt"\n", __func__, __LINE__, ##args)

#undef SLOW
#undef SINFO
#if FIH_CAMERA_BBS_DEBUG
#define SLOW(fmt, args...)  pr_err("%s:%d " fmt"\n", __func__, __LINE__, ##args)
#define SINFO(fmt, args...)  pr_err("%s:%d " fmt"\n", __func__, __LINE__, ##args)
#else
#define SLOW(fmt, args...)  pr_debug("%s:%d " fmt"\n", __func__, __LINE__, ##args)
#define SINFO(fmt, args...)  pr_info("%s:%d " fmt"\n", __func__, __LINE__, ##args)
#endif

#define FIH_MAX_NAME_LEN MAX_SENSOR_NAME

const char *FIH_BBS_CAMERA_MODULE_NAME[FIH_BBS_CAMERA_MODULE_MAX]=
{
    "sensor",
    "actuator",
    "eeprom",
    "ois",
};

struct fih_camera_sub_module_info{
    int subdev_id;
    char name[FIH_MAX_NAME_LEN];
    enum cci_i2c_master_t cci_master;
    unsigned short sid;
    int save_done;
};

struct fih_camera_sensor_info {
    enum camb_position_t position;
    struct fih_camera_sub_module_info module_info[FIH_BBS_CAMERA_MODULE_MAX];
} *fih_cam_info;

void fih_camera_sensor_info_init(struct fih_camera_sensor_info* info)
{
    int i = 0, id = 0;
    for (id=0;id<FIH_BBS_CAMERA_LOCATION_MAX;id++)
    {
        for (i=0;i<FIH_BBS_CAMERA_MODULE_MAX;i++)
        {
           info[id].module_info[i].subdev_id=-1;
           info[id].module_info[i].cci_master=-1;
           info[id].module_info[i].sid=-1;
           info[id].module_info[i].save_done=0;
        }
    }
}

char* fih_camera_bbs_get_name_by_cci(int master,int sid)
{
    int id=0,i=0;

    if(!fih_cam_info)
        return NULL;

    for (id=0;id<FIH_BBS_CAMERA_LOCATION_MAX;id++)
    {
        for (i=0;i<FIH_BBS_CAMERA_MODULE_MAX;i++)
        {
            if((fih_cam_info[id].module_info[i].cci_master == master) &&
                    (fih_cam_info[id].module_info[i].sid)==sid)
            {
                goto GetName;
            }
        }
    }
    if(id==FIH_BBS_CAMERA_LOCATION_MAX)
    {
        SERR("cannot match cci master and slave address.");
        return NULL;
    }
GetName:
    return fih_cam_info[id].module_info[i].name;
}
EXPORT_SYMBOL(fih_camera_bbs_get_name_by_cci);

void fih_camera_bbs_by_cci(int master,int sid,int error_code)
{
    char module_name[FIH_MAX_NAME_LEN];
    char error_cause[64];
    int id=0,i=0,position=-1,module=0,bbs_id=0,bbs_err=99;

    if(!fih_cam_info)
        return;
    //SLOW("search cci master=%d,sid=%d,err=%d\n",master,sid,error_code);

    for (id=0;id<FIH_BBS_CAMERA_LOCATION_MAX;id++)
    {
        for (i=0;i<FIH_BBS_CAMERA_MODULE_MAX;i++)
        {
            if((fih_cam_info[id].module_info[i].cci_master == master) &&
                    (fih_cam_info[id].module_info[i].sid)==sid)
            {
                position=fih_cam_info[i].position;
                strcpy(module_name, fih_cam_info[id].module_info[i].name);
                module = i;
                goto GetId;
            }
        }
    }
    if(id==FIH_BBS_CAMERA_LOCATION_MAX)
    {
        SERR("cannot match cci master and slave address.");
        return;
    }

GetId:
    //UPD
    switch (error_code) {
        case FIH_BBS_CAMERA_ERRORCODE_POWER_UP: strcpy(error_cause, "Power up fail"); break;
        case FIH_BBS_CAMERA_ERRORCODE_POWER_DW: strcpy(error_cause, "Power down fail"); break;
        case FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR: strcpy(error_cause, "MCLK error"); break;
        case FIH_BBS_CAMERA_ERRORCODE_I2C_READ: strcpy(error_cause, "i2c read err"); break;
        case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE: strcpy(error_cause, "i2c write err"); break;
        case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ: strcpy(error_cause, "i2c seq write err"); break;
        case FIH_BBS_CAMERA_ERRORCODE_UNKOWN: strcpy(error_cause, "unknow error"); break;
        default: strcpy(error_cause, "Unknown"); break;
    }
    printk("BBox::UPD;88::%s::%s\n",module_name,error_cause);

    //UEC
    switch (position) {
        case BACK_CAMERA_B: bbs_id = FIH_BBSUEC_MAIN_CAM_ID; break;
        case FRONT_CAMERA_B: bbs_id = FIH_BBSUEC_FRONT_CAM_ID; break;
        case FIH_BBS_CAMERA_LOCATION_SUB: bbs_id = FIH_BBSUEC_AUX_CAM_ID; break;//temp
        default: bbs_id = 99; break;
    }
    switch (error_code) {
        case FIH_BBS_CAMERA_ERRORCODE_POWER_UP:
        case FIH_BBS_CAMERA_ERRORCODE_POWER_DW:
            switch (module) {
                case FIH_BBS_CAMERA_MODULE_IC: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_SENSOR_POWER; break;
                case FIH_BBS_CAMERA_MODULE_ACTUATOR: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_ACTUATOR_POWER; break;
                case FIH_BBS_CAMERA_MODULE_EEPROM: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_EEPROM_POWER; break;
                case FIH_BBS_CAMERA_MODULE_OIS: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_OIS_POWER; break;
            }
            break;
        case FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR:
            break;
        case FIH_BBS_CAMERA_ERRORCODE_I2C_READ:
        case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE:
        case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ:
            switch (module) {
                case FIH_BBS_CAMERA_MODULE_IC: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_SENSOR_I2C; break;
                case FIH_BBS_CAMERA_MODULE_EEPROM: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_EEPROM_I2C; break;
                case FIH_BBS_CAMERA_MODULE_ACTUATOR: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_ACTUATOR_I2C; break;
                case FIH_BBS_CAMERA_MODULE_OIS: bbs_err = FIH_BBSUEC_CAMERA_ERRORCODE_OIS_I2C; break;
            }
            break;
        case FIH_BBS_CAMERA_ERRORCODE_UNKOWN:
        default:
            break;
    }
    printk("BBox::UEC;%d::%d\n", bbs_id, bbs_err);
}
EXPORT_SYMBOL(fih_camera_bbs_by_cci);

//save actuator info by subid
int fih_camera_bbs_set(int subid,int master,unsigned short sid,int module)
{
    int i=0;

    if(module>=FIH_BBS_CAMERA_MODULE_MAX)
        SERR("error! module %d not exist",module);

    SLOW(" subid=%d,master=%d,sid=0x%x,module=%d",subid,master,sid,module);

    for (i=0;i<FIH_BBS_CAMERA_LOCATION_MAX;i++)
    {
        if(fih_cam_info[i].module_info[module].subdev_id == subid)
            break;
    }
    if(i==FIH_BBS_CAMERA_LOCATION_MAX)
    {
        SERR("cannot match subdev_id=%d",subid);
        return -1;
    }
    if(fih_cam_info[i].module_info[module].save_done)
    {
        SLOW("already save done,bypass");
        return 0;
    }

    fih_cam_info[i].module_info[module].cci_master = master;
    fih_cam_info[i].module_info[module].sid = sid;
    fih_cam_info[i].module_info[module].save_done=1;
    SINFO("set %s parameter for camera%d,master=%d,sid=0x%x",FIH_BBS_CAMERA_MODULE_NAME[module],fih_cam_info[i].position,master,sid);

    return 0;
}
EXPORT_SYMBOL(fih_camera_bbs_set);

int fih_camera_bbs_init(struct msm_sensor_ctrl_t * ctrl)
{
    struct msm_camera_sensor_slave_info *slave_info=NULL;
    int position=-1,id=-1,temp=0;

    if (!ctrl->sensordata->cam_slave_info)
    {
        SERR("cam_slave_info is null pointer");
        return -1;
    }

    slave_info = ctrl->sensordata->cam_slave_info;
    position = slave_info->sensor_init_params.position;
    SLOW("position %d", position);

    switch(position){
        case BACK_CAMERA_B:id = FIH_BBS_CAMERA_LOCATION_MAIN;break;
        case FRONT_CAMERA_B:id = FIH_BBS_CAMERA_LOCATION_FRONT;break;
        case AUX_CAMERA_B:id = FIH_BBS_CAMERA_LOCATION_SUB;break;
        default:SERR("invalid position %d", position);break;
    }

    fih_cam_info[id].position = position;
    //set sensor info
    fih_cam_info[id].module_info[FIH_BBS_CAMERA_MODULE_IC].subdev_id=(int)ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_SENSOR];
    snprintf(fih_cam_info[id].module_info[FIH_BBS_CAMERA_MODULE_IC].name,FIH_MAX_NAME_LEN,"%s",slave_info->sensor_name);
    fih_cam_info[id].module_info[FIH_BBS_CAMERA_MODULE_IC].cci_master = ctrl->cci_i2c_master;
    fih_cam_info[id].module_info[FIH_BBS_CAMERA_MODULE_IC].sid = slave_info->slave_addr >> 1;
    fih_cam_info[id].module_info[FIH_BBS_CAMERA_MODULE_IC].save_done = 1;

    //get actuator subdev id
    if (ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_ACTUATOR]!=-1)
    {
        temp=FIH_BBS_CAMERA_MODULE_ACTUATOR;
        snprintf(fih_cam_info[id].module_info[temp].name,FIH_MAX_NAME_LEN,"%s",slave_info->actuator_name);
        fih_cam_info[id].module_info[temp].subdev_id=(int)ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_ACTUATOR];
        SINFO(" [cam%d][%s]subdev_id=%d",id,FIH_BBS_CAMERA_MODULE_NAME[temp],fih_cam_info[id].module_info[temp].subdev_id);
    }else
        SLOW("no %s module in cam%d",FIH_BBS_CAMERA_MODULE_NAME[temp],id);

    //get eeprom subdev id
    if (ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_EEPROM]!=-1)
    {
        temp=FIH_BBS_CAMERA_MODULE_EEPROM;
        snprintf(fih_cam_info[id].module_info[temp].name,FIH_MAX_NAME_LEN,"%s",slave_info->eeprom_name);
        fih_cam_info[id].module_info[temp].subdev_id=(int)ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_EEPROM];
        SINFO(" [cam%d][%s]subdev_id=%d",id,FIH_BBS_CAMERA_MODULE_NAME[temp],fih_cam_info[id].module_info[temp].subdev_id);
    }else
        SLOW("no %s module in cam%d",FIH_BBS_CAMERA_MODULE_NAME[temp],id);

    //get ois subdev id
    if (ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_OIS]!=-1)
    {
        temp=FIH_BBS_CAMERA_MODULE_OIS;
        snprintf(fih_cam_info[id].module_info[temp].name,FIH_MAX_NAME_LEN,"%s",slave_info->ois_name);
        fih_cam_info[id].module_info[temp].subdev_id=(int)ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_OIS];
        SINFO(" [cam%d][%s]subdev_id=%d",id,FIH_BBS_CAMERA_MODULE_NAME[temp],fih_cam_info[id].module_info[temp].subdev_id);
    }else
        SLOW("no %s module in cam%d",FIH_BBS_CAMERA_MODULE_NAME[temp],id);

    return 0;
}
EXPORT_SYMBOL(fih_camera_bbs_init);

//cam_debug
static int fih_camera_dev_debug_show(struct seq_file *m, void *v)
{
    int id=0,i=0;
    for (id=0;id<FIH_BBS_CAMERA_LOCATION_MAX;id++)
    {
        seq_printf(m," CAM-%d\n",id);
        for (i=0;i<FIH_BBS_CAMERA_MODULE_MAX;i++)
        {
            if(!FIH_BBS_CAMERA_MODULE_NAME[i])
            {
                SERR("FIH_BBS_CAMERA_MODULE_NAME[%d] is NULL",i);
                seq_printf(m," | %s\n",FIH_BBS_CAMERA_MODULE_NAME[i]);
                seq_printf(m," | | [None]\n");
                continue;
            }
            if(fih_cam_info[id].module_info[i].subdev_id < 0)
            {
                SLOW("[CAM%d][%s]no this module",id,FIH_BBS_CAMERA_MODULE_NAME[i]);
                seq_printf(m," | %s\n",FIH_BBS_CAMERA_MODULE_NAME[i]);
                seq_printf(m," | | [None]\n");
                continue;
            }
            seq_printf(m," | %s\n",FIH_BBS_CAMERA_MODULE_NAME[i]);
            seq_printf(m," | | subdevid      %d\n", fih_cam_info[id].module_info[i].subdev_id);
            seq_printf(m," | | name          %s\n", fih_cam_info[id].module_info[i].name);
            seq_printf(m," | | cci_master    %d\n", fih_cam_info[id].module_info[i].cci_master);
            seq_printf(m," | | sid           0x%x\n", fih_cam_info[id].module_info[i].sid);
            seq_printf(m," | | save_done     %d\n", fih_cam_info[id].module_info[i].save_done);
        }
    }
    return 0;
}

static int fih_camera_dev_proc_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, fih_camera_dev_debug_show, NULL);
};
static ssize_t fih_camera_dev_proc_debug_write(struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
{
    unsigned char tmp[5];
    unsigned int size;
    int id=0,i=0;

    size = (count > sizeof(tmp))? sizeof(tmp):count;

    if (copy_from_user(tmp, buffer, size))
    {
        SERR("%s: copy_from_user fail\n", __func__);
        return -EFAULT;
    }

    if(strstr(tmp,"test")!=NULL)
    {
        SLOW("get:%s,start to print BBS log",tmp);
        for (id=0;id<FIH_BBS_CAMERA_LOCATION_MAX;id++)
        {
            for (i=0;i<FIH_BBS_CAMERA_MODULE_MAX;i++)
            {
                if(!fih_cam_info[id].module_info[i].save_done)
                    continue;
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_POWER_UP);
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_POWER_DW);
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR);
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_I2C_READ);
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE);
                fih_camera_bbs_by_cci(fih_cam_info[id].module_info[i].cci_master,fih_cam_info[id].module_info[i].sid,FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ);
             }
        }
    }
    return size;
}
static struct file_operations debug_file_ops = {
    .owner   = THIS_MODULE,
    .open    = fih_camera_dev_proc_debug_open,
    .read    = seq_read,
    .write   = fih_camera_dev_proc_debug_write,
    .llseek  = seq_lseek,
    .release = single_release
};


static int __init fih_camera_dev_probe(void)
{
    int fsize = 0;
    struct proc_dir_entry *entry = 0;

    fsize = sizeof(struct fih_camera_sensor_info)*FIH_BBS_CAMERA_LOCATION_MAX;
    fih_cam_info = kzalloc(fsize, GFP_KERNEL);
    if (!fih_cam_info) {
        SERR("failed no memory\n");
        return -ENOMEM;
    }

    fih_camera_sensor_info_init(fih_cam_info);

    entry = proc_create("cam_debug", 0444, NULL, &debug_file_ops);
    if (!entry) {
        SERR(" fail to create /proc/cam_debug");
    }

    SINFO("fih_camera_dev_probe alloc success (%d)",fsize);
    return (0);
}

module_init(fih_camera_dev_probe);

