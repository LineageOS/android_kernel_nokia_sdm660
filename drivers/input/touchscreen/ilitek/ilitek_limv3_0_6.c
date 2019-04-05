/*
 * Copyright (C) 2006-2015 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Steward Fu
 * Maintain: Luca Hsu, Tigers Huang
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,8)
  #include <linux/input/mt.h>
#endif
#include "ilitek.h"
#include "ilitek_fw.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
/*Win add for proc callback function*/
#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
#include "../../../fih/fih_touch.h"
#endif
#include <linux/socket.h>
#include <net/net_namespace.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <linux/proc_fs.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/udp.h>
extern int tp_probe_success;
extern struct fih_touch_cb touch_cb;
char fih_touch_ilitek[32]={0};
/*Win add for proc callback function*/
#define PINCTRL_STATE_ACTIVE  "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND "pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE "pmx_ts_release"
int tp_st_count = 0;
//module information
MODULE_AUTHOR("Steward_Fu");
MODULE_DESCRIPTION("ILITEK I2C touchscreen driver for linux platform");
MODULE_LICENSE("GPL");

int driver_information[] = {DERVER_VERSION_MAJOR, DERVER_VERSION_MINOR, RELEASE_VERSION};

int touch_key_hold_press = 0;
#ifdef VIRTUAL_KEY_PAD
int touch_key_code[] = {KEY_MENU, KEY_HOME/*KEY_HOMEPAGE*/, KEY_BACK, KEY_VOLUMEDOWN, KEY_VOLUMEUP};
int touch_key_press[] = {0, 0, 0, 0, 0};
#endif
unsigned long touch_time = 0;

#ifdef CONFIG_AOD_FEATURE
extern int aod_en;	//SW4-HL-Touch-ImplementDoubleTap-00+_20170623
#endif

static int ilitek_i2c_register_device(void);
static void ilitek_set_input_param(struct input_dev*, int, int, int);
static int ilitek_i2c_read_tp_info(void);
static int ilitek_init(void);
static void ilitek_exit(void);

//i2c functions
int ilitek_i2c_transfer(struct i2c_client*, struct i2c_msg*, int);
static int ilitek_i2c_read(struct i2c_client*, uint8_t*, int);
static int ilitek_i2c_process_and_report(void);
static int ilitek_i2c_suspend(struct i2c_client*, pm_message_t);
static int ilitek_i2c_resume(struct i2c_client*);
static void ilitek_i2c_shutdown(struct i2c_client*);
static int ilitek_i2c_probe(struct i2c_client*, const struct i2c_device_id*);
static int ilitek_i2c_remove(struct i2c_client*);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h);
static void ilitek_i2c_late_resume(struct early_suspend *h);
#endif

static int ilitek_i2c_polling_thread(void*);
static irqreturn_t ilitek_i2c_isr(int, void*);
static void ilitek_i2c_irq_work_queue_func(struct work_struct*);

//file operation functions
static int ilitek_file_open(struct inode*, struct file*);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static ssize_t ilitek_file_write(struct file*, const char*, size_t, loff_t*);
static ssize_t ilitek_file_read(struct file*, char*, size_t, loff_t*);
static int ilitek_file_close(struct inode*, struct file*);

void ilitek_i2c_irq_enable(void);
void ilitek_i2c_irq_disable(void);

int ilitek_i2c_reset(void);
int Glance_mode_on(void);
int Glance_mode_off(void);

//global variables
struct i2c_data i2c;
static struct dev_data dev;
static char DBG_FLAG;
static char Report_Flag;
volatile static char int_Flag;
volatile static char update_Flag;
static int update_timeout;
int gesture_flag, gesture_count, getstatus;
static struct sock *netlink_sock;
bool gesture_state = true;
bool i2c_retry = true;
int Iramtest_status = -1;
int gdouble_tap_enable;	//SW4-HL-Touch-ImplementDoubleTap-00+_20170623
static int gLcmOnOff = 0;	//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00+_20170927

//static int get_time_diff(struct timeval *past_time);//
#ifdef CONFIG_FB
	struct notifier_block ilitek_notifier;
#endif
#ifdef CONFIG_OF
static struct of_device_id ili_match_table[] = {
	#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
	{ .compatible = "ilitech,ili2121x",},
	#else
	{ .compatible = "tchip,ilitek" },
	#endif
	{ },
};
#endif
//i2c id table
static const struct i2c_device_id ilitek_i2c_id[] = {
  {ILITEK_I2C_DRIVER_NAME, 0}, {}
};
MODULE_DEVICE_TABLE(i2c, ilitek_i2c_id);

//declare i2c function table
static struct i2c_driver ilitek_i2c_driver = {
  .id_table = ilitek_i2c_id,
  .driver = {
    .name = ILITEK_I2C_DRIVER_NAME,
    .owner = THIS_MODULE,
#ifdef CONFIG_OF
	#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
    .of_match_table = ili_match_table,
	#else
	.of_match_table = of_match_ptr(ili_match_table),
	#endif
#endif
  },
//  .resume = ilitek_i2c_resume,
//  .suspend  = ilitek_i2c_suspend,
  .shutdown = ilitek_i2c_shutdown,
  .probe = ilitek_i2c_probe,
  .remove = ilitek_i2c_remove,
};
//declare file operations
struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
  .unlocked_ioctl = ilitek_file_ioctl,
#else
  .ioctl = ilitek_file_ioctl,
#endif
  .read = ilitek_file_read,
  .write = ilitek_file_write,
  .open = ilitek_file_open,
  .release = ilitek_file_close,
};

void udp_reply(int pid,int seq,void *payload,int size)
{
	struct sk_buff	*skb;
	struct nlmsghdr	*nlh;
	int		len = NLMSG_SPACE(size);
	void		*data;
	int ret;
	u8 buf[64]={0};
	memcpy(buf,payload,53);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;
	nlh= nlmsg_put(skb, pid, seq, 0, size, 0);
	nlh->nlmsg_flags = 0;
	data=NLMSG_DATA(nlh);
	memcpy(data, payload, size);
	NETLINK_CB(skb).portid = 0;         /* from kernel */
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	ret=netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret <0)
	{
		printk("ilitek send failed\n");
		return;
	}
	return;

}

/* Receive messages from netlink socket. */
uint8_t ttpid = 100, seq = 23/*, sid*/;
kuid_t uid;
static void udp_receive(struct sk_buff  *skb)
{
	void			*data;
	uint8_t buf[64] = {0};
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)skb->data;
	ttpid  = 100;//NETLINK_CREDS(skb)->ttpid;
	uid  = NETLINK_CREDS(skb)->uid;
	//sid  = NETLINK_CB(skb).sid;
	seq  = 23;//nlh->nlmsg_seq;
	data = NLMSG_DATA(nlh);
	//printk("recv skb from user space uid:%d ttpid:%d seq:%d,sid:%d\n",uid,ttpid,seq,sid);
	if(!strcmp(data,"Hello World!"))
	{
		printk("recv skb from user space uid:%d ttpid:%d seq:%d\n",uid.val,ttpid,seq);
		printk("data is :%s\n",(char *)data);
		udp_reply(ttpid,seq,data,sizeof("Hello World!"));
		//udp_reply(ttpid,seq,data);
	}
	else
	{
		memcpy(buf,data,64);
	}
	//kfree(data);
	return ;
}

/*
description
  open function for character device driver
prarmeters
  inode
      inode
  filp
      file pointer
return
  status
*/
static int ilitek_file_open(struct inode *inode, struct file *filp)
{
  DBG("%s\n", __func__);
  return 0;
}
unsigned int hex_2_dec(char *hex, int len)
{
	unsigned int ret = 0, temp = 0;
	int i, shift = (len - 1) * 4;

	for(i = 0; i < len; shift -= 4, i++)
	{
		if((hex[i] >= '0') && (hex[i] <= '9'))
		{
			temp = hex[i] - '0';
		}
		else if((hex[i] >= 'a') && (hex[i] <= 'f'))
		{
			temp = (hex[i] - 'a') + 10;
		}
		else if((hex[i] >= 'A') && (hex[i] <= 'F'))
		{
			temp = (hex[i] - 'A') + 10;
		}
		else{
			return -1;
		}
		ret |= (temp << shift);
	}
	return ret;
}

