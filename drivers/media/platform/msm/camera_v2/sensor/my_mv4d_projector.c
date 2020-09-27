
//#define pr_fmt(fmt) DRIVER_NAME ":%s:%d: " fmt, __func__, __LINE__
//#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define pr_fmt(fmt) KBUILD_MODNAME ":%s:%d: " fmt, __func__, __LINE__

#ifndef DEBUG
#define DEBUG
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>

#include <my_mv4d_projector_ioctl.h>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//TODO : CHANGE THIS!!!

#define MV4D_ADAPTER_NUMBER 3               //i2c-number bus
#define MV4D_PROJECTOR_I2C_ADDRESS  0x50    //mv4d i2c address
//CHECK : platform_driver

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


#define DRIVER_NAME "mvprojector"

static struct i2c_client *g_client = 0;

static struct mutex      mv4d_projector_mutex;

static int g_device_Open = 0; /* Is device open?
 * Used to prevent multiple access to device */

///////////////////////////////////////////////////////////////////////////
///                         Declaration                                 ///
///////////////////////////////////////////////////////////////////////////

static long mv4d_projector_ioctl(struct file *file,
        unsigned int cmd, unsigned long arg);

static int mv4d_projector_ioctl_handler(struct file *file,
        unsigned int cmd, unsigned long arg, void __user *p);

static int mv4d_projector_open(struct inode *inode, struct file *file);

static ssize_t mv4d_projector_read(struct file *filp, char* buffer, /* buffer to fill with data */
        size_t length, /* length of the buffer     */
        loff_t * offset);

static int mv4d_projector_release(struct inode *inode, struct file *file);

static int mv4d_projector_i2c_read(struct i2c_client *i2c, uint8_t cmd, uint8_t* data);

///////////////////////////////////////////////////////////////////////////
///                         Misc Char Device                            ///
///////////////////////////////////////////////////////////////////////////


static const struct file_operations mv4d_projector_fops = {
        .owner =            THIS_MODULE,
        .open =                mv4d_projector_open,
        .read =             mv4d_projector_read,
        .unlocked_ioctl =    mv4d_projector_ioctl,
        //.flush =             mv4d_projector_flush,
        .release =             mv4d_projector_release,
};

static struct miscdevice mv4d_projector_device = {
        .minor =    MISC_DYNAMIC_MINOR,
        .name =        DRIVER_NAME,
        .fops =        &mv4d_projector_fops
};

struct mvprojector_data
{
    struct i2c_client *i2c_client;
};

///////////////////////////////////////////////////////////////////////////
///                             Misc Driver                             ///
///////////////////////////////////////////////////////////////////////////

/*
 * Called when a process tries to open the device file
 */
static int mv4d_projector_open(struct inode *inode, struct file *file)
{
    int rc = 0;
    pr_devel("Started\n");
    if (g_device_Open)
    {
        pr_err("Error\n");
        return -EBUSY;
    }

    rc = nonseekable_open(inode, file);
    if (rc)
        return rc;

    g_device_Open++;
    //file->private_data = &mvprojector;

    try_module_get(THIS_MODULE); //not needed.
    pr_devel("Finished\n");
    return 0;
}

static ssize_t mv4d_projector_read(struct file *filp, char* buffer, /* buffer to fill with data */
        size_t length, /* length of the buffer     */
        loff_t * offset)
{
    int rc = 0;
    uint8_t rdata[10];
    pr_devel("Started\n");
    rc = mv4d_projector_i2c_read(g_client, 0x00, rdata);
    pr_devel("read fw version status3:0 chip version : %d", rc);
    pr_devel("Finished\n");
    return rc;
}

/*
 * Called when a process closes the device file.
 */
