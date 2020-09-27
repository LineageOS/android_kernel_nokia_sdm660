#define ROCKCHIP     1

#define PLAT      ROCKCHIP

//#define ILI_UPDATE_FW

#define CLOCK_INTERRUPT
//#define POLLING_MODE

#define RK_I2C_FREQ     100000

//#define RESET_GPIO

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
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#endif
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/io.h>

//driver information
#define DERVER_VERSION_MAJOR 		3
#define DERVER_VERSION_MINOR 		6
#define CUSTOMER_ID 				0
#define MODULE_ID					1
#define PLATFORM_ID					0
#define PLATFORM_MODULE				0
#define ENGINEER_ID					0

#ifdef ILI_UPDATE_FW
unsigned char ILITEK_CTPM_FW[]={
	#include "ilitek_fw.h"
};
#endif

#ifdef RESET_GPIO
    static int reset_pin = RESET_GPIO;
#else
    static int reset_pin = 0;
#endif


static char EXCHANG_XY = 0;
static char REVERT_X = 0;
static char REVERT_Y = 0;
static char DBG_FLAG,DBG_COR;

#define DBG(fmt, args...)   if (DBG_FLAG)printk("%s(%d): " fmt, __func__,__LINE__,  ## args)
#define DBG_CO(fmt, args...)   if (DBG_FLAG||DBG_COR)printk("%s: " fmt, "ilitek",  ## args)

#ifdef ILI_UPDATE_FW
static int ilitek_upgrade_firmware(void);
#endif

static int ilitek_i2c_register_device(void);
static void ilitek_set_input_param(struct input_dev*, int, int, int);
static int ilitek_i2c_read_tp_info(void);
static int ilitek_i2c_reread_tp_info(void);

static int ilitek_init(void);
static void ilitek_exit(void);

// i2c functions
static int ilitek_i2c_transfer(struct i2c_client*, struct i2c_msg*, int);
static int ilitek_i2c_read(struct i2c_client*, uint8_t, uint8_t*, int);
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

// file operation functions
static int ilitek_file_open(struct inode*, struct file*);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static int ilitek_file_open(struct inode*, struct file*);
static ssize_t ilitek_file_write(struct file*, const char*, size_t, loff_t*);
static ssize_t ilitek_file_read(struct file*, char*, size_t, loff_t*);
static int ilitek_file_close(struct inode*, struct file*);

static void ilitek_i2c_irq_enable(void);//luca 20120120
static void ilitek_i2c_irq_disable(void);//luca 20120120
static int Request_IRQ(void);

// declare touch point data
/*struct touch_data {
	// x, y value
	int x, y;
	// check wehther this point is valid or not
	int valid;
	// id information
	int id;
};
*/

//key
struct key_info {
	int id;
	int x;
	int y;
	int status;
	int flag;
};

// declare i2c data member
struct i2c_data {
	// input device
	struct input_dev *input_dev;
	// i2c client
	struct i2c_client *client;
	// polling thread
	struct task_struct *thread;
	//firmware version
	unsigned char firmware_ver[4];
	// maximum x
	int max_x;
	// maximum y
	int max_y;
		// minimum x
	int min_x;
	// minimum y
	int min_y;
	// maximum touch point
	int max_tp;
	// maximum key button
	int max_btn;
	// the total number of x channel
	int x_ch;
	// the total number of y channel
	int y_ch;
	// check whether i2c driver is registered success
	int valid_i2c_register;
	// check whether input driver is registered success
	int valid_input_register;
	// check whether the i2c enter suspend or not
	int stop_polling;
	// read semaphore
	struct semaphore wr_sem;
	// protocol version
	int protocol_ver;
	int set_polling_mode;
	// valid irq request
	int valid_irq_request;
	//reset request flag
	int reset_request_success;
	// work queue for interrupt use only
	struct workqueue_struct *irq_work_queue;
	// work struct for work queue
	struct work_struct irq_work;

    struct timer_list timer;
    int report_status;
	int reset_gpio;
	int irq_status;
	int irq_pin;
	int enable_gpio;
	//irq_status enable:1 disable:0
	struct completion complete;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
    int keyflag;
	int keycount;
	int key_xlen;
	int key_ylen;
	struct key_info keyinfo[10];

		// touch_flag release:0 touch:1
 	int touch_flag ;

};

static struct i2c_data i2c;

static void ilitek_reset(int reset_gpio)
{
    DBG("Enter\n");

    gpio_direction_output(reset_gpio,1);
	msleep(100);
    gpio_direction_output(reset_gpio,0);
    msleep(100);
	gpio_direction_output(reset_gpio,1);
	msleep(100);
	printk("%s, reset_gpio = %d\n", __func__, reset_gpio);
	return;
}

/*
for compatible purpose
judge if driver should loading and do some previous work,
if do not loading driver,return value should < 0
*/
static int ilitek_should_load_driver(void)
{
    //add judge here
    return 0;
}


/*
do some previous work depends on platform before add i2c driver,
if return value  < 0,driver will not register,
if return value  >= 0,conrinue register work
*/
static int ilitek_register_prepare(void)
{
    //if necessary,add work here
    return 0;
}


/*
request reset gpio and reset tp,
return value < 0 means fail
*/
static int ilitek_request_init_reset(void)
{
    s32 ret=0;
    printk("Enter %s\n",__func__);
#if (PLAT == ROCKCHIP)
    i2c.reset_gpio = reset_pin;
    if(i2c.reset_gpio){
    	ret=gpio_request(i2c.reset_gpio, "ILITEK-TOUCH-RESET");
    	if(ret<0)
    	{
    		printk("% ILITEK RESET GPIO REQUEST FAILD!\n");
    		return ret;
    	}
    }
    else{
        printk("ilitek_request_init_reset:can't request reset gpio i2c.reset_gpio = %d\n", i2c.reset_gpio);
        return -1;
    }
#endif

    ilitek_reset(i2c.reset_gpio);
    return ret;
}

static int Request_IRQ(void)
{
    int ret = 0;
    ret = request_irq(i2c.client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING/*| IRQF_SHARED*/ , "ilitek_i2c_irq", &i2c);
    return ret;
}

/*
set the value of i2c.client->irq,could add some other work about irq here
return value < 0 means fail

*/
static int ilitek_set_irq(void)
{
    return 0;
}

/*
  irq here
return value < 0 means fail

*/
static int ilitek_config_irq(void)
{
    return 0;
}

void ilitek_set_finish_init_flag(void)
{
    return;
}