/*
description
  write function for character device driver
prarmeters
  filp
    file pointer
  buf
    buffer
  count
    buffer length
  f_pos
    offset
return
  status
*/
static ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned char buffer[128] = {0};

	mm_segment_t fs;
	fs = get_fs();
	set_fs(KERNEL_DS);

  //before sending data to touch device, we need to check whether the device is working or not
  if(i2c.valid_i2c_register == 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, i2c device driver doesn't be registered\n", __func__);
    return -1;
  }

  //check the buffer size whether it exceeds the local buffer size or not
  if(count > 128)
  {
    printk(ILITEK_ERROR_LEVEL "%s, buffer exceed 128 bytes\n", __func__);
    return -1;
  }

  //copy data from user space
  ret = copy_from_user(buffer, buf, count - 1);
  if(ret < 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed", __func__);
    return -1;
  }

	pr_debug("[HL]%s: buffer = (%s)\n", __func__, buffer);

  //parsing command
  if(strcmp(buffer, "dbg") == 0)
  {
	pr_debug("[HL]%s: buffer = dbg\n", __func__);
    DBG_FLAG = !DBG_FLAG;
    printk("%s, %s message(%X).\n", __func__, DBG_FLAG ? "Enabled" : "Disabled", DBG_FLAG);
  }
  else if(strcmp(buffer, "info") == 0)
  {
	pr_debug("[HL]%s: buffer = info\n", __func__);
    ilitek_i2c_read_tp_info();
  }
  else if(strcmp(buffer, "report") == 0)
  {
	pr_debug("[HL]%s: buffer = report\n", __func__);
		i2c.report_status =! i2c.report_status;
		printk("i2c.report_status=%d\n", i2c.report_status);
  }
  else if(strcmp(buffer, "stop_report") == 0)
  {
	pr_debug("[HL]%s: buffer = stop_report\n", __func__);
    i2c.report_status = 0;
    printk("The report point function is disable.\n");
  }
  else if(strcmp(buffer, "start_report") == 0)
  {
	pr_debug("[HL]%s: buffer = start_report\n", __func__);
    i2c.report_status = 1;
    printk("The report point function is enable.\n");
  }
  else if(strcmp(buffer, "update_flag") == 0)
  {
	pr_debug("[HL]%s: buffer = update_flag\n", __func__);
    printk("update_Flag=%d\n", update_Flag);
  }
  else if(strcmp(buffer, "irq_status") == 0)
  {
	pr_debug("[HL]%s: buffer = irq_status\n", __func__);
    printk("i2c.irq_status = %d\n", i2c.irq_status);
  }
  else if(strcmp(buffer, "disable_irq") == 0)
  {
	pr_debug("[HL]%s: buffer = disable_irq\n", __func__);
    ilitek_i2c_irq_disable();
    printk("i2c.irq_status = %d\n", i2c.irq_status);
  }
  else if(strcmp(buffer, "enable_irq") == 0)
  {
	pr_debug("[HL]%s: buffer = enable_irq\n", __func__);
    ilitek_i2c_irq_enable();
    printk("i2c.irq_status=%d\n", i2c.irq_status);
  }
  else if(strcmp(buffer, "reset") == 0)
  {
	pr_debug("[HL]%s: buffer = reset\n", __func__);
    ilitek_i2c_reset();
  }
	//SW4-HL-Touch-ImplementDoubleTap-00*{_20170623
	#if 0
	else if(strcmp(buffer, "gesture") == 0)
	{
	printk("start gesture_state\n");
	gesture_state =! gesture_state;
	printk("%s, %s gesture_state(%X).\n", __func__, gesture_state ? "Enabled" : "Disabled", gesture_state);
	printk("end gesture\n");
	}
	#else
	else if(strcmp(buffer, "gesture_enable") == 0)
	{
		pr_debug("[HL]%s: buffer = gesture_enable\n", __func__);
		gesture_state = true;
		printk("%s, %s gesture_state(%X).\n", __func__, gesture_state ? "Enabled" : "Disabled", gesture_state);
	}
	else if(strcmp(buffer, "gesture_disable") == 0)
	{
		pr_debug("[HL]%s: buffer = gesture_disable\n", __func__);
		gesture_state = false;
		printk("%s, %s gesture_state(%X).\n", __func__, gesture_state ? "Enabled" : "Disabled", gesture_state);
	}
	else if(strcmp(buffer, "gesture_status") == 0)
	{
		pr_debug("[HL]%s: buffer = gesture_status\n", __func__);
		printk("%s, %s gesture_state(%X).\n", __func__, gesture_state ? "Enabled" : "Disabled", gesture_state);
	}
	#endif
	//SW4-HL-Touch-ImplementDoubleTap-00*}_20170623
   else if(strcmp(buffer, "update") == 0)
  {
	pr_debug("[HL]%s: buffer = update\n", __func__);
	touch_upgrade_write(0);
  }
   else if(strcmp(buffer, "suspend") == 0)
  {
	pr_debug("[HL]%s: buffer = suspend\n", __func__);
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
  }
   else if(strcmp(buffer, "resume") == 0)
  {
	pr_debug("[HL]%s: buffer = resume\n", __func__);
	ilitek_i2c_resume(i2c.client);
  }
	else if(strcmp(buffer, "change_dbgmode") == 0)
  {
	pr_debug("[HL]%s: buffer = change_dbgmode\n", __func__);
    i2c.debug_flag = !i2c.debug_flag;
    pr_err("%s, %s debug mode(%X).\n", __func__, i2c.debug_flag ? "Enabled" : "Disabled", i2c.debug_flag);
  }
	else if(strcmp(buffer, "iramtest") == 0)
	{
		pr_debug("[HL]%s: buffer = iramtest\n", __func__);
		ilitek_i2c_IRam_test();
		if(Iramtest_status == 0)
		{
			printk("IRam Test:Pass\n");
		}
		else
		{
			printk("IRam Test:Fail\n");
		}
	}
#ifdef GESTURE_FUN
#if GESTURE_FUN == GESTURE_FUN1
  else if(strcmp(buffer, "readgesturelist") == 0)
  {
	pr_debug("[HL]%s: buffer = readgesturelist\n", __func__);
    readgesturelist();
  }
#endif
#endif
  return count;
}

/*
description
  ioctl function for character device driver
prarmeters
  inode
    file node
  filp
    file pointer
  cmd
    command
  arg
    arguments
return
  status
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
  static unsigned char buffer[512] = {0};
  static int len = 0, i;
  int ret;
  struct i2c_msg msgs[] = {
    {.addr = i2c.client->addr, .flags = 0, .len = len, .buf = buffer,}
  };

  //parsing ioctl command
  switch(cmd)
  {
    case ILITEK_IOCTL_I2C_WRITE_DATA:
      ret = copy_from_user(buffer, (unsigned char*)arg, len);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed\n", __func__);
        return -1;
      }
    #ifdef  SET_RESET
      if(buffer[0] == 0x60)
      {
        ilitek_i2c_reset();
      }
    #endif
      ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, i2c write, failed\n", __func__);
        return -1;
      }
      break;
    case ILITEK_IOCTL_I2C_READ_DATA:
      msgs[0].flags = I2C_M_RD;
      ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, i2c read, failed\n", __func__);
        return -1;
      }
      ret = copy_to_user((unsigned char*)arg, buffer, len);

      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
        return -1;
      }
      break;
    case ILITEK_IOCTL_I2C_WRITE_LENGTH:
    case ILITEK_IOCTL_I2C_READ_LENGTH:
      if(len>512){ //CWE-22 Buferr overflow need to protect
        len=0;
        return -1;
      }
      len = arg;
      break;
    case ILITEK_IOCTL_DRIVER_INFORMATION:
      for(i = 0; i < 3; i++)
      {
        buffer[i] = driver_information[i];
      }
      ret = copy_to_user((unsigned char*)arg, buffer, 3);
      break;
    case ILITEK_IOCTL_I2C_UPDATE:
      break;
    case ILITEK_IOCTL_I2C_INT_FLAG:
      if(update_timeout == 1)
      {
        buffer[0] = int_Flag;
        ret = copy_to_user((unsigned char*)arg, buffer, 1);
        if(ret < 0)
        {
          printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
          return -1;
        }
      }
      else
      {
        update_timeout = 1;
      }
      break;
    case ILITEK_IOCTL_START_READ_DATA:
      i2c.stop_polling = 0;
      if(i2c.client->irq != 0)
      {
        ilitek_i2c_irq_enable();
      }
      i2c.report_status = 1;
			printk("The report point function is enable,i2c.report_status=%d.\n", i2c.report_status);
      break;
    case ILITEK_IOCTL_STOP_READ_DATA:
      i2c.stop_polling = 1;
      if(i2c.client->irq != 0)
      {
        ilitek_i2c_irq_disable();
      }
      i2c.report_status = 0;
			printk("The report point function is disable,i2c.report_status=%d.\n", i2c.report_status);
      break;
    case ILITEK_IOCTL_I2C_SWITCH_IRQ:
      ret = copy_from_user(buffer, (unsigned char*)arg, 1);
      if(buffer[0] == 0)
      {
        if(i2c.client->irq != 0)
        {
          ilitek_i2c_irq_disable();
        }
      }
      else
      {
        if(i2c.client->irq != 0)
        {
          ilitek_i2c_irq_enable();
        }
      }
      break;
    case ILITEK_IOCTL_UPDATE_FLAG:
      update_timeout = 1;
      update_Flag = arg;
      DBG("%s, update_Flag = %d\n", __func__, update_Flag);
      break;
    case ILITEK_IOCTL_I2C_UPDATE_FW:
      ret = copy_from_user(buffer, (unsigned char*)arg, 35);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, copy data from user space, failed\n", __func__);
        return -1;
      }
      msgs[0].len = buffer[34];
      ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
    #ifndef CLOCK_INTERRUPT
      ilitek_i2c_irq_enable();
    #endif
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, i2c write, failed\n", __func__);
        return -1;
      }

      int_Flag = 0;
      update_timeout = 0;

      break;
    case ILITEK_IOCTL_I2C_GESTURE_FLAG:
      gesture_flag = arg;
      printk("%s, gesture_flag = %d\n", __func__, gesture_flag);
      break;
    case ILITEK_IOCTL_I2C_GESTURE_RETURN:
      buffer[0] = getstatus;
      printk("%s, getstatus = %d\n", __func__, getstatus);
      ret = copy_to_user((unsigned char*)arg, buffer, 1);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
        return -1;
      }
      //getstatus = 0;
      break;
  #ifdef GESTURE_FUN
  #if GESTURE_FUN == GESTURE_FUN_1
    case ILITEK_IOCTL_I2C_GET_GESTURE_MODEL:
      for(i = 0; i < 32; i = i + 2)
      {
        buffer[i] = gesture_model_value_x(i / 2);
        buffer[i + 1]= gesture_model_value_y(i / 2);
        printk("x[%d] = %d, y[%d] = %d\n", i / 2, buffer[i], i / 2, buffer[i + 1]);
      }
      ret = copy_to_user((unsigned char*)arg, buffer, 32);
      if(ret < 0)
      {
        printk(ILITEK_ERROR_LEVEL "%s, copy data to user space, failed\n", __func__);
        return -1;
      }
      //getstatus = 0;
      break;
    case ILITEK_IOCTL_I2C_LOAD_GESTURE_LIST:
      printk("start\n");
      readgesturelist();
      printk("end--------------\n");
      break;
  #endif
  #endif
	case ILITEK_IOCTL_INT_STATUS:
		put_user(gpio_get_value(i2c.irq_gpio), (int *)arg);
		break;
	case ILITEK_IOCTL_DEBUG_SWITCH:
		ret = copy_from_user(buffer, (unsigned char*)arg, 1);
		printk("ilitek The debug_flag = %d.\n", buffer[0]);
		if (buffer[0] == 0)
		{
			i2c.debug_flag = false;
		}
		else if (buffer[0] == 1)
		{
			i2c.debug_flag = true;
		}
		break;
    default:
      return -1;
  }
    return 0;
}

/*
description
  read function for character device driver
prarmeters
  filp
      file pointer
  buf
      buffer
  count
      buffer length
  f_pos
      offset
return
  status
*/
static ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
  return 0;
}