static int mv4d_projector_release(struct inode *inode, struct file *file)
{
    pr_devel("Started\n");
    g_device_Open--;  /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);  //not needed.
    pr_devel("Finished\n");
    return 0;
}
#if 0
static unsigned char mvchecksum(unsigned char cId, unsigned char cCommand, unsigned char *pParams, int nParams)
{
    int i, sum;
    sum = (int)cId + (int)cCommand;
    for (i=0; i<nParams; ++i)
        sum += (int)pParams[i];
    sum = sum%256;
    return (unsigned char)sum;
}
#endif
static int mv4d_projector_i2c_read(struct i2c_client *i2c, uint8_t cmd, uint8_t* data)
{
    int ret;
    uint8_t i2c_buf[2];
    uint8_t rx_buf[1];
    //uint8_t checksum;

    struct i2c_msg msgs[] = {
        {
            .addr = i2c->addr,
            .flags = 0,
            .len = 1,
            .buf = i2c_buf,
        },
        {
            .addr = i2c->addr,
            .flags = I2C_M_RD | I2C_M_NO_RD_ACK | I2C_M_STOP,
            .len = 1,
            .buf = rx_buf,
        },
    };

    i2c_buf[0] = cmd;               // command
    //i2c_buf[1] = i2c->addr + cmd;   // checksum
    //i2c_buf[1] = mvchecksum(0x50, cmd, NULL, 0);

    dev_info(&i2c->dev, "%s cmd:0x%02X.\n", __func__, cmd);

    ret = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
    if (ret < 0)
    {
        dev_err(&i2c->dev, "%s: transfer failed.\n", __func__);
        return ret;
    }
    else if (ret != ARRAY_SIZE(msgs)) {
        dev_err(&i2c->dev, "%s: transfer failed(size error).\n",
                __func__);
        return -ENXIO;
    }
#if 0
    //checksum = cmd + rx_buf[0] + rx_buf[1];
    if (mvchecksum(0x50, 0, rx_buf, 2) != rx_buf[2])
    {
        // checksum error
        dev_err(&i2c->dev, "%s: checksum error [0x%02X 0x%02X 0x%02X].\n",
                __func__, rx_buf[0], rx_buf[1], rx_buf[2]);
        return -EIO;
    }
#endif
    data[0] = rx_buf[0];
//    data[1] = rx_buf[1];
//    data[2] = rx_buf[2];
//    dev_info(&i2c->dev, "%s read cmd:0x%02X.i2cbuf_0=0x%x\n", __func__, cmd,i2c_buf[0] );
    dev_info(&i2c->dev, "%s read cmd:0x%02X.i2c_buf0=0x%x,\n", __func__, cmd,data[0]);

    return 0;
}

static int mv4d_projector_i2c_write(struct i2c_client *i2c, uint8_t cmd, uint8_t* data)
{
    int ret;
    uint8_t i2c_buf[2] = {0};

    struct i2c_msg msg[] = {
        {
            .addr = i2c->addr,
            .flags = 0,
            .len = 2,
            .buf = i2c_buf,
        },
    };

    i2c_buf[0] = cmd;               // command
    //i2c_buf[1] = i2c->addr + cmd + data[0] + data[1] + data[2];   // checksum
    //i2c_buf[1] = mvchecksum(0x50, cmd, data, 3);
    i2c_buf[1] = data[0];
//    i2c_buf[2] = data[1];
//    i2c_buf[3] = data[2];

    dev_info(&i2c->dev, "%s cmd:0x%02X.\n", __func__, cmd);
//    dev_info(&i2c->dev, "%s cmd:0x%02X.i2c_buf1=0x%x,i2c_buf2=0x%x,i2c_buf3=0x%x,i2cbuf_0=0x%x\n", __func__, cmd,i2c_buf[1],i2c_buf[2],i2c_buf[3],i2c_buf[0] );
dev_info(&i2c->dev, "%s cmd:0x%02X. i2cbuf_1=0x%x\n", __func__, cmd,i2c_buf[1]);


//  ret= i2c_smbus_write_byte_data(g_client,0x17,1);
    ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: transfer failed.", __func__);
        return ret;
    } else if (ret != ARRAY_SIZE(msg)) {
        dev_err(&i2c->dev, "%s: transfer failed(size error).",
                __func__);
        return -ENXIO;
    }
 // ret= i2c_smbus_write_byte_data(g_client,0x17,1);
    return 0;
}

