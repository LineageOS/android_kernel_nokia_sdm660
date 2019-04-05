/*
 * Driver for TCA8418 I2C keyboard
 *
 * Copyright (C) 2011 Fuel7, Inc.  All rights reserved.
 *
 * Author: Kyle Manna <kyle.manna@fuel7.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 * If you can't comply with GPLv2, alternative licensing terms may be
 * arranged. Please contact Fuel7, Inc. (http://fuel7.com/) for proprietary
 * alternative licensing inquiries.
 */
#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
//#include <linux/input/tca6416_keypad.h>
#include <linux/of.h>


#include <linux/platform_device.h>

#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

static int major;
static struct class *class;
static struct i2c_client *tca6416_client;
//unsigned int reg_buf_gpio[]={0x02FE,0x02FD,0x02FC,0x02FB,0x02FA,0x02F9,0x02F8,0x02F7,
//                                  0x03FE,0x03FD,0x03FC,0x03FB,0x03FA,0x03F9,0x03F8,0x03F7};//config which gpio

unsigned char reg_buf_gpio_out[]={0xFE,0xFD,0xFB,0xF7,0xEF,0xDF,0xBF,0x7F,0x00,0x00,0xFE,0xFD,0xFB,0xF7,0xEF,0xDF,0xBF,0x7F};//  06 07 Configuration registers input/output 0 is output
unsigned char reg_buf_config_high[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};//02 03 output logicl config low or high  1 is high
unsigned char reg_buf_config_low[]={0xFE,0xFD,0xFB,0xF7,0xEF,0xDF,0xBF,0x7F,0x00,0x00,0xFE,0xFD,0xFB,0xF7,0xEF,0xDF,0xBF,0x7F};//02 03 config low or high
unsigned char data =0;



struct tca6416_chip {
    struct gpio_chip                 gpio_chip;
    struct i2c_client               *client;
    u32              irq_masked;
    u32              dev_sense;
    u32              dev_masked;
    struct mutex                     lock;
};


/* 传入: buf[0] : addr
 * 输出: buf[0] : data
 */