/*
description
  close function
prarmeters
  inode
      inode
  filp
      file pointer
return
  status
*/
static int ilitek_file_close(struct inode *inode, struct file *filp)
{
  DBG("%s\n", __func__);
  return 0;
}

/*
description
  set input device's parameter
prarmeters
  input
    input device data
  max_tp
    single touch or multi touch
  max_x
    maximum x value
  max_y
    maximum y value
return
  nothing
*/
static void ilitek_set_input_param(struct input_dev *input, int max_tp, int max_x, int max_y)
{
  int key;
  input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
  input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#ifndef ROTATE_FLAG
  input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_x + 2, 0, 0);
  input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_y + 2, 0, 0);
#else
  input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_y + 2, 0, 0);
  input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_x + 2, 0, 0);
#endif
  input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
  input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
#ifdef TOUCH_PRESSURE
	input_set_abs_params(input, ABS_MT_PRESSURE , 0, 255, 0, 0);
#endif
#ifdef TOUCH_PROTOCOL_B
  #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
  input_mt_init_slots(input,max_tp,INPUT_MT_DIRECT);
  #else
  input_mt_init_slots(input,max_tp);
  #endif
#else
  input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
#endif
  set_bit(INPUT_PROP_DIRECT, input->propbit);
	if(i2c.protocol_ver == 0x300 || i2c.protocol_ver == 0x301){
    for(key = 0; key < i2c.keycount; key++)
    {
      if(i2c.keyinfo[key].id <= 0)
      {
        continue;
      }

      set_bit(i2c.keyinfo[key].id & KEY_MAX, input->keybit);
    }
  }
  else if((i2c.protocol_ver & 0x200) == 0x200){
  #ifdef touch_key_code
    for(key = 0; key < sizeof(touch_key_code); key++)
    {
      if(touch_key_code[key] <= 0)
      {
        continue;
      }

      set_bit(touch_key_code[key] & KEY_MAX, input->keybit);
    }
  #endif
  }
#ifdef GESTURE
	input_set_capability(input, EV_KEY, KEY_POWER);
	input_set_capability(input, EV_KEY, KEY_W);
	input_set_capability(input, EV_KEY, KEY_LEFT);
	input_set_capability(input, EV_KEY, KEY_RIGHT);
	input_set_capability(input, EV_KEY, KEY_UP);
	input_set_capability(input, EV_KEY, KEY_DOWN);
	input_set_capability(input, EV_KEY, KEY_O);
	input_set_capability(input, EV_KEY, KEY_C);
	input_set_capability(input, EV_KEY, KEY_E);
	input_set_capability(input, EV_KEY, KEY_M);
	input_set_capability(input, EV_KEY, KEY_WAKEUP);	//SW4-HL-Touch-ChangeDoubleTapReportKeyFromPowerKeyToWakeUpKey-00+_20170630
	device_init_wakeup(&i2c.client->dev, 1);
#endif
  input->name = ILITEK_I2C_DRIVER_NAME;
  input->id.bustype = BUS_I2C;
  input->dev.parent = &(i2c.client)->dev;
}

/*
description
  send message to i2c adaptor
parameter
  client
    i2c client
  msgs
    i2c message
  cnt
    i2c message count
return
  >= 0 if success
  others if error
*/
int ilitek_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs, int cnt)
{
  int ret, count = ILITEK_I2C_RETRY_COUNT;
 #if PLATFORM_FUN == PLATFORM_RK3288
    msgs->scl_rate = 100000;//400000;
#endif
	if(!i2c_retry)
	{
		printk("%s,i2c not retry\n",__func__);
		count = 0;
	}
  while(count >= 0)
  {
    count -= 1;
    ret = down_interruptible(&i2c.wr_sem);
    ret = i2c_transfer(client->adapter, msgs, cnt);
    up(&i2c.wr_sem);
    if(ret < 0 && i2c_retry)
    {
      mdelay(10);
      continue;
    }
    break;
  }
  return ret;
}

