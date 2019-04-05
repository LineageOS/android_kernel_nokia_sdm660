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
//driver information
#define DERVER_VERSION_MAJOR  3
#define DERVER_VERSION_MINOR  0
#define RELEASE_VERSION     17
//hardware setting

//i2c board info

//debug message
#define ILITEK_DEBUG_LEVEL  KERN_ERR	//Orig - KERN_ALERT	//HL*_20170327
#define ILITEK_ERROR_LEVEL  KERN_ERR	//Orig - KERN_ALERT	//HL*_20170327
#define DBG(fmt, args...) if (DBG_FLAG)printk("%s(%d): " fmt, __func__, __LINE__, ## args)
#define DBG_CO(fmt, args...)  if (DBG_FLAG || DBG_COR)printk("%s: " fmt, "ilitek",  ## args)

//gesture resume function
#define GESTURE									//SW4-HL-Touch-ImplementDoubleTap-00*_20170623
//#ifdef GESTURE
//#define GESTURE_FUN_1 1 //model learning function
//#define GESTURE_FUN_2 2 //algorithm function
//#define GESTURE_FUN   GESTURE_FUN_2
//#define _DOUBLE_CLICK_
//#define GESTURE_H
//#include "gesture.h"
//#endif

//defing key pad function for protocol 2.X
//#define VIRTUAL_KEY_PAD
#define VIRTUAL_FUN_1 1 //0X81 with key_id
#define VIRTUAL_FUN_2 2 //0x81 with x position
#define VIRTUAL_FUN_3 3 //Judge x & y position
//#define VIRTUAL_FUN VIRTUAL_FUN_2
#define BTN_DELAY_TIME  500 //ms
/*Win add for proc callback function*/
#include "../../../fih/fih_touch.h"
//define key pad range for protocol 2.X
#define KEYPAD01_X1 0
#define KEYPAD01_X2 1000
#define KEYPAD02_X1 1000
#define KEYPAD02_X2 2000
#define KEYPAD03_X1 2000
#define KEYPAD03_X2 3000
#define KEYPAD04_X1 3000
#define KEYPAD04_X2 3968
#define KEYPAD_Y  2100

//define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)
#define ILITEK_IOCTL_DRIVER_INFORMATION       _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)
#define ILITEK_IOCTL_I2C_INT_FLAG             _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE       _IOWR(ILITEK_IOCTL_BASE, 14, int)//default setting is i2c interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ       _IOWR(ILITEK_IOCTL_BASE, 15, int)
#define ILITEK_IOCTL_UPDATE_FLAG        _IOWR(ILITEK_IOCTL_BASE, 16, int)
#define ILITEK_IOCTL_I2C_UPDATE_FW        _IOWR(ILITEK_IOCTL_BASE, 18, int)
#define ILITEK_IOCTL_INT_STATUS					_IOWR(ILITEK_IOCTL_BASE, 20, int)
#define ILITEK_IOCTL_DEBUG_SWITCH				_IOWR(ILITEK_IOCTL_BASE, 21, int)
#define ILITEK_IOCTL_I2C_GESTURE_FLAG     _IOWR(ILITEK_IOCTL_BASE, 26, int)
#define ILITEK_IOCTL_I2C_GESTURE_RETURN     _IOWR(ILITEK_IOCTL_BASE, 27, int)
#define ILITEK_IOCTL_I2C_GET_GESTURE_MODEL    _IOWR(ILITEK_IOCTL_BASE, 28, int)
#define ILITEK_IOCTL_I2C_LOAD_GESTURE_LIST    _IOWR(ILITEK_IOCTL_BASE, 29, int)

//i2c command for ilitek touch screen
#define ILITEK_TP_CMD_READ_DATA       0x10
#define ILITEK_TP_CMD_READ_SUB_DATA     0x11
#define ILITEK_TP_CMD_GET_RESOLUTION    0x20
#define ILITEK_TP_CMD_GET_KEY_INFORMATION 0x22
#define ILITEK_TP_CMD_SLEEP                 0x30
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION  0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION  0x42
#define ILITEK_TP_CMD_CALIBRATION     0xCC
#define ILITEK_TP_CMD_CALIBRATION_STATUS  0xCD
#define ILITEK_TP_CMD_ERASE_BACKGROUND    0xCE