void tca6416_write_ker(int gpio_num,int val,int delay_time)
{
  printk("enter tca6416_write  gpio_num=%d val=%d reg_buf_gpio[]=0x%x delay=%d\n",gpio_num,val,reg_buf_gpio_out[gpio_num],delay_time);
  if(tca6416_client != NULL)
  {
    printk("enter tca6416_write_ker tca6416_client=%p addr=0x%x\n",tca6416_client,tca6416_client->addr);
   #if 1
   //read need register 00 01 ,
   if((0<=gpio_num )&&(gpio_num<=7))//  1= gpio 1 2 = gpio 2...
    {
//06 07 output/input first
        data=i2c_smbus_read_byte_data(tca6416_client, 0x06);
        printk("wbl tca6416_read data 0-7 out/in before=0x%02x  writer data=0x%x  delay=%d\n",data,reg_buf_gpio_out[gpio_num]&data,delay_time);
        i2c_smbus_write_byte_data(tca6416_client,0x06,reg_buf_gpio_out[gpio_num]&data);
        data=i2c_smbus_read_byte_data(tca6416_client, 0x06);
        printk("wbl tca6416_read data 0-7 out/in after =0x%02x  writer data=0x%x\n",data,reg_buf_gpio_out[gpio_num]&data);
       if(val ==2)
       {//02 ,03 high/low need read 00 01
          data=i2c_smbus_read_byte_data(tca6416_client, 0x00);
          printk("wbl tca6416_read data0-7 high before=0x%02x  writer data=0x%x\n",data,reg_buf_config_high[gpio_num]|data);
          i2c_smbus_write_byte_data(tca6416_client,0x02,reg_buf_config_high[gpio_num]|data);
          mdelay(delay_time);
          data=i2c_smbus_read_byte_data(tca6416_client, 0x00);
          printk("wbl tca6416_read data 0-7 high after=0x%02x  writer data=0x%x\n",data,reg_buf_config_high[gpio_num]|data);
       }
        else if(val == 0)
        {
          data=i2c_smbus_read_byte_data(tca6416_client, 0x00);
          printk("wbl tca6416_read data 0-7 low before=0x%02x    writer data=0x%x\n",data,reg_buf_config_low[gpio_num]&data);
          i2c_smbus_write_byte_data(tca6416_client,0x02,reg_buf_config_low[gpio_num]&data);
          mdelay(delay_time);
          data=i2c_smbus_read_byte_data(tca6416_client, 0x00);
          printk("wbl tca6416_read data 0-7 low after=0x%02x    writer data=0x%x\n",data,reg_buf_config_low[gpio_num]&data);
        }
    }
   else if((10<=gpio_num)&&( gpio_num<=17))
   {
       //06 07 output/input first
       data=i2c_smbus_read_byte_data(tca6416_client, 0x07);
       printk("wbl tca6416_read data 10-17 out/in before =0x%02x  writer data=0x%x\n",data,reg_buf_gpio_out[gpio_num]&data);
       i2c_smbus_write_byte_data(tca6416_client,0x07,reg_buf_gpio_out[gpio_num]&data);
       //  mdelay(delay_time);
       data=i2c_smbus_read_byte_data(tca6416_client, 0x07);
       printk("wbl tca6416_read data 10-17 out/in after =0x%02x  writer data=0x%x\n",data,reg_buf_gpio_out[gpio_num]&data);
   if(val ==2)
   {
       data=i2c_smbus_read_byte_data(tca6416_client, 0x01);
       printk("wbl tca6416_read data10-17 high before=0x%02x  writer data=0x%x\n",data,reg_buf_config_high[gpio_num]|data);
       i2c_smbus_write_byte_data(tca6416_client,0x03,reg_buf_config_high[gpio_num]|data);
       mdelay(delay_time);
       data=i2c_smbus_read_byte_data(tca6416_client, 0x01);
       printk("wbl tca6416_read data10-17 high afer=0x%02x  writer data=0x%x\n",data,reg_buf_config_high[gpio_num]|data);
    }
    else if(val == 0)
    {
        data=i2c_smbus_read_byte_data(tca6416_client, 0x01);
        printk("wbl tca6416_read data 10-17 low before=0x%02x     writer data=0x%x\n",data,reg_buf_config_low[gpio_num]&data);
        i2c_smbus_write_byte_data(tca6416_client,0x03,reg_buf_config_low[gpio_num]&data);
        mdelay(delay_time);
        data=i2c_smbus_read_byte_data(tca6416_client, 0x01);
        printk("wbl tca6416_read data10-17 low afer=0x%02x  writer data=0x%x\n",data,reg_buf_config_low[gpio_num]&data);
    }
  }
  else
  {
        printk("err no compatibal gpio");
  }
  #endif
 }
 else
 {
  printk("wbl error!! failed tca6416_client=null");
 }
}
EXPORT_SYMBOL(tca6416_write_ker);

static ssize_t tca6416_read(struct file * file, char __user *buf, size_t count, loff_t *off)

{
    unsigned char addr;
    unsigned int r;
    if(tca6416_client == NULL)
    {
      printk("error tca6416_read tca6416_client==null return");
    }
    printk("tca6416_read\n");

   r= copy_from_user(&addr, buf, 1);
    if (r) {
        kfree(buf);
        return -EFAULT;
    }

   printk("tca6416_readaddr=0x%02x\n",addr);
  r= i2c_smbus_read_byte_data(tca6416_client, addr);
  if (r < 0)
      return r;
  printk("tca6416_read data=0x%02x\n",r);
    return 1;
}

/* buf[0] : addr
 * buf[1] : data
 */
 #if 1