/*
description
  read data from i2c device
parameter
  client
    i2c client data
  data
    data for transmission
  length
    data length
return
  status
*/
static int ilitek_i2c_read(struct i2c_client *client, uint8_t *data, int length)
{
  int ret;
  struct i2c_msg msgs[] = {
    {.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
  };

    ret = ilitek_i2c_transfer(client, msgs, 1);
  if(ret < 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
  }
  return ret;
}

/*
description
  process i2c data and then report to kernel
parameters
  none
return
  status
*/
static int ilitek_i2c_process_and_report(void)
{
	int i, len = 0, ret, x = 0, y = 0, pressure = 0, tp_status = 0, release_flag[10] = {0},max_tp = 10;
	struct input_dev *input = i2c.input_dev;
	unsigned char buf[64] = {0};
	DBG("%s,enter\n", __func__);
	if(i2c.report_status == 0)
	{
		return 1;
	}
	//read i2c data from device
	buf[0] = ILITEK_TP_CMD_READ_DATA;
	ret = ilitek_i2c_write_read(i2c.client, buf, 1, 0, buf, 53);
	if(ret < 0)
	{
		return ret;
	}
	len = buf[0];
	ret = 1;
	if(i2c.suspend_flag == 1 && gesture_state == true )
	{
		DBG("ilitek gesture wake up 0x%x, 0x%x, 0x%x\n", buf[0], buf[1], buf[2]);
		switch(buf[2]) {
			case 0x60:
				DBG("gesture wake up this is c\n");
				input_report_key(i2c.input_dev, KEY_C, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_C, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x62:
				DBG("gesture wake up this is e\n");
				input_report_key(i2c.input_dev, KEY_E, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_E, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x64:
				DBG("gesture wake up this is m\n");
				input_report_key(i2c.input_dev, KEY_M, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_M, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x66:
				DBG("gesture wake up this is w\n");
				input_report_key(i2c.input_dev, KEY_W, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_W, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x68:
				DBG("gesture wake up this is o\n");
				input_report_key(i2c.input_dev, KEY_O, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_O, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x84:
				DBG("gesture wake up this is slide right\n");
				input_report_key(i2c.input_dev, KEY_RIGHT, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_RIGHT, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x8c:
				DBG("gesture wake up this is slide left\n");
				input_report_key(i2c.input_dev, KEY_LEFT, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_LEFT, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x88:
				DBG("gesture wake up this is slide down\n");
				input_report_key(i2c.input_dev, KEY_DOWN, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_DOWN, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x80:
				DBG("gesture wake up this is slide up\n");
				input_report_key(i2c.input_dev, KEY_UP, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_UP, 0);
				input_sync(i2c.input_dev);
				break;
			case 0x22:
				DBG("gesture wake up this is duble click\n");
				input_report_key(i2c.input_dev, KEY_WAKEUP, 1);
				input_sync(i2c.input_dev);
				input_report_key(i2c.input_dev, KEY_WAKEUP, 0);
				input_sync(i2c.input_dev);
				break;
			default:
				DBG("no support this gesture!\n");
				break;
		}
		return ret;
	}

	if (i2c.debug_flag)
	{
		udp_reply(ttpid,seq,buf,53);
	}
	if (buf[1] == 0x5F)
	{
		return 0;
	}
	//release point
	if(len == 0)
	{
		DBG("Release, ID=%02X, X=%04d, Y=%04d\n", buf[0] & 0x3F, x, y);
		#ifdef TOUCH_PROTOCOL_B
		for(i = 0; i < max_tp; i++){
			if(i2c.touchinfo[i].flag == 1){
				input_mt_slot(i2c.input_dev, i);
				input_mt_report_slot_state(input, MT_TOOL_FINGER,false);
			}
			i2c.touchinfo[i].flag = 0;
		}
		#else
		input_report_key(i2c.input_dev, BTN_TOUCH, 0);
		input_mt_sync(i2c.input_dev);
		#endif
	}
	else{
		for(i = 0; i < max_tp; i++){
			tp_status = buf[i*5+3] >> 7;
			x = (((buf[i*5+3] & 0x3F) << 8) + buf[i*5+4]);
			y = (buf[i*5+5] << 8) + buf[i*5+6];
			pressure = buf[i*5+7];
			if(tp_status){
				if(touch_key_hold_press == 0){
					#ifdef TOUCH_PROTOCOL_B
					input_mt_slot(i2c.input_dev, i);
					input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER,true);
					i2c.touchinfo[i].flag = 1;
					#endif
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
					#ifdef TOUCH_PRESSURE
					input_event(i2c.input_dev, EV_ABS, ABS_PRESSURE, pressure);
					#endif
					#ifndef TOUCH_PROTOCOL_B
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
					input_report_key(i2c.input_dev, BTN_TOUCH,  1);
					input_mt_sync(i2c.input_dev);
					#endif
					release_flag[i] = 1;
					DBG("Point, ID=%02X, X=%04d, Y=%04d,release_flag[%d]=%d,tp_status=%d\n",i, x,y,i,release_flag[i],tp_status);
				}
				ret = 0;
			}
			else{
				release_flag[i] = 0;
				#ifdef TOUCH_PROTOCOL_B
				if(i2c.touchinfo[i].flag == 1)
				{
					DBG("release, ID=%02X, X=%04d, Y=%04d,release_flag[%d]=%d,tp_status=%d\n",i, x,y,i,release_flag[i],tp_status);
					input_mt_slot(i2c.input_dev, i);
					input_mt_report_slot_state(input, MT_TOOL_FINGER,false);
					i2c.touchinfo[i].flag = 0;
				}
				#else
					input_mt_sync(i2c.input_dev);
				#endif
			}
		}
	}
	#ifdef TOUCH_PROTOCOL_B
	input_mt_report_pointer_emulation(i2c.input_dev, true);
	#endif
	input_sync(i2c.input_dev);
	return ret;
    }
/*
description
  work queue function for irq use
parameter
  work
    work queue
return
  none
*/
static void ilitek_i2c_irq_work_queue_func(struct work_struct *work)
{
  int ret;
#ifndef CLOCK_INTERRUPT
  struct i2c_data *priv = container_of(work, struct i2c_data, irq_work);
#endif
  ret = ilitek_i2c_process_and_report();
  DBG("%s,enter\n",__func__);
#ifdef CLOCK_INTERRUPT
  //ilitek_i2c_irq_enable();
#else
    if(ret == 0)
  {
    if(!i2c.stop_polling)
    {
      mod_timer(&priv->timer, jiffies + msecs_to_jiffies(0));
    }
  }
    else if(ret == 1)
  {
    if(!i2c.stop_polling)
    {
      ilitek_i2c_irq_enable();
    }
    DBG("stop_polling\n");
  }
  else if(ret < 0)
  {
    mdelay(100);
    DBG(ILITEK_ERROR_LEVEL "%s, process error\n", __func__);
    ilitek_i2c_irq_enable();
    }
#endif
}

/*
description
  i2c interrupt service routine
parameters
  irq
    interrupt number
  dev_id
    device parameter
return
  status
*/
static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id)
{
#ifndef CLOCK_INTERRUPT
  if(i2c.irq_status == 1)
  {
    disable_irq_nosync(i2c.client->irq);
    DBG("disable nosync\n");
    i2c.irq_status = 0;
  }
#endif
  if(update_Flag == 1)
  {
    int_Flag = 1;
  }
  else
  {
    queue_work(i2c.irq_work_queue, &i2c.irq_work);
  }
  return IRQ_HANDLED;
}

/*
description
  i2c polling thread
parameters
  arg
    arguments
return
  status
*/
static int ilitek_i2c_polling_thread(void *arg)
{
  int ret = 0;
  DBG(ILITEK_DEBUG_LEVEL "%s, enter\n", __func__);
  //mainloop
  while(1)
  {
    //check whether we should exit or not
    if(kthread_should_stop())
    {
      printk(ILITEK_DEBUG_LEVEL "%s, stop\n", __func__);
      break;
    }

    //this delay will influence the CPU usage and response latency
    mdelay(10);

    //when i2c is in suspend or shutdown mode, we do nothing
    if(i2c.stop_polling)
    {
      mdelay(1000);
      continue;
    }

    //read i2c data
    if(ilitek_i2c_process_and_report() < 0)
    {
      mdelay(3000);
      printk(ILITEK_ERROR_LEVEL "%s, process error\n", __func__);
    }
  }
  return ret;
}
#ifdef UPGRADE_FIRMWARE_ON_BOOT
#if UPGRAGE_FIRMWARE_MODE == UPGRADE_THREAD
static int ilitek_i2c_upgrade_thread(void *arg)
{
	int ret = 0;
	unsigned int timeout;

	timeout = FB_READY_TIMEOUT_S * 1000 / FB_READY_WAIT_MS + 1;
	printk(ILITEK_DEBUG_LEVEL "F@TOUCH %s, enter\n", __func__);
	while (1) {
		mdelay(FB_READY_WAIT_MS);
		timeout--;
		if (timeout == 0) {
			printk("F@TOUCH %s: Timed out waiting for FB ready\n",__func__);
			break;
		}
	}
	touch_upgrade_write(0);

	return ret;
}
#endif
#endif
/*
description
  i2c early suspend function
parameters
  h
    early suspend pointer
return
  none
*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_early_suspend(struct early_suspend *h)
{
  ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
  printk("%s\n", __func__);
}
#endif

/*
description
  i2c later resume function
parameters
  h
    early suspend pointer
return
  none
*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ilitek_i2c_late_resume(struct early_suspend *h)
{
  ilitek_i2c_resume(i2c.client);
  printk("%s\n", __func__);
}
#endif

/*
description
  i2c irq enable function
*/
void ilitek_i2c_irq_enable(void)
{
	pr_debug("[HL]%s: ilitek_i2c_irq_enable\n", __func__);

  if (i2c.irq_status == 0)
  {
    i2c.irq_status = 1;
	i2c.report_status = 1;
    enable_irq(i2c.client->irq);
    pr_info("F@TOUCH %s enable\n",__func__);
  }
  else
  {
    pr_info("F@TOUCH %s no enable\n",__func__);
  }
}

/*
description
    i2c irq disable function
*/
void ilitek_i2c_irq_disable(void)
{
	pr_debug("[HL]%s: ilitek_i2c_irq_disable\n", __func__);

  if (i2c.irq_status == 1)
  {
    i2c.irq_status = 0;
	i2c.report_status = 0;
    disable_irq_nosync(i2c.client->irq);
    pr_info("F@TOUCH %s disable\n",__func__);
  }
  else
  {
    pr_info("F@TOUCH %s no disable\n",__func__);
  }
}

/*
description
  i2c suspend function
parameters
  client
    i2c client data
  mesg
    suspend data
return
  status
*/
static int ilitek_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	#ifdef GESTURE
	unsigned char buf[2] = {0};
	int ret = 0;
	#endif

	int i = 0;

	DBG("F@TOUCH %s\n",__func__);

	if(i2c.valid_irq_request != 0)
	{
		ilitek_i2c_irq_disable();
	}
	else
	{
		i2c.stop_polling = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread polling\n", __func__);
	}
	#ifdef PLATFORM_C1N
	i2c.suspend_flag = 1;
	#ifdef GESTURE
	#ifdef CONFIG_AOD_FEATURE
	pr_debug("[HL]%s: gdouble_tap_enable == %d, aod_en = %d\n", __func__, gdouble_tap_enable, aod_en);
	if ((gdouble_tap_enable) && (!aod_en))
	#else
	pr_debug("[HL]%s: gdouble_tap_enable == %d\n", __func__, gdouble_tap_enable);
	if (gdouble_tap_enable)
	#endif
	{
		pr_debug("[HL]%s: gdouble_tap_enable == true\n", __func__);

		if(gesture_state == true)
		{
			pr_debug("[HL]%s: gesture_state == true\n", __func__);

			buf[0] = 0x01;
			buf[1] = 0x00;
			ret = ilitek_i2c_write_read(i2c.client, buf, 2, 10, buf, 0);
			if(ret < 0)
			{
				pr_err("%s, 0x01 0x00 set tp suspend err, ret %d\n", __func__, ret);
			}
			else
			{
				pr_info("F@TOUCH %s TP sensing stop\n", __func__);
			}
		}
		else
		{
			pr_debug("[HL]%s: gesture_state == false\n", __func__);
		}

		ilitek_i2c_irq_enable();
	}
	else
	{
		pr_debug("[HL]%s: gdouble_tap_enable == false\n", __func__);
	}
	#else
	pr_debug("[HL]%s: GESTURE is not defined!\n", __func__);
	#endif
	#endif

	#ifdef TOUCH_PROTOCOL_B
	for(i = 0; i < i2c.max_tp; i++){
		if(i2c.touchinfo[i].flag == 1){
			input_mt_slot(i2c.input_dev, i);
			input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER,false);
			DBG("Release, ID=%02X\n", i);
		}
		i2c.touchinfo[i].flag = 0;
	}
	#else
	input_report_key(i2c.input_dev, BTN_TOUCH, 0);
	input_mt_sync(i2c.input_dev);
	#endif
	input_sync(i2c.input_dev);

	return 0;
}

void fih_tp_lcm_suspend(void)
{
	//ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);

	//SW4-HL-Touch-ImplementDoubleTap-00*{_20170623
	#if 0
	pr_info("F@TOUCH %s sleep start\n",__func__);

	if(i2c.valid_irq_request != 0)
	{
		ilitek_i2c_irq_disable();
	}
	else
	{
		i2c.stop_polling = 1;
		printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread polling\n", __func__);
	}

	i2c.suspend_flag = 1;
	#else
	pr_debug("[HL]%s: ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND) <-- START\n", __func__);
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
	pr_debug("[HL]%s: ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND) <-- END\n", __func__);
	#endif
	//SW4-HL-Touch-ImplementDoubleTap-00*}_20170623

  return;
}
EXPORT_SYMBOL(fih_tp_lcm_suspend);

//SW4-HL-Touch-ImplementDoubleTap-00+{_20170623
static int ilitek_i2c_suspend_lpwg_on(struct i2c_client *client, pm_message_t mesg)
{
	#ifdef GESTURE
	unsigned char buf[2] = {0};
	int ret = 0;
	#endif

	pr_debug("F@TOUCH %s\n",__func__);

	#ifdef PLATFORM_C1N
	//#ifndef GESTURE
	//pr_info("F@TOUCH %s sleep start\n",__func__);
	//buf[0] = ILITEK_TP_CMD_SLEEP;
	//ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 0);
	//
	//pr_info("F@TOUCH %s sleep end\n",__func__);
	//#else
	#ifdef GESTURE
	#ifdef CONFIG_AOD_FEATURE
	pr_debug("[HL]%s: gdouble_tap_enable == %d, aod_en = %d\n", __func__, gdouble_tap_enable, aod_en);
	if ((gdouble_tap_enable) && (!aod_en))
	#else
	pr_debug("[HL]%s: gdouble_tap_enable == %d\n", __func__, gdouble_tap_enable);
	if (gdouble_tap_enable)
	#endif
	{
		pr_debug("[HL]%s: gdouble_tap_enable == true\n", __func__);

		if(gesture_state == true)
		{
			pr_debug("[HL]%s: gesture_state == true\n", __func__);

			i2c.suspend_flag = 1;

			pr_debug("Enter ilitek_i2c_suspend 0x0A 0x01\n");

			buf[0] = 0x0A;
			buf[1] = 0x01;
			ret = ilitek_i2c_write_read(i2c.client, buf, 2, 10, buf, 0);
			if(ret < 0)
			{
				pr_err("%s, 0x0A 0x01 set tp suspend err, ret %d\n", __func__, ret);
			}
			else
			{
				enable_irq_wake(i2c.client->irq);

				msleep(100);

				pr_info("F@TOUCH %s TP LPWG on\n", __func__);
			}
		}
		//else
		//{
		//	pr_info("F@TOUCH %s sleep start\n",__func__);
		//	buf[0] = ILITEK_TP_CMD_SLEEP;
		//	ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 0);
		//	pr_info("F@TOUCH %s sleep end\n",__func__);
		//}
		else
		{
			pr_debug("[HL]%s: gesture_state == falses\n", __func__);
		}

	}
	else
	{
		pr_debug("[HL]%s: gdouble_tap_enable == false\n", __func__);
	}
	#endif
	#endif

	return 0;
}
 void fih_tp_lcm_suspend_lpwg_on(void)
{
	ilitek_i2c_suspend_lpwg_on(i2c.client, PMSG_SUSPEND);
}
EXPORT_SYMBOL(fih_tp_lcm_suspend_lpwg_on);
//SW4-HL-Touch-ImplementDoubleTap-00+}_20170623

/*
description
  i2c resume function
parameters
  client
    i2c client data
return
  status
*/
static int ilitek_i2c_resume(struct i2c_client *client)
{
	DBG("F@TOUCH %s\n",__func__);

	#if PLATFORM_FUN == PLATFORM_RK3288
	ilitek_i2c_reset();
	#endif

	#ifdef PLATFORM_C1N
	if(i2c.valid_irq_request != 0)
	{
		ilitek_i2c_irq_enable();
	}
	else
	{
		i2c.stop_polling = 0;
		DBG(ILITEK_DEBUG_LEVEL "%s, start i2c thread polling\n", __func__);
	}

	i2c.suspend_flag = 0;
	#endif

	return 0;
}

void fih_tp_lcm_resume(void)
{
	//SW4-HL-Touch-ImplementDoubleTap-00*_20170623
	ilitek_i2c_resume(i2c.client);

	return;
}
EXPORT_SYMBOL(fih_tp_lcm_resume);

//SW4-HL-Touch-ImplementDoubleTap-00+{_20170623
int ilitek_i2c_resume_lpwg_off(struct i2c_client *client)
{
	#ifdef PLATFORM_C1N
	#ifdef GESTURE
	unsigned char buf[2] = {0};
	int ret = 0;

	#ifdef CONFIG_AOD_FEATURE
	pr_debug("[HL]%s: gdouble_tap_enable == %d, aod_en = %d\n", __func__, gdouble_tap_enable, aod_en);
	if ((gdouble_tap_enable) && (!aod_en))
	#else
	pr_debug("[HL]%s: gdouble_tap_enable == %d\n", __func__, gdouble_tap_enable);
	if (gdouble_tap_enable)
	#endif
	{
		pr_debug("[HL]%s: if (gdouble_tap_enable)\n", __func__);

		if(gesture_state == true)
		{
			pr_debug("[HL]%s: gesture_state == true\n", __func__);

			pr_debug("Enter 0x0A 0x00, 0x01 0x01\n");

			buf[0] = 0x0A;
			buf[1] = 0x00;
			ret = ilitek_i2c_write_read(i2c.client, buf, 2, 10, buf, 0);
			if (ret < 0)
			{
				pr_err("%s, 0x0A 0x01 set tp resume err, ret %d\n", __func__, ret);
				return ret;
			}
			else
			{
				msleep(12);

				pr_info("F@TOUCH %s TP LPWG off\n", __func__);
			}
		}
		else
		{
			pr_debug("[HL]%s: gesture_state == false\n", __func__);
		}

	}
	else
	{
		pr_debug("[HL]%s: if (gdouble_tap_enable) is FALSE\n", __func__);
	}
	#endif
	#endif

	return 0;
}

int fih_tp_lcm_resume_lpwg_off(void)
{
	int ret = 0;

	ret = ilitek_i2c_resume_lpwg_off(i2c.client);

	pr_debug("[HL]%s: ret = %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(fih_tp_lcm_resume_lpwg_off);

static int ilitek_i2c_resume_sensing_start(struct i2c_client *client)
{
	#ifdef PLATFORM_C1N
	#ifdef GESTURE
	unsigned char buf[2] = {0};
	int ret = 0;

	#ifdef CONFIG_AOD_FEATURE
	pr_debug("[HL]%s: gdouble_tap_enable == %d, aod_en = %d\n", __func__, gdouble_tap_enable, aod_en);
	if ((gdouble_tap_enable) && (!aod_en))
	#else
	pr_debug("[HL]%s: gdouble_tap_enable == %d\n", __func__, gdouble_tap_enable);
	if (gdouble_tap_enable)
	#endif
	{
		pr_debug("[HL]%s: if (gdouble_tap_enable) is TRUE\n", __func__);

		if(gesture_state == true)
		{
			pr_debug("[HL]%s: gesture_state == true\n", __func__);

			buf[0] = 0x01;
			buf[1] = 0x01;
			ret = ilitek_i2c_write_read(i2c.client, buf, 2, 10, buf, 0);
			if (ret < 0)
			{
				pr_err("%s, 0x01 0x00 set tp resume err, ret %d\n", __func__, ret);
			}

			msleep(40);

			pr_info("F@TOUCH %s TP sensing start\n", __func__);
		}
		else
		{
			pr_debug("[HL]%s: gesture_state == false\n", __func__);
		}

	}
	else
	{
		pr_debug("[HL]%s: if (gdouble_tap_enable) is FALSE\n", __func__);
	}
	#endif
	#endif

	return 0;
}

void fih_tp_lcm_resume_sensing_start(void)
{
	ilitek_i2c_resume_sensing_start(i2c.client);

	return;
}
EXPORT_SYMBOL(fih_tp_lcm_resume_sensing_start);
//SW4-HL-Touch-ImplementDoubleTap-00+}_20170623

/*
description
  reset touch ic
prarmeters
  reset_pin
    reset pin
return
  status
*/
int ilitek_i2c_reset(void)
{
  int ret = 0;
#ifndef SET_RESET
  static unsigned char buffer[64]={0};
  struct i2c_msg msgs[] = {
    {.addr = i2c.client->addr, .flags = 0, .len = 1, .buf = buffer,}
    };
  buffer[0] = 0x60;
  ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
#else
	/*
	____         ___________
    |_______|
       1ms      100ms
*/
	#if PLATFORM_FUN == PLATFORM_TINY210
	if(i2c.irq_status == 1)
	{
		disable_irq_nosync(i2c.client->irq);
		DBG("disable nosync\n");
		i2c.irq_status = 0;
	}
	printk("TP reset\n");
	gpio_direction_output(TP_RESET(x), 0);
	mdelay(10);
	ilitek_i2c_irq_enable();
	gpio_direction_output(TP_RESET(x), 1);
	#else
	printk(" TP reset\n");
	ilitek_i2c_irq_disable();
	gpio_direction_output(i2c.reset_gpio, 1);
	msleep(1);
	gpio_direction_output(i2c.reset_gpio, 0);
	mdelay(1);
	//mdelay(1);
	gpio_direction_output(i2c.reset_gpio, 1);
	mdelay(25);
	ilitek_i2c_irq_enable();
#endif
#endif
	//mdelay(5);
	printk("Touch IC Reboot ... \n");
  return ret;
}

/*
description
  i2c shutdown function
parameters
  client
    i2c client data
return
  nothing
*/
static void ilitek_i2c_shutdown(struct i2c_client *client)
{
  printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
  i2c.stop_polling = 1;
}
#ifdef CONFIG_OF
static int parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;

    i2c.irq_gpio = of_get_named_gpio_flags(np, "touch,irq-gpio", 0, NULL);
    pr_info("F@TOUCH %s IRQ Pin:%d\n",__func__,i2c.irq_gpio);
#ifdef SET_RESET
	i2c.reset_gpio = of_get_named_gpio_flags(np, "touch,reset-gpio", 0, NULL);
    printk("F@TOUCH %s, Reset Pin:%d\n",__func__, i2c.reset_gpio);
#endif
    return 0;
}
#endif

void touch_upgrade_read(char *buf)
{
	sprintf(buf, "%d\n",i2c.fw_upgrade_flag);
	return;
}

void touch_upgrade_write(int flag)
{
	int ret, hexfilesize = 0, i, j,ilifile_len = 0;
	char firmware_name[512];
	struct file *ffilp = NULL;
	unsigned char *ilifile_buf;
	mm_segment_t fs;
	fs = get_fs();
	pr_info("F@TOUCH flag = %d\n",flag);
	if(flag == 0)
	{
		pr_info("F@TOUCH upgrade from /system/etc/firmware/TP_Firmware_ilitek.ili\n");
	snprintf(firmware_name,sizeof(firmware_name),"/system/etc/firmware/%s","TP_Firmware_ilitek.ili");
	ffilp = filp_open(firmware_name, O_RDONLY, (S_IRUGO | S_IWUSR) );
	if(IS_ERR(ffilp))
	{
		printk("\nFile Open Error:%s\n",firmware_name);
		i2c.fw_upgrade_flag = 1;
		goto ERROR;
	}

	if(!ffilp->f_op)
	{
		printk("\nFile Operation Method Error!!\n");
		i2c.fw_upgrade_flag = 1;
		goto ERROR;
	}
	hexfilesize = ffilp->f_op->llseek(ffilp,0,SEEK_END);
	ffilp->f_op->llseek(ffilp,0,SEEK_SET);
	ilifile_buf = kzalloc(hexfilesize, GFP_KERNEL);
	if (!ilifile_buf)
	{
		printk("BBox;%s: firmware upgrade fail, No memory!\n", __func__);
		printk("BBox::UEC;7::6\n");
		i2c.fw_upgrade_flag = 1;
		goto ERROR_FS;
	}
	printk("ili file size = %d\n",hexfilesize);
	memset(ilifile_buf, 0, hexfilesize);
	ret = ffilp->f_op->read(ffilp, ilifile_buf, hexfilesize, &ffilp->f_pos);
	//printk("%s\n", ilifile_buf);
	for(i = 0, j = 0; i < 32 * 5 ; i = i+5, j++)
	{
		ILITEK_CTPM_FW[j] = hex_2_dec(&ilifile_buf[i+2], 2);
		if(j % 16 == 0)
		{
			//printk("0x%06X:", i / 5);
		}
		//printk("0x%02X ", ILITEK_CTPM_FW[j]);
		if(j != 0 && (j+1) % 16 == 0)
		{
			//printk("\n");
		}
	}
	ilifile_len = (( ILITEK_CTPM_FW[26] << 16 ) + ( ILITEK_CTPM_FW[27] << 8 ) + ILITEK_CTPM_FW[28]) + (( ILITEK_CTPM_FW[29] << 16 ) + ( ILITEK_CTPM_FW[30] << 8 ) + ILITEK_CTPM_FW[31]) + 32;
	printk("ili file data zise:%x\n", ilifile_len);
	ilifile_len = ilifile_len * 5 + (0xDFF0/0x10) + 1;
	printk("ili file data zise:%x\n", ilifile_len);
	//for(i = 32 * 5 + 2, j = 32; i < ilifile_len, j < 0xF000; i = i+5, j++)
	for(i = 32 * 5 + 2, j = 32; j < 0xDFFF + 32; i = i+5, j++)
	{
		ILITEK_CTPM_FW[j] = hex_2_dec(&ilifile_buf[i+2], 2);
		//if(j < 0x1000)
		//{
			if(j % 16 == 0)
			{
				//printk("0x%06X:", j - 0x20);
			}
			//printk("0x%02X ", ILITEK_CTPM_FW[j]);
			if(j != 32 && (j+1) % 16 == 0)
			{
				//printk("\n");
				i = i+2;
			}
		//}
	}
	}
	ilitek_i2c_irq_disable();
	i2c.fw_upgrade_flag = ilitek_upgrade_firmware();
	ilitek_i2c_irq_enable();
	i2c_retry = true;
	if(i2c.fw_upgrade_flag<=3 && i2c.fw_upgrade_flag!=0)
	{
		printk("BBox;%s: firmware upgrade fail!\n", __func__);
		printk("BBox::UEC;7::6\n");
	}
ERROR_FS:
	if(flag == 0)
	{
	filp_close(ffilp,NULL);
ERROR:
	set_fs(fs);
	}
	return;
}

#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
static int fts_gpio_setup(int gpio, bool config, int dir, int state)
{
    int retval = 0;
    unsigned char buf[16];

    if (config) {
        snprintf(buf, 16, "fts_gpio_%u\n", gpio);
        retval = gpio_request(gpio, buf);
        if (retval) {
            pr_err("%s: Failed to get gpio %d (code: %d)\n",__func__, gpio, retval);
            return retval;
        }

        if (dir == 0)
            retval = gpio_direction_input(gpio);
        else
            retval = gpio_direction_output(gpio, state);
        if (retval) {
            pr_err("%s: Failed to set gpio %d direction\n",__func__, gpio);
            return retval;
        }
    } else {
        gpio_free(gpio);
    }

    return retval;
}

static int fts_set_gpio(void)
{
    int retval;

    retval = fts_gpio_setup(i2c.irq_gpio,true, 0, 0);
    if (retval < 0) {
        pr_err("%s: Failed to configure attention GPIO\n",__func__);
        goto err_gpio_irq;
    }
    return 0;

err_gpio_irq:
    return retval;
}

static int fts_pinctrl_init(void)
{
    int retval;

    /* Get pinctrl if target uses pinctrl */
    i2c.ts_pinctrl = devm_pinctrl_get(&(i2c.client->dev));
    if (IS_ERR_OR_NULL(i2c.ts_pinctrl)) {
        retval = PTR_ERR(i2c.ts_pinctrl);
        dev_dbg(i2c.client->dev.parent,"Target does not use pinctrl %d\n", retval);
        goto err_pinctrl_get;
    }

    i2c.pinctrl_state_active = pinctrl_lookup_state(i2c.ts_pinctrl, "pmx_ts_active");
    if (IS_ERR_OR_NULL(i2c.pinctrl_state_active)) {
        retval = PTR_ERR(i2c.pinctrl_state_active);
        dev_err(i2c.client->dev.parent,"Can not lookup %s pinstate %d\n",PINCTRL_STATE_ACTIVE, retval);
        goto err_pinctrl_lookup;
    }

    i2c.pinctrl_state_suspend = pinctrl_lookup_state(i2c.ts_pinctrl, "pmx_ts_suspend");
    if (IS_ERR_OR_NULL(i2c.pinctrl_state_suspend)) {
        retval = PTR_ERR(i2c.pinctrl_state_suspend);
        dev_dbg(i2c.client->dev.parent,"Can not lookup %s pinstate %d\n",PINCTRL_STATE_SUSPEND, retval);
        goto err_pinctrl_lookup;
    }

    i2c.pinctrl_state_release = pinctrl_lookup_state(i2c.ts_pinctrl, "pmx_ts_release");
    if (IS_ERR_OR_NULL(i2c.pinctrl_state_release)) {
        retval = PTR_ERR(i2c.pinctrl_state_release);
        dev_dbg(i2c.client->dev.parent,"Can not lookup %s pinstate %d\n",PINCTRL_STATE_RELEASE, retval);
    }

    return 0;

err_pinctrl_lookup:
    devm_pinctrl_put(i2c.ts_pinctrl);
err_pinctrl_get:
    i2c.ts_pinctrl = NULL;
    return retval;
}

/*Win add for proc callback function*/
void touch_vendor_read_ilitek(char *buf)
{
	sprintf(buf, "%s\n","ilitek");
	return;
}

void touch_fwimver_read(char *buf)
{
	return;
}

void touch_fwver_read(char *fwver)
{
  sprintf(fwver, "%s",fih_touch_ilitek);
	return;
}

static int selftest_result_read(void)
{
	pr_info("F@TOUCH %s Iramtest_status=%d",__func__,Iramtest_status);
	return Iramtest_status;
}

static void touch_selftest_write(void)
{
	return;
}

//SW4-HL-Touch-ImplementDoubleTap-00+{_20170623
int touch_double_tap_read_ilitek(void)
{
	pr_debug("%s, gdouble_tap_enable = %d", __func__, gdouble_tap_enable);

	return gdouble_tap_enable;
}
int touch_double_tap_write_ilitek(int enable)
{
	pr_info( "%s: set gdouble_tap_enable = %d", __func__, enable);

	//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00+{_20170927
	if (!gLcmOnOff)
	{
		pr_err("[HL]%s: LCM is OFF! Not Allow To Modify Double Tap Value!\n", __func__);
		return -1;
	}
	else
	{
		pr_debug("[HL]%s: LCM is ON! Allow To Modify Double Tap Value!\n", __func__);
	}
	//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00+{_20170927

	gdouble_tap_enable = enable;
	pr_debug("[HL]%s: gdouble_tap_enable = enable = %d\n", __func__, gdouble_tap_enable);

	return 0;
}
EXPORT_SYMBOL(gdouble_tap_enable);
//SW4-HL-Touch-ImplementDoubleTap-00+}_20170623
#endif

//win add for ALT test
static void touch_alt_reset(void)
{
	int ret = 0;
	static unsigned char buffer[64]={0};
	struct i2c_msg msgs[] = {
	{.addr = i2c.client->addr, .flags = 0, .len = 1, .buf = buffer,}
	};
	ilitek_i2c_irq_disable();
	buffer[0] = 0x60;
	ret = ilitek_i2c_transfer(i2c.client, msgs, 1);
	ilitek_i2c_irq_enable();
	tp_st_count = 0;
	pr_info("F@TOUCH %s tp_st_count=%d",__func__,tp_st_count);
	return ;
}
static int touch_alt_read_count(void)
{
	pr_info("F@TOUCH %s tp_st_count=%d",__func__,tp_st_count);
	return tp_st_count;
}
static void touch_alt_enable(int flag)
{
	ktime_t ktime;
	if(flag == 1)
	{
		pr_info("F@TOUCH %s flag = %d ",__func__,flag);
		ktime = ktime_set(0, 5* NSEC_PER_MSEC);
    hrtimer_start(&i2c.timer_tpalt, ktime, HRTIMER_MODE_REL);
	}
	else if(flag == 0)
	{
		pr_info("F@TOUCH %s flag = %d ",__func__,flag);
		hrtimer_cancel(&i2c.timer_tpalt);
    cancel_work_sync(&i2c.tpalt_work);
    flush_workqueue(i2c.alt_test_wq);
	}
	else
	{
		pr_info("F@TOUCH %s ",__func__);
	}
	return ;
}

static enum hrtimer_restart tpalt_timer_handle(struct hrtimer *hrtimer)
{
    ktime_t ktime;

    queue_work(i2c.alt_test_wq, &i2c.tpalt_work);
    ktime = ktime_set(0,5* NSEC_PER_MSEC);
    hrtimer_forward_now(&i2c.timer_tpalt,ktime);

    return HRTIMER_RESTART;
}

static void tpalt_input_work_fn(struct work_struct *work)
{
		unsigned char buf[64] = {0};
		unsigned int get_ic_firmware_version = 0;
		buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	//alt st read fw from i2c
	if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 4) < 0)
  {
		tp_st_count++;
		pr_info("%s tp st error count=%d",__func__, tp_st_count);
  }
  get_ic_firmware_version = (buf[0] << 16) | (buf[1] << 12) | (buf[2] << 8) | (buf[3]);
	pr_debug("F@TOUCH : %s firmware version = %.4X\n", __func__, get_ic_firmware_version);
  msleep(50);
	//alt st read fw from i2c
}
//win add for ALT test

#ifdef CONFIG_FB
//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00*{_20170927
static int ilitek_i2c_fb_notifier_enable(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct fb_event *evdata = data;
	//if (evdata && evdata->data) {
		if (event == FB_EVENT_BLANK)
		{
			transition = evdata->data;
			if (*transition == FB_BLANK_POWERDOWN)
			{
				pr_debug("F@TOUCH %s FB_BLANK_POWERDOWN\n",__func__);
				//ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
				//rmi4_data->fb_ready = false;
				//i2c.suspend_flag = 1;
				gLcmOnOff = 0;
			}
			else if (*transition == FB_BLANK_UNBLANK)
			{
				pr_debug("F@TOUCH %s FB_BLANK_UNBLANK\n",__func__);
				//ilitek_i2c_resume(i2c.client);
				//i2c.suspend_flag = 0;
				gLcmOnOff = 1;

			}
		}
	//}

	return 0;
}
//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00*}_20170927
#endif

/*Win add for proc callback function*/

/*
description
  when adapter detects the i2c device, this function will be invoked.
parameters
  client
    i2c client data
  id
    i2c data
return
  status
*/
static int ilitek_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  //register i2c device
  int ret = 0;
	struct netlink_kernel_cfg cfg =
	{
		.groups = 0,
		.input	= udp_receive,
	};

  //allocate character device driver buffer
  ret = alloc_chrdev_region(&dev.devno, 0, 1, ILITEK_FILE_DRIVER_NAME);
  if(ret)
  {
    printk(ILITEK_ERROR_LEVEL "%s, can't allocate chrdev\n", __func__);
    return ret;
  }
  printk(ILITEK_DEBUG_LEVEL "%s, register chrdev(%d, %d)\n", __func__, MAJOR(dev.devno), MINOR(dev.devno));

  //initialize character device driver
  cdev_init(&dev.cdev, &ilitek_fops);
  dev.cdev.owner = THIS_MODULE;
  ret = cdev_add(&dev.cdev, dev.devno, 1);
  if(ret < 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, add character device error, ret %d\n", __func__, ret);
    return ret;
  }
  dev.class = class_create(THIS_MODULE, ILITEK_FILE_DRIVER_NAME);
  if(IS_ERR(dev.class))
  {
    printk(ILITEK_ERROR_LEVEL "%s, create class, error\n", __func__);
    return ret;
  }
  device_create(dev.class, NULL, dev.devno, NULL, "ilitek_ctrl");

  proc_create("ilitek_ctrl", 0666, NULL, &ilitek_fops);

	netlink_sock = netlink_kernel_create(&init_net, 21, &cfg);
  Report_Flag = 0;

  if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
  {
    printk(ILITEK_ERROR_LEVEL "%s, I2C_FUNC_I2C not support\n", __func__);
    return -1;
  }
  i2c.client = client;
  #ifdef CONFIG_FB
	//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00*{_20170927
    ilitek_notifier.notifier_call = ilitek_i2c_fb_notifier_enable;
 	ret = fb_register_client(&ilitek_notifier);
	if (ret < 0) {
		printk("%s: Failed to register fb notifier client\n", __func__);
	}
	//SW4-HL-Touch-DoubleTapOptionAllowedModifyOnlyWhenLcmIsOn-00*}_20170927
  #endif
	#if PLATFORM_FUN == PLATFORM_TINY210
	ret=gpio_request(TP_RESET(x), "RET");
	gpio_direction_output(TP_RESET(x), 1);
  #endif
#ifdef CONFIG_OF
  ret = parse_dt(&client->dev);
  client->irq = gpio_to_irq(i2c.irq_gpio);
  i2c.dev = &i2c.client->dev;
  printk(ILITEK_DEBUG_LEVEL "%s, i2c new style format\n", __func__);
  printk("%s, IRQ: 0x%X, gpio=%d\n", __func__, client->irq, i2c.irq_gpio);
	#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
  pr_info("F@TOUCH fts_pinctrl_init start \n");
  ret = fts_pinctrl_init();
  if (!ret && i2c.ts_pinctrl) {
    /*
     * Pinctrl handle is optional. If pinctrl handle is found
     * let pins to be configured in active state. If not
     * found continue further without error.
     */
    ret = pinctrl_select_state(i2c.ts_pinctrl, i2c.pinctrl_state_active);
    if (ret < 0) {
      dev_err(i2c.dev, "%s: Failed to select %s pinstate %d\n",__func__, PINCTRL_STATE_ACTIVE, ret);
    }
    pr_info("F@TOUCH fts_pinctrl_init end \n");
  }
  pr_err("F@TOUCH check gpio \n");
  ret = fts_set_gpio();
  if (ret < 0) {
    pr_err("FTS ERROR: %s: Failed to set up GPIO's\n",__func__);
    pinctrl_select_state(i2c.ts_pinctrl, i2c.pinctrl_state_release);
    return -1;
  }
#endif
#endif
	i2c.irq_status = 0;
	ret = ilitek_i2c_register_device();
  if(ret < 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, register i2c device, error\n", __func__);
    //Win add for BBox info
    printk("BBox;%s: Probe fail!\n", __func__);
    printk("BBox::UEC;7::0\n");
    //Win add for BBox info
    return ret;
  }
	#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
  /*Win add for proc callback function*/
	tp_probe_success = 1;
	touch_cb.touch_vendor_read = touch_vendor_read_ilitek;
	touch_cb.touch_tpfwimver_read = touch_fwimver_read;
	touch_cb.touch_tpfwver_read = touch_fwver_read;
	touch_cb.touch_fwupgrade_read = touch_upgrade_read;
	touch_cb.touch_fwupgrade = touch_upgrade_write;
	touch_cb.touch_selftest = touch_selftest_write;
	touch_cb.touch_selftest_result = selftest_result_read;
	touch_cb.touch_double_tap_read = touch_double_tap_read_ilitek;		//SW4-HL-Touch-ImplementDoubleTap-00+_20170623
	touch_cb.touch_double_tap_write = touch_double_tap_write_ilitek;	//SW4-HL-Touch-ImplementDoubleTap-00+_20170623
	//win add for ALT test
	touch_cb.touch_alt_rst = touch_alt_reset;
	touch_cb.touch_alt_st_count = touch_alt_read_count;
	touch_cb.touch_alt_st_enable = touch_alt_enable;
	i2c.alt_test_wq = create_freezable_workqueue("tp_alt_work");
	hrtimer_init(&i2c.timer_tpalt, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	i2c.timer_tpalt.function = tpalt_timer_handle;
	INIT_WORK(&i2c.tpalt_work, tpalt_input_work_fn);
	//win add for ALT test
	/*Win add for proc callback function*/
	#endif
	#ifdef UPGRADE_FIRMWARE_ON_BOOT
	#if UPGRAGE_FIRMWARE_MODE == UPGRADE_THREAD
	i2c.upgrade_thread = kthread_create(ilitek_i2c_upgrade_thread, NULL, "ilitek_i2c_upgrade");
	if(i2c.upgrade_thread == (struct task_struct*)ERR_PTR)
	{
		i2c.upgrade_thread = NULL;
		printk(ILITEK_ERROR_LEVEL "%s, kthread create, error\n", __func__);
	}
    else
    {
			wake_up_process(i2c.upgrade_thread);
    }
	#endif
	#if UPGRAGE_FIRMWARE_MODE == UPGRADE_BUILDIN
	touch_upgrade_write(1);
	#endif
	#endif
  return 0;
}

/*
description
  when the i2c device want to detach from adapter, this function will be invoked.
parameters
  client
    i2c client data
return
  status
*/
static int ilitek_i2c_remove(struct i2c_client *client)
{
  printk( "%s\n", __func__);
  i2c.stop_polling = 1;
#ifdef CONFIG_HAS_EARLYSUSPEND
  unregister_early_suspend(&i2c.early_suspend);
#endif
  //delete i2c driver
  if(i2c.client->irq != 0)
  {
    if(i2c.valid_irq_request != 0)
    {
      free_irq(i2c.client->irq, &i2c);
      printk(ILITEK_DEBUG_LEVEL "%s, free irq\n", __func__);
      if(i2c.irq_work_queue)
      {
        destroy_workqueue(i2c.irq_work_queue);
        printk(ILITEK_DEBUG_LEVEL "%s, destory work queue\n", __func__);
      }
    }
  }
  else
  {
    if(i2c.thread != NULL)
    {
      kthread_stop(i2c.thread);
      printk(ILITEK_DEBUG_LEVEL "%s, stop i2c thread\n", __func__);
    }
  }
  if(i2c.valid_input_register != 0)
  {
    input_unregister_device(i2c.input_dev);
    printk(ILITEK_DEBUG_LEVEL "%s, unregister i2c input device\n", __func__);
  }
#ifdef CONFIG_FB
	fb_unregister_client(&ilitek_notifier);
#endif
  //delete character device driver
  cdev_del(&dev.cdev);
  unregister_chrdev_region(dev.devno, 1);
  device_destroy(dev.class, dev.devno);
  class_destroy(dev.class);
  printk(ILITEK_DEBUG_LEVEL "%s\n", __func__);
  return 0;
}

/*
description
  write data to i2c device and read data from i2c device
parameter
  client
    i2c client data
  cmd
    data for write
  delay
    delay time(ms) after write
  data
    data for read
  length
    data length
return
  status
*/
int ilitek_i2c_write_read(struct i2c_client *client, uint8_t *cmd, int write_length, int delay, uint8_t *data, int read_length)
{
  int ret, i;
    struct i2c_msg msgs_send[] = {
		{.addr = client->addr, .flags = 0, .len = write_length, .buf = cmd,},
    };
  struct i2c_msg msgs_receive[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_length, .buf = data,}
    };
  ret = ilitek_i2c_transfer(client, msgs_send, 1);
  if(ret < 0 && i2c_retry)
  {
    printk(ILITEK_ERROR_LEVEL "%s, i2c write error, ret = %d\n", __func__, ret);
	for(i = 0; i < write_length; i++)
		printk("cmd[%d]=0x%2X\n", i, cmd[i]);
    //Win add for BBox info
    printk("BBox;%s: i2c write error!\n", __func__);
    printk("BBox::UEC;7::2\n");
    //Win add for BBox info
    return ret;
  }
  if(delay != 0)
		mdelay(delay);
	if(read_length ==0)
		return ret;
    ret = ilitek_i2c_transfer(client, msgs_receive, 1);
  if(ret < 0)
  {
    printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret = %d\n", __func__, ret);
    //Win add for BBox info
    printk("BBox;%s: i2c read error!\n", __func__);
    printk("BBox::UEC;7::1\n");
    //Win add for BBox info
    return ret;
  }
  return ret;
}

/*
description
  read touch information
parameters
  none
return
  status
*/
static int ilitek_i2c_read_tp_info(void)
{
  int i;
  unsigned char buf[64] = {0};
#ifdef TRANSFER_LIMIT
  int j;
#endif
	/*if(i2c.irq_status == 1)
	{
		disable_irq_nosync(i2c.client->irq);
		DBG("disable nosync\n");
		i2c.irq_status = 0;
	}*/
	ilitek_i2c_irq_disable();
  //read driver version
  printk(ILITEK_DEBUG_LEVEL "%s, driver version:%d.%d.%d\n", __func__, driver_information[0], driver_information[1], driver_information[2]);

  //read firmware version
  buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
  if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 4) < 0)
  {
    return -1;
  }
#ifdef UPGRADE_FIRMWARE_ON_BOOT
  for(i = 0; i < 4; i++)
  {
    i2c.firmware_ver[i] = buf[i];
  }
#endif
  printk(ILITEK_DEBUG_LEVEL "%s, firmware version:%d.%d.%d.%d\n", __func__, buf[0], buf[1], buf[2], buf[3]);

  //Win add for touch firmware version
  snprintf(fih_touch_ilitek, PAGE_SIZE, "ilitek-V%d%d%d%02d\n", buf[0], buf[1], buf[2], buf[3]);
	#if PLATFORM_FUN == PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
	fih_info_set_touch(fih_touch_ilitek);
	//Win add for touch firmware version
	#endif
  //read protocol version
  buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
  if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 2) < 0)
  {
    return -1;
  }
  i2c.protocol_ver = (((int)buf[0]) << 8) + buf[1];
  printk(ILITEK_DEBUG_LEVEL "%s, protocol version:%d.%d\n", __func__, buf[0], buf[1]);

    //read touch resolution
  i2c.max_tp = SUPPORT_MAXPOINT;
  buf[0] = ILITEK_TP_CMD_GET_RESOLUTION;