//i2c command for Protocol 3.1
#define ILITEK_TP_CMD_TOUCH_STATUS      0x0F

#define TOUCH_POINT   0x80
#define TOUCH_KEY   0xC0
#define RELEASE_KEY   0x40
#define RELEASE_POINT 0x00
//----TP function define-------
//#define ROTATE_FLAG
//#define TRANSFER_LIMIT
#define CLOCK_INTERRUPT
//#define SET_RESET
#define TOUCH_PROTOCOL_B
//#define TOUCH_PRESSURE
#define ILITEK_I2C_RETRY_COUNT    3
#define PLATFORM_D1C 		1
#define PLATFORM_TINY210 	2
#define PLATFORM_RK3288 	3
#define PLATFORM_C1N 		4				//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
#define PLATFORM_FUN		PLATFORM_C1N	//SW4-HL-TouchPanel-BringUpILI2121-00*_20170421
//#define CONFIG_OF
//#define CONFIG_FB
#define ROSILUTION_X 1080
#define ROSILUTION_Y 1920
#define SUPPORT_MAXPOINT 10
//upgrade firmware on boot
#define UPGRADE_FIRMWARE_ON_BOOT
#define FB_READY_WAIT_MS 100
#define FB_READY_TIMEOUT_S 6
#ifdef UPGRADE_FIRMWARE_ON_BOOT
	#define UPGRADE_THREAD		1
	#define UPGRADE_BUILDIN 	2
	#define UPGRAGE_FIRMWARE_MODE UPGRADE_BUILDIN
#endif
#define UPDATE_PAGE_LEN 256

//-----------------------------
#define ILITEK_I2C_DRIVER_NAME    "ilitek_i2c"
#define ILITEK_FILE_DRIVER_NAME   "ilitek_file"
int ilitek_i2c_transfer(struct i2c_client*, struct i2c_msg*, int);
int ilitek_i2c_reset(void);
extern int ilitek_upgrade_firmware(void);
int ilitek_i2c_write_read(struct i2c_client *client, uint8_t *cmd, int write_length, int delay, uint8_t *data, int read_length);
void touch_upgrade_write(int flag);
extern int ilitek_i2c_IRam_test(void);
//extern int ilitek_CDC_function(void);
//key
struct key_info {
  int id;
  int x;
  int y;
  int status;
  int flag;
};
//touch
struct touch_info {
  int id;
  int x;
  int y;
  int status;
  int flag;
};
// declare i2c data member
struct i2c_data {
   struct device    *dev;
  // input device
  struct input_dev *input_dev;
  // i2c client
  struct i2c_client *client;
  // polling thread
  struct task_struct *thread;
	// upgrade thread
	struct task_struct *upgrade_thread;
  // maximum x
  int max_x;
  // maximum y
  int max_y;
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
  //firmware version
    unsigned char firmware_ver[4];
  // valid irq request
  int valid_irq_request;
  // work queue for interrupt use only
  struct workqueue_struct *irq_work_queue;
  // work struct for work queue
  struct work_struct irq_work;

  struct timer_list timer;

  int report_status;

  int irq_status;
  //irq_status enable:1 disable:0
  struct completion complete;
  // touch_flag release:0 touch:1
  int touch_flag ;
#ifdef CONFIG_HAS_EARLYSUSPEND
  struct early_suspend early_suspend;
#endif
	int fw_upgrade_flag;
  int keyflag;
  int keycount;
  int key_xlen;
  int key_ylen;
  struct key_info keyinfo[10];
  struct touch_info touchinfo[10];
  int irq_gpio;
  int reset_gpio;
  struct pinctrl *ts_pinctrl;
  struct pinctrl_state *pinctrl_state_active;
  struct pinctrl_state *pinctrl_state_suspend;
  struct pinctrl_state *pinctrl_state_release;
	int suspend_flag;//suspend flag enable:1 disable:0
	bool debug_flag;
	//win add for ALT test
	struct workqueue_struct  *alt_test_wq;
	struct hrtimer timer_tpalt;
	struct work_struct tpalt_work;
	//win add for ALT test
};

//device data
struct dev_data {
  //device number
  dev_t devno;
  //character device
  struct cdev cdev;
  //class device
  struct class *class;
};
//global variables
#if PLATFORM_FUN == PLATFORM_TINY210
	#define TP_RESET(x)     	(S5PV210_GPE1(0))
#endif