static ssize_t tca6416_write(struct file * file,const char __user *buf, size_t count, loff_t *off)
{
#if 1
    unsigned   int gpio_num;
    unsigned    int data;
    unsigned int delay;

    unsigned int ker_buf[15][3];
    unsigned int r;
    unsigned  char i;
    //unsigned char addr, data;
   // printk("addr = 0x%02x, data = 0x%02x\n", addr, data);
    printk("tca6416_write1\n");

   r= copy_from_user(ker_buf, buf, sizeof(ker_buf));
    if (r) {
        kfree(buf);
        return -EFAULT;
    }
    gpio_num = ker_buf[0][0];
    data = ker_buf[0][1];
    delay =ker_buf[0][2];

    printk("addr = %d, data =%d, delay=%d\n", gpio_num, data,delay);
    for( i=0;i<15;i++)
    {
      if(ker_buf[i][0]||ker_buf[i][1]||ker_buf[i][2])
          {
              printk("gpio_num = %d, data =%d, delay=%d\n",  ker_buf[i][0],  ker_buf[i][0],ker_buf[i][2]);
          tca6416_write_ker( ker_buf[i][0],  ker_buf[i][0],ker_buf[i][2]);
          }
    }


  //  if (!i2c_smbus_write_byte_data(tca6416_client, addr, data))
  //      return 2;
 //   else
 //       return -EIO;
 #endif
    return 0;
}
#endif
static struct file_operations tca6416_fops = {
    .owner = THIS_MODULE,
    .read  = tca6416_read,
    .write = tca6416_write,
};

static int  tca6416_probe(struct i2c_client *client,
                  const struct i2c_device_id *id)
{
    static const u32 i2c_funcs = I2C_FUNC_SMBUS_BYTE_DATA |
                     I2C_FUNC_SMBUS_WRITE_WORD_DATA;
    struct tca6416_chip *chip;

    if (!i2c_check_functionality(client->adapter, i2c_funcs))
        return -ENOSYS;

    chip = devm_kzalloc(&client->dev,
        sizeof(struct tca6416_chip), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;

    i2c_set_clientdata(client, chip);

    tca6416_client = client;

    printk(" %s %d tca6416_client=%p\n", __FUNCTION__, __LINE__,tca6416_client);
    major = register_chrdev(0, "tca6416", &tca6416_fops);
    class = class_create(THIS_MODULE, "tca6416");
    device_create(class, NULL, MKDEV(major, 0), NULL, "tca6416"); /* /dev/tca6416 */

    return 0;
}

static int  tca6416_remove(struct i2c_client *client)
{
    //printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    device_destroy(class, MKDEV(major, 0));
    class_destroy(class);
    unregister_chrdev(major, "tca6416");

    return 0;
}

static struct of_device_id tca6416_match_table[] = {
    { .compatible = "ti,tca6416",},
    { },
};
static const struct i2c_device_id tca6416_id_table[] = {
    { "tca6416", 0 },
    {}
};


/* 1. 分配/设置i2c_driver */
static struct i2c_driver tca6416_driver = {
    .driver = {
        .name   = "tca6416",
        .owner  = THIS_MODULE,
        .of_match_table = tca6416_match_table,
    },
    .probe      = tca6416_probe,
    .remove     = tca6416_remove,
    .id_table   = tca6416_id_table,
};

static int tca6416_drv_init(void)
{
    /* 2. 注册i2c_driver */
   int rc =0;
    printk("init  %s %d\n", __FUNCTION__, __LINE__);
   rc = i2c_add_driver(&tca6416_driver);

    printk("init afer %s %d rc=%d\n", __FUNCTION__, __LINE__,rc);
    return rc;
}

static void tca6416_drv_exit(void)
{
    i2c_del_driver(&tca6416_driver);
}


module_init(tca6416_drv_init);
module_exit(tca6416_drv_exit);
MODULE_LICENSE("GPL");