#ifdef TRANSFER_LIMIT
  if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 8) < 0)
  {
    return -1;
  }
  if(ilitek_i2c_read(i2c.client, buf+8, 2) < 0)
  {
    return -1;
  }
#else
  if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 10) < 0)
  {
    return -1;
  }
#endif

  //calculate the resolution for x and y direction
  //i2c.max_x = buf[0];
  //i2c.max_x+= ((int)buf[1]) * 256;
  //i2c.max_y = buf[2];
  //i2c.max_y+= ((int)buf[3]) * 256;
  i2c.max_x = ROSILUTION_X;
  i2c.max_y = ROSILUTION_Y;
  i2c.x_ch = buf[4];
  i2c.y_ch = buf[5];
  //maximum touch point
  //i2c.max_tp = buf[6];
  //maximum button number
  i2c.max_btn = buf[7];
  i2c.max_tp = SUPPORT_MAXPOINT;
  //key count
  i2c.keycount = buf[8];

  printk(ILITEK_DEBUG_LEVEL "%s, max_x: %d, max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, max_btn: %d, key_count: %d\n", __func__, i2c.max_x, i2c.max_y, i2c.x_ch, i2c.y_ch, i2c.max_tp, i2c.max_btn, i2c.keycount);

	if(i2c.protocol_ver == 0x300 || i2c.protocol_ver == 0x301)
  {
    //get key infotmation
	buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
  #ifdef TRANSFER_LIMIT
    if(ilitek_i2c_write_read(i2c.client, buf, 1, 0, buf, 8) < 0)
    {
      return -1;
    }
    for(i = 1, j = 1; j < i2c.keycount ; i++)
    {
      if(i2c.keycount > j)
      {
        if(ilitek_i2c_read(i2c.client, buf+i*8, 8) < 0)
        {
          return -1;
        }
        j = (4 + 8 * i) / 5;
      }
    }
    for(j = 29; j < (i + 1) * 8; j++)
    {
      buf[j] = buf[j + 3];
    }
  #else
    if(i2c.keycount)
    {
      if(ilitek_i2c_write_read(i2c.client, buf, 1, 0, buf, 29) < 0)
      {
        return -1;
      }
      if(i2c.keycount > 5)
      {
        if(ilitek_i2c_read(i2c.client, buf+29, 25) < 0)
        {
          return -1;
        }
      }
    }
  #endif
    i2c.key_xlen = (buf[0] << 8) + buf[1];
    i2c.key_ylen = (buf[2] << 8) + buf[3];
    printk(ILITEK_DEBUG_LEVEL "%s, key_xlen: %d, key_ylen: %d\n", __func__, i2c.key_xlen, i2c.key_ylen);

    //print key information
    for(i = 0; i < i2c.keycount; i++)
    {
      i2c.keyinfo[i].id = buf[i*5+4];
      i2c.keyinfo[i].x = (buf[i*5+5] << 8) + buf[i*5+6];
      i2c.keyinfo[i].y = (buf[i*5+7] << 8) + buf[i*5+8];
      i2c.keyinfo[i].status = 0;
      printk(ILITEK_DEBUG_LEVEL "%s, key_id: %d, key_x: %d, key_y: %d, key_status: %d\n", __func__, i2c.keyinfo[i].id, i2c.keyinfo[i].x, i2c.keyinfo[i].y, i2c.keyinfo[i].status);
    }
  }
  ilitek_i2c_irq_enable();
  return 0;
}


