/*  
 *  A simple function to keep useful parameters in a special partition.
 *
 *  Copyright (c) 2017-2020, zhangzhe.sharp.corp, All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  Modify record:
 *  <author>	<date>		<version>	<desc>
 *  zhangzhe	2017-09-01	1.00		create
 *
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/wakelock.h>
#include <linux/sharp_param.h>


//SP_DEBUG_LEVEL: 0-all,8-bare
#define SP_DEBUG_LEVEL		(16)
#define SP_DEBUG_ALWAYS		(1)
#define SP_DEBUG_NORMAL		((SP_DEBUG_LEVEL>=16)?1:0)
#define SP_DEBUG_HARDLY		((SP_DEBUG_LEVEL>=32)?1:0)
#define SP_DEBUG_FORBID		(0)

#define SP_HEAD			"SharpParams:"
#define FUNC_HEAD(fmt)		"%s(): " fmt, __func__
#define LOGE(fmt, ...)\
        printk(KERN_ERR SP_HEAD FUNC_HEAD(fmt), ##__VA_ARGS__)
#define LOGD(fmt, ...)\
        printk(KERN_DEBUG SP_HEAD FUNC_HEAD(fmt), ##__VA_ARGS__)

#define PARAMS_PATH     	"/dev/block/bootdevice/by-name/reserved"
#define THIS_DEVICE_NAME	"SD1"
typedef char ablock[PARAM_BLOCK_SIZE];
static ablock **params;
static unsigned int BlockChanged = 0;
static unsigned int ParamsInited = 0;
static int NeedFormat = 1;
static int LoadParams = 0;

//wake lock timeout
static struct wake_lock  params_wake_lock_timeout;

//spin lock
DEFINE_SPINLOCK(param_lock);

//delayed work
struct delayed_work	sharp_delay_init_work;
struct delayed_work     sharp_sync_disk_work;
int sharp_param_read(char *buff);

#define DYNAMIC_ALLOC           (1)
#if(DYNAMIC_ALLOC)
static int __init sharp_param_early_init(void)
{
	static ablock **bp;
        unsigned int amount;
        unsigned int i;


	amount = PARAM_BLOCK_NUM;
	LOGE(" BLOCK_SIZE = %d, BLOCK_NUM_LIMIT = %d, BLOCK_NEED_NOW = %d\n",PARAM_BLOCK_SIZE, (PARAM_SIZE/PARAM_BLOCK_SIZE),amount);

	if(amount>(PARAM_SIZE / PARAM_BLOCK_SIZE))
	{
		LOGE("Block amount is OVER RANGE!\n");
		return -1;
	}

	bp = kmalloc_array(amount, sizeof(struct  ablock *), GFP_KERNEL);
	if (!bp) {
		LOGE(" CAN NOT allocate page-pointer-array!\n");
		return -ENOMEM;
	}

        for (i = 0; i < amount; i++) {
		bp[i] = kmalloc(PARAM_BLOCK_SIZE, GFP_KERNEL);
        }

	params = bp;

        return 0;
}
subsys_initcall(sharp_param_early_init)
#else
static int __init sharp_param_early_init(void)
{
//point to a array which contains pointer to each page
	struct page **pp;
//amount of pages
	unsigned int amount;
	unsigned int i;

	amount = PARAM_SIZE / PAGE_SIZE;
	LOGE(" PAGE_SIZE = %ld, PAGE_SHIFT = %d, amount = %d\n",PAGE_SIZE,PAGE_SHIFT,amount);

	pp = kmalloc_array(amount, sizeof(struct page *), GFP_KERNEL);
	if (!pp) {
		LOGE(" CAN NOT allocate page-pointer-array!\n");
		return -ENOMEM;
	}

	for (i = 0; i < amount; i++) {
		phys_addr_t addr = PARAM_BASE + i * PAGE_SIZE;
		pp[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	params = (ablock *)vmap(pp, amount, VM_MAP, PAGE_KERNEL);

	kfree(pp);

	return 0;
}
subsys_initcall(sharp_param_early_init)
#endif

static int sharp_param_load(int index)
{
        const int size = PARAM_BLOCK_SIZE;
        unsigned char *buff;
        int fd = -1;
        int ret;

        //LOGD("index[%d]\n",index);
	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

        if (index < 0) {
                LOGE(" do NOT ask me doing things for nothing!\n");
                return -1;
        }

        buff = kmalloc(size, GFP_KERNEL);
        if (!buff)
        {
                LOGE(" kmalloc fail!\n");
		kfree(buff);
                return -ENOMEM;
        }

        fd = sys_open(PARAMS_PATH, O_RDONLY, 0);
        if (fd < 0) {
                LOGE(" fd = %d, sys_open partition: %s fail\n",fd,PARAMS_PATH);
		sys_close(fd);
		kfree(buff);
                return -1;
        }

        sys_lseek(fd, index * PARAM_BLOCK_SIZE, SEEK_SET);
        ret = sys_read(fd, buff, PARAM_BLOCK_SIZE);
        if (ret < PARAM_BLOCK_SIZE)
        {
                LOGE(" load block[%d] fail, ret=%d\n", index,ret);
                sys_close(fd);
		kfree(buff);
                return -1;
        }

        spin_lock(&param_lock);
        memcpy(params[index],buff, PARAM_BLOCK_SIZE);
        spin_unlock(&param_lock);
        LOGD(" load block[%d] = %s\t... done!\n", index,(char *)(params[index]));

        sys_close(fd);

	kfree(buff);

        return 0;
}

static int get_block_index(const char *buff)
{
        int i;

        for (i = 0; i < PARAM_BLOCK_NUM; i++) {
                if (!strncmp(sharp_params_map[i], buff, NAME_LENGTH)) {
                        //LOGD("block[%d]=%s found.\n",i, buff);
                        return i;
                }
        }

	LOGE("can't find block[%s]!\n", buff);
        return -ERANGE;
}

int sharp_param_read(char *buff)
{
	int index=-1;

	if (!params) {
                LOGE("Params is NOT inited!\n");
                return -ENODEV;
        }

	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

        index = get_block_index(buff);
        if(index < 0) {
                LOGE("Can't get block:%s!\n", buff);
                return -ERANGE;
        }

	spin_lock(&param_lock);
	memcpy(buff, params[index], PARAM_BLOCK_SIZE);
	spin_unlock(&param_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(sharp_param_read);

int sharp_param_write(const char *buff)
{
	int index=-1;

	if (!params) {
		LOGE("Params is NOT inited!\n");
		return -ENODEV;
	}

	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

        index = get_block_index(buff);
	if(index < 0) {
		LOGE("Can't get block[%s]!\n", buff);
		return -ERANGE;
	}

	spin_lock(&param_lock);
	memcpy(params[index], buff, PARAM_BLOCK_SIZE);
	BlockChanged |= (1 << index);
	schedule_delayed_work(&sharp_sync_disk_work, msecs_to_jiffies(0));
	spin_unlock(&param_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(sharp_param_write);

static int sharp_param_format(int force)
{
	int index=-1;
	int ret=0;
	static ablock                   buff;
        struct BlockDeviceInfo 		*buff_device;
	struct BlockManufactureInfo 	*buff_manu;
	struct BlockCommunicationInfo	*buff_comm;
	struct BlockSecRelatedSetting	*buff_sec;	
	struct BlockRunModeSetting 	*buff_mode;
	LOGE("\n");

	if (!params) {
                LOGE("Params is NOT inited!\n");
                return -ENODEV;
        }

	wake_lock_timeout(&params_wake_lock_timeout, HZ/50);

        if(!force)
	{
		if(sharp_param_load(0)<0)
		{
			LOGE(" sharp_param_load ERROR!\n");
			return -1;
		}

		buff_device = (struct BlockDeviceInfo *)(buff);
		sprintf(buff_device->BlockName, "%s", "DEVICE");
		if(sharp_param_read((char *)buff_device)<0)
		{
			LOGE("ERROR to read block[%s]\n",buff_device->BlockName);
		}

		//LOGE(" ParamsFormat = %d, PARAMFORMAT = %d", *(unsigned int *)(buff_device->ParamsFormat),PARAMFORMAT);
		if(*(unsigned int *)(buff_device->ParamsFormat) == PARAMFORMAT)
		{
			LOGE("FormatFlag was set ...\n");
			if (!strncmp(THIS_DEVICE_NAME,buff_device->DeviceName, NAME_LENGTH))
			{//same
				LOGE("DeviceName is right, NO DEED!\n");
				return 0;
			}
			LOGE("DeviceNameNow is %s, reFormat...\n",buff_device->DeviceName);
		}
	}

//DEVICE
        index = get_block_index("DEVICE");
        if(index < 0) {
                LOGE("Can't get block[DEVICE]!\n");
                return -ERANGE;
        }
	memset(&buff, 0, sizeof(buff));
	buff_device = (struct BlockDeviceInfo *)(&buff);
        sprintf(buff_device->BlockName, 		"%s", "DEVICE");
        sprintf(buff_device->DeviceName, 		"%s", THIS_DEVICE_NAME);
	*(unsigned int	*)(buff_device->ParamVersion) 	= PARAM_VERSION;
	*(unsigned int  *)(buff_device->ParamsFormat)   = PARAMFORMAT;
        memcpy(params[index], (char *)(buff_device), PARAM_BLOCK_SIZE);
	
        BlockChanged |= (1 << index);
	LOGE("sync params: %s\n",(char *)(params[index]));

//MANUFACTURE
        index = get_block_index("MANUFACTURE");
        if(index < 0) {
                LOGE("Can't get block[MANUFACTURE]!\n");
		ret = -ERANGE;
                goto format_out;
        }

	memset(&buff, 0, sizeof(buff));
	buff_manu = (struct BlockManufactureInfo *)(&buff);
        sprintf(buff_manu->BlockName,    		"%s", "MANUFACTURE");
        memcpy(params[index], (char *)(buff_manu), PARAM_BLOCK_SIZE);
        BlockChanged |= (1 << index);
        LOGE("sync params: %s\n",(char *)(params[index]));

//COMMUNICATION
        index = get_block_index("COMMUNICATION");
        if(index < 0) {
                LOGE("Can't get block[COMMUNICATION]!\n");
                ret = -ERANGE;
		goto format_out;
        }

	memset(&buff, 0, sizeof(buff));
	buff_comm = (struct BlockCommunicationInfo *)(&buff);
        sprintf(buff_comm->BlockName,    		"%s", "COMMUNICATION");
        memcpy(params[index], (char *)(buff_comm), PARAM_BLOCK_SIZE);
        BlockChanged |= (1 << index);
        LOGE("sync params: %s\n",(char *)(params[index]));

//SECURTIY
        index = get_block_index("SECURTIY");
        if(index < 0) {
                LOGE("Can't get block[SECURTIY]!\n");
                ret = -ERANGE;
		goto format_out;
        }

	memset(&buff, 0, sizeof(buff));
	buff_sec = (struct BlockSecRelatedSetting *)(&buff);
        sprintf(buff_sec->BlockName,    		"%s", "SECURTIY");
        memcpy(params[index], (char *)(buff_sec), PARAM_BLOCK_SIZE);
        BlockChanged |= (1 << index);
        LOGE("sync params: %s\n",(char *)(params[index]));

//MODE
	index = get_block_index("MODE");
	if(index < 0) {
		LOGE("Can't get block[MODE]!\n");
		ret = -ERANGE;
		goto format_out;
	}
	memset(&buff, 0, sizeof(buff));
	buff_mode = (struct BlockRunModeSetting *)(&buff);
	sprintf(buff_mode->BlockName,			"%s", "MODE");
	*(unsigned short*)(buff_mode->RunMode)		= (unsigned short)RUN_MODE_NORMAL;
	memcpy(params[index], (char *)(buff_mode), PARAM_BLOCK_SIZE);
	BlockChanged |= (1 << index);
	LOGE("sync params: %s\n",(char *)(params[index]));
//##############################

	LoadParams = 1;
	schedule_delayed_work(&sharp_sync_disk_work, msecs_to_jiffies(0));
	LOGE(" wait for writing disk ...\n");
	mdelay(20);
format_out:
	LOGE(" ..................... end\n");
	return ret;
}

static void sharp_param_check(void)
{
	int ret;
	static ablock 			buff;
	struct BlockDeviceInfo 		*block_device;
        struct BlockManufactureInfo     *block_manu;
        struct BlockCommunicationInfo   *block_comm;
        struct BlockSecRelatedSetting   *block_sec;
	struct BlockRunModeSetting	*block_mode;

	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

	block_device = (struct BlockDeviceInfo *)(buff);
	sprintf(block_device->BlockName, "%s", "DEVICE");
	ret = sharp_param_read((char *)block_device);
	if(ret<0)
	{
		LOGE("ERROR to read %s\n",block_device->BlockName);
		return;
	}
	LOGE("Block[%s]: ParamVersion[%d], ParamsFormat[0x%x], ParamReserve[%s], DeviceName[%s], Reserve=%s\n",
		block_device->BlockName, *(unsigned short*)block_device->ParamVersion, *(unsigned int *)block_device->ParamsFormat,
			block_device->ParamReserve, block_device->DeviceName, block_device->Reserve);

	block_manu = (struct BlockManufactureInfo *)(buff);
        sprintf(block_manu->BlockName, "%s", "MANUFACTURE");
        ret = sharp_param_read((char *)block_manu);
        if(ret<0)
        {
                LOGE("ERROR to read %s",block_manu->BlockName);
                return;
        }
	LOGE("Block[%s]: Reserve=%s\n",block_manu->BlockName, block_manu->Reserve);

	block_comm = (struct BlockCommunicationInfo *)(buff);
        sprintf(block_comm->BlockName, "%s", "COMMUNICATION");
        ret = sharp_param_read((char *)block_comm);
        if(ret<0)
        {
                LOGE("ERROR to read %s\n",block_comm->BlockName);
                return;
        }
        LOGE("Block[%s]: Reserve=%s\n",block_comm->BlockName, block_comm->Reserve);

        block_sec = (struct BlockSecRelatedSetting *)(buff);
	sprintf(block_sec->BlockName, "%s", "SECURTIY");
        ret = sharp_param_read((char *)block_sec);
        if(ret<0)
        {
                LOGE("ERROR to read %s\n",block_sec->BlockName);
                return;
        }
        LOGE("Block[%s]: Reserve=%s\n",block_sec->BlockName, block_sec->Reserve);

	block_mode = (struct BlockRunModeSetting *)(buff);
	sprintf(block_mode->BlockName, "%s", "MODE");
	ret = sharp_param_read((char *)block_mode);
	if(ret<0)
	{
		LOGE("ERROR to read %s\n",block_mode->BlockName);
		return;
	}
	LOGE("Block[%s]: RunMode[%d], Reserve=%s\n",
		block_mode->BlockName, *(unsigned short*)block_mode->RunMode, block_mode->Reserve);

	return;
}

static ssize_t sharp_param_sys_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
	size_t size;
	static ablock	buff;

	LOGD("\n");
	if (count < PARAM_BLOCK_SIZE) {
		LOGE(" invalid size %ld \n", count);
		return -EINVAL;
	}

	if((ParamsInited != PARAMINITED)||(!LoadParams))
        {
                schedule_delayed_work(&sharp_delay_init_work, msecs_to_jiffies(0));
                mdelay(50);
                if(ParamsInited != PARAMINITED)
                {
                        LOGE("NOT ready, please try later!\n");
                        return -EAGAIN;
                }
        }

	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

	size = copy_from_user(buff, buf, PARAM_BLOCK_SIZE);
	if (size != 0) {
		LOGE(" failed to copy from userspace, left %ld\n", size);
		return -EACCES;
	}

	LOGD(" try to read block[%s]\n", buff);
	size = sharp_param_read((char *)buff);
	if (size<0)
	{
		LOGE(" read error!\n");
		return -EAGAIN;
	}

	size = copy_to_user(buf, buff, PARAM_BLOCK_SIZE);
	if (size) {
		LOGE(" failed to copy to userspace, left %ld\n", size);
		return -EACCES;
	}
	return PARAM_BLOCK_SIZE;
}

static ssize_t sharp_param_sys_write(struct file *file, const char __user *buf,size_t count, loff_t *ppos)
{
        size_t size;
	static ablock   buff;

	LOGD("\n");
        if (count < PARAM_BLOCK_SIZE) {
                LOGE(" invalid size %ld \n", count);
                return -EINVAL;
        }

	if((ParamsInited != PARAMINITED)||(!LoadParams))
	{
		schedule_delayed_work(&sharp_delay_init_work, msecs_to_jiffies(0));
		mdelay(50);
		if(ParamsInited != PARAMINITED)
		{
			LOGE("NOT ready, please try later!\n");
			return -EAGAIN;
		}
	}

	wake_lock_timeout(&params_wake_lock_timeout, HZ/100);

        size = copy_from_user(buff, buf, PARAM_BLOCK_SIZE);
        if (size != 0) {
                LOGE(" failed to copy from userspace, left %ld\n", size);
                return -EACCES;
        }

        LOGD(" try to write block[%s]\n", buff);
        size = sharp_param_write((char *)buff);
        if (size<0)
        {
                LOGE(" write error!\n");
                return -EAGAIN;
        }

        return PARAM_BLOCK_SIZE;
}

static int delay_init_retry = 0;
static void sharp_delay_init_work_func(struct work_struct *work)
{
	int ret,index;

	LOGD(" NeedFormat = %d, LoadParams = %d\n",NeedFormat,LoadParams);
	wake_lock_timeout(&params_wake_lock_timeout, HZ/20);
	if(NeedFormat)
	{
		ret = sharp_param_format(0);
        	if(ret<0)
        	{
                	LOGE(" sharp_param_format ERROR!\n");
			delay_init_retry++;
			if(delay_init_retry<5)
			schedule_delayed_work(&sharp_delay_init_work, msecs_to_jiffies(1000));
			return;
        	}
		LOGE(" sharp param formated!\n");
		NeedFormat = 0;
	}

	if(!LoadParams)
	{
		for(index=0;index<PARAM_BLOCK_NUM;index++)
		{
			ret = sharp_param_load(index);
			if(ret<0)
			{
				LOGE(" sharp_param_load ERROR!\n");
				return;
			}
		}
		LOGE(" sharp param loaded!\n");
		LoadParams = 1;
	}

	ParamsInited = PARAMINITED; 

	if(SP_DEBUG_HARDLY)
	{
		sharp_param_check();
	}

	return;
}


static int sync_retry 	= 0;
static int sync_fail 	= 0;
static void sharp_sync_disk_work_func(struct work_struct *work)
{
	char buff[512];
	int fd = -1;
	int index;

	if (!BlockChanged)
		return;

	if (unlikely(fd < 0)) {
		fd = sys_open(PARAMS_PATH, O_WRONLY, 0);
		if (fd < 0) {
			LOGE(" fd = %d, sys_open partition: %s fail\n",fd,PARAMS_PATH);
			sync_retry++;
			schedule_delayed_work(&sharp_sync_disk_work, msecs_to_jiffies(sync_retry*5000));
			return;
		}
		else
		{
			sync_retry = 0;
			LOGE(" fd = %d, sys_open partition: %s\n",fd,PARAMS_PATH);
		}
	}

	spin_lock(&param_lock);
	index = fls(BlockChanged);
	if (index) {
		--index;
		memcpy(buff, params[index], PARAM_BLOCK_SIZE);
		BlockChanged &= ~(1 << index);
		spin_unlock(&param_lock);
	} else {
		spin_unlock(&param_lock);
		LOGE(" do NOT ask me doing things for nothing!\n");
		return;
	}

	do{
		int res;
		sys_lseek(fd, index * PARAM_BLOCK_SIZE, SEEK_SET);
		res = sys_write(fd, buff, PARAM_BLOCK_SIZE);
		if (res < PARAM_BLOCK_SIZE) {
			sync_fail++;
			LOGE("sync block[%d] fail %d times, res=%d\n", index,sync_fail,res);
		}
		else
		{
			sync_fail = 0;
			LOGE("sync block[%s] done!\n",(char *)buff);
		}
	}while((sync_fail>0)&&(sync_fail<3));

	if (BlockChanged)
	{
		LOGE("BlockChanged(0x%x) NOT clear!\n",BlockChanged);
		schedule_delayed_work(&sharp_sync_disk_work, msecs_to_jiffies(0));
	}

        return;
}

static const struct file_operations sharp_param_fops = {
	.owner		= THIS_MODULE,
	.read		= sharp_param_sys_read,
	.write		= sharp_param_sys_write,
};

static struct miscdevice sharp_param_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sharp_params",
	.fops = &sharp_param_fops,
};

static int __init sharp_param_init(void)
{
	int ret;
	LOGE("\n");

	if (!params) {
		LOGE("param buffer is NOT allocated!\n");
		return 0;
	}

//wake lock
	wake_lock_init(&params_wake_lock_timeout, WAKE_LOCK_SUSPEND, "sharp_params_wake_lock_timeout");

//init driver
	ret = misc_register(&sharp_param_dev);
	if(ret<0)
	{
		LOGE(" misc_register ERROR!\n");
		return ret;
	}

//sync disk work
	INIT_DELAYED_WORK(&sharp_sync_disk_work, sharp_sync_disk_work_func);

//init delay
	INIT_DELAYED_WORK(&sharp_delay_init_work, sharp_delay_init_work_func);
	schedule_delayed_work(&sharp_delay_init_work, msecs_to_jiffies(5000));

	return ret;
}
module_init(sharp_param_init);

MODULE_DESCRIPTION("sharp_params driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("zhangzhe.sharp");