static long mv4d_projector_ioctl(struct file *file,
        unsigned int cmd, unsigned long arg)
{
    int ret;
    void __user *argp = (void __user *) arg;
    pr_devel("Started\n");
    mutex_lock(&mv4d_projector_mutex);
    if (!argp)
    {
        pr_err("ERROR, argp is null");
        return -EINVAL;
    }
    else
    {
        ret = mv4d_projector_ioctl_handler(file, cmd, arg, argp);
        mutex_unlock(&mv4d_projector_mutex);
    }
    pr_devel("Finished\n");
    return ret;
}

/*
 * misc device file operation functions
 */
static int mv4d_projector_ioctl_handler(struct file *file,
        unsigned int cmd, unsigned long arg, void __user *argp)
{
    long rc = 0;

    // uint16 fw_version;
    uint8_t data[3] = {0};
    //struct mv4d_projector_data *mv4d = file->private_data;
    //struct i2c_client *i2c = mv4d->i2c;
    //struct i2c_client *i2c = NULL;


    pr_devel("Started\n");

    pr_devel("ioc value : %d\n", _IOC_DIR(cmd));
    if( _IOC_DIR(cmd) & _IOC_READ)
    {
        pr_devel("command type : _IOC_READ\n");

        rc = mv4d_projector_i2c_read(g_client, _IOC_NR(cmd), data);
        if (rc < 0)
            return rc;
        if (copy_to_user(argp, data, sizeof(data)))
        {
            pr_err("Error\n");
            //dev_err(&i2c->dev, "copy_to_user failed.");
            return -EFAULT;
        }
    }
    else if( _IOC_DIR(cmd) & _IOC_WRITE)
    {
        pr_devel("command type : _IOC_WRITE \n");
        if (argp == NULL)
        {
            pr_err("Error argp is null\n");
            //dev_err(&i2c->dev, "invalid argument.");
            return -EINVAL;
        }
        if (copy_from_user(data, argp, sizeof(data)))
        {
            pr_err("Error\n");
            //dev_err(&i2c->dev, "copy_from_user failed.");
            return -EFAULT;
        }
        rc = mv4d_projector_i2c_write(g_client, _IOC_NR(cmd), data);
        if (rc < 0)
            return rc;
    }
    else
    {
        pr_devel("command type unknown\n");
    }

    pr_devel("Finished\n");
    return rc;
}

///////////////////////////////////////////////////////////////////////////
///                             I2C                                     ///
///////////////////////////////////////////////////////////////////////////

#define  MVPROJECTOR_ID  0x50

static const struct i2c_device_id mv4d_projector_i2c_id[] = {
        {DRIVER_NAME, 0},
        {},
};

MODULE_DEVICE_TABLE(i2c, mv4d_projector_i2c_id);

static int mv4d_projector_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct mvprojector_data *mvdata;
    pr_devel("Started\n");
    pr_err("mv4d_projector_i2c_probe in\n");

    g_client = client;

    /* Check if the adapter supports the needed features */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        return -EIO;

    //mvprojector_data
    mvdata = devm_kzalloc(&client->dev, sizeof(struct mvprojector_data), GFP_KERNEL);

    if (!mvdata)
    {
        dev_err(&client->dev, "Failed to allocate memory for logical device\n");
        return -ENOMEM;
    }
    mvdata->i2c_client = client;
    i2c_set_clientdata(client, mvdata);

    pr_devel("Finished\n");
    return 0;
}

static int mv4d_projector_i2c_remove(struct i2c_client *client)
{
    pr_devel("Started\n");
    //struct v4l2_subdev *sd = i2c_get_clientdata(client);

    //TODO : free mvdata!!!
    return 0;
}

#ifdef CONFIG_PM
//static int mv4d_projector_suspend(struct i2c_client *client, pm_message_t pm)
//{
//    dev_warn(&client->dev, "%s called\n", __func__);
//    //mv4d_projector_power(0);
//    return 0;
//}