/*
description
	register i2c device and its input device
parameters
	none
return
  status
*/
static int ilitek_i2c_register_device(void)
{
	int ret = 0;
	#ifdef UPGRADE_FIRMWARE_ON_BOOT
	//int i = 0;
	#endif
	printk(ILITEK_DEBUG_LEVEL "%s, client.addr: 0x%X\n", __func__, (unsigned int)i2c.client->addr);
	if((i2c.client->addr == 0) || (i2c.client->adapter == 0))// || (i2c.client->driver == 0))
  {
		printk(ILITEK_ERROR_LEVEL "%s, invalid register\n", __func__);
		return ret;
  }
	if(ret)
	{
		printk(ILITEK_ERROR_LEVEL "%s, register input device, error\n", __func__);
		return ret;
	}
	if(i2c.client->irq != 0)
  {
		i2c.irq_work_queue = create_singlethread_workqueue("ilitek_i2c_irq_queue");
		if(i2c.irq_work_queue)
  {
			INIT_WORK(&i2c.irq_work, ilitek_i2c_irq_work_queue_func);
		#ifdef CLOCK_INTERRUPT
			if(request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING , "ilitek_i2c_irq", &i2c))
  {
				printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
    }
			else
    {
				i2c.valid_irq_request = 1;
				i2c.irq_status = 1;
				printk(ILITEK_ERROR_LEVEL "%s, request irq(Trigger Falling), success\n", __func__);
    }
		#else
			init_timer(&i2c.timer);
			i2c.timer.data = (unsigned long)&i2c;
			//i2c.timer.function = ilitek_i2c_timer;
			if(request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_LOW, "ilitek_i2c_irq", &i2c))
      {
				printk(ILITEK_ERROR_LEVEL "%s, request irq, error\n", __func__);
    }
    else
    {
				i2c.valid_irq_request = 1;
				i2c.irq_status = 1;
				printk(ILITEK_ERROR_LEVEL "%s, request irq(Trigger Low), success\n", __func__);
    }
		#endif
    }
  }
  else
  {
		i2c.stop_polling = 0;
		i2c.thread = kthread_create(ilitek_i2c_polling_thread, NULL, "ilitek_i2c_thread");
		printk(ILITEK_ERROR_LEVEL "%s, polling mode\n", __func__);
		if(i2c.thread == (struct task_struct*)ERR_PTR)
      {
			i2c.thread = NULL;
			printk(ILITEK_ERROR_LEVEL "%s, kthread create, error\n", __func__);
    }
    else
    {
			wake_up_process(i2c.thread);
    }
}
  //register input device
	i2c.input_dev = input_allocate_device();
	if(i2c.input_dev == NULL)
	{
		printk(ILITEK_ERROR_LEVEL "%s, allocate input device, error\n", __func__);
		return -1;
	}
	ret = ilitek_i2c_read_tp_info();
	if(ret)
		return ret;
	ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_x, i2c.max_y);
	ret = input_register_device(i2c.input_dev);

  printk(ILITEK_ERROR_LEVEL "%s, register input device, success\n", __func__);
  i2c.valid_input_register = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
  i2c.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
  i2c.early_suspend.suspend = ilitek_i2c_early_suspend;
  i2c.early_suspend.resume = ilitek_i2c_late_resume;
  register_early_suspend(&i2c.early_suspend);