//static int mv4d_projector_resume(struct i2c_client *client)
//{
//    dev_warn(&client->dev, "%s called\n", __func__);
//    //mv4d_projector_power(1);
//    return 0;
//}
#else
#define mv4d_projector_suspend NULL
#define mv4d_projector_resume NULL
#endif


static const struct of_device_id mv4d_projector_dt_ids[] = {
    { .compatible = "mantis-vision,mvprojector", },
    { }
};

MODULE_DEVICE_TABLE(of, mv4d_projector_dt_ids);


//TODO: 1. Check the relation between this and "mv4d_projector_device" the misc driver.
//      2. They need different names?

static struct i2c_driver mv4d_projector_driver = {
    .id_table = mv4d_projector_i2c_id,
    .probe  = mv4d_projector_i2c_probe,
    .remove = mv4d_projector_i2c_remove,
//    .suspend    = mv4d_projector_suspend,
//    .resume        = mv4d_projector_resume,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = mv4d_projector_dt_ids,
    },
};

///////////////////////////////////////////////////////////////////////////
///                             INIT/EXIT                               ///
///////////////////////////////////////////////////////////////////////////

static int __init mv4d_projector_module_init(void)
{
    int ret;//, i;
    //int values[4];
    pr_err("mv4d_projector_module_init in\n");

    /*XX
    struct i2c_adapter *adapter;

    const struct i2c_board_info info = {
            .type = DRIVER_NAME,
            .addr = MV4D_PROJECTOR_I2C_ADDRESS,
    };
    //*/

//    for(i = 0; i < 2; ++i)
//        gpio_set_value(45+i, 0);
//
//    for(i = 0; i < 4; ++i)
//        values[i] = gpio_get_value(45+i);
//    pr_devel("gpios values : %d | %d | %d | %d", values[0], values[1], values[2], values[3]);


    pr_devel("Started\n");
    //pr_devel("Skip on I2C init...\n");
    //CANCEL THIS THEN I2C WORK

    /*XX
    //int i2c_register_board_info(int busnum, struct i2c_board_info *info, unsigned len);
    i2c_register_board_info(MV4D_ADAPTER_NUMBER, &info, sizeof(info));
    //*/

    ret = i2c_add_driver(&mv4d_projector_driver);
    if(ret)
    {
        pr_err("Error\n");
        return(ret);
    }
    pr_devel("i2c_add_driver pass...\n");
    //return ret;

    //pr_devel("Skip on I2C init...\n");
    //goto init_finished;

    /*XX
    adapter = i2c_get_adapter(MV4D_ADAPTER_NUMBER); //// means i2c-number bus
    if (!adapter)
    {
        pr_err("Error\n");
        return -EINVAL;
    }
    pr_devel("i2c_get_adapter pass...\n");

    client = i2c_new_device(adapter, &info);
    if (!client)
    {
        pr_err("Error\n");
        return -EINVAL;
    }
    pr_devel("i2c_new_device pass...\n");
    //*/
    //init_finished:

    mutex_init(&mv4d_projector_mutex);
    //TODO : move this?
    //to register as a misc device
    ret = misc_register(&mv4d_projector_device);
    if (ret){
        pr_err("Error\n");
        //goto exit_kfree_client;
        return ret;
    }
    pr_devel("Registered as misc device\n");

    pr_devel("Finished\n");
    return (0);
}

static void __exit mv4d_projector_module_exit(void)
{
    pr_devel("Started\n");
    misc_deregister(&mv4d_projector_device);
    ///////////////////////////////////////////////
    //OPEN THIS THEN I2C WORK
//    i2c_unregister_device(client);
    i2c_del_driver(&mv4d_projector_driver);
    ///////////////////////////////////////////////
    pr_devel("Finished\n");
}


module_init(mv4d_projector_module_init);
module_exit(mv4d_projector_module_exit);


MODULE_AUTHOR("Yehuda Sar Shalom");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mantis Vision Projector");