#endif
	ilitek_i2c_reset();
	//read touch parameter

  return 0;
}

/*
description
  initiali function for driver to invoke.
parameters
  none
return
  status
*/
static int __init ilitek_init(void)
{
  int ret = 0;
  //initialize global variable
  memset(&dev, 0, sizeof(struct dev_data));
  memset(&i2c, 0, sizeof(struct i2c_data));

  //initialize mutex object
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
  init_MUTEX(&i2c.wr_sem);
#else
  sema_init(&i2c.wr_sem, 1);
#endif
  i2c.wr_sem.count = 1;
  i2c.report_status = 1;
  ret = i2c_add_driver(&ilitek_i2c_driver);
  if(ret == 0)
  {
    i2c.valid_i2c_register = 1;
    printk(ILITEK_DEBUG_LEVEL "%s, add i2c device, success\n", __func__);
    if(i2c.client == NULL)
    {
      printk(ILITEK_ERROR_LEVEL "%s, no i2c board information\n", __func__);
    }
  }
  else
  {
    printk(ILITEK_ERROR_LEVEL "%s, add i2c device, error\n", __func__);
  }
  return ret;
}

/*
description
  driver exit function
parameters
  none
return
  nothing
*/
static void __exit ilitek_exit(void)
{
  printk("%s, enter\n", __func__);
  if(i2c.valid_i2c_register != 0)
  {
    printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver\n", __func__);
    i2c_del_driver(&ilitek_i2c_driver);
    printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver\n", __func__);
    }
  else
  {
    printk(ILITEK_DEBUG_LEVEL "%s, delete i2c driver Fail\n", __func__);
  }

  remove_proc_entry("ilitek_ctrl", NULL);
}

//set init and exit function for this module
module_init(ilitek_init);
module_exit(ilitek_exit);
