/****************************************************************************
* File: fih_bbs_camera.c                                                    *
* Description: write camera error message to bbs log                        *
*                                                                           *
****************************************************************************/

/****************************************************************************
*                               Include File                                *
****************************************************************************/
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/io.h>
#include "fih_bbs_camera.h"

/****************************************************************************
*                               Module I2C ADDR                             *
****************************************************************************/
struct fih_bbs_camera_module_list {
	u8 i2c_addr;
	u8 type;
	char module_name[32];
};

//IMX258 color
#define FIH_I2C_ADDR_IMX258_COLOR_CAMERA 0x34
#define FIH_I2C_ADDR_IMX258_COLOR_EEPROM 0xA8
#define FIH_I2C_ADDR_IMX258_COLOR_ACTUATOR 0x1C
#define FIH_I2C_ADDR_IMX258_COLOR_OIS 0x7C

//IMX258 mono
#define FIH_I2C_ADDR_IMX258_MONO_CAMERA 0x20
#define FIH_I2C_ADDR_IMX258_MONO_EEPROM 0xA8
#define FIH_I2C_ADDR_IMX258_MONO_ACTUATOR 0x1C

//IMX258 fronts
#define FIH_I2C_ADDR_IMX258_FRONT_CAMERA 0x34
#define FIH_I2C_ADDR_IMX258_FRONT_EEPROM 0xA0
#define FIH_I2C_ADDR_IMX258_FRONT_ACTUATOR 0x18

/****************************************************************************
*                          Private Type Declaration                         *
****************************************************************************/

struct fih_bbs_camera_module_list fih_camera_list[] = {
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_COLOR_CAMERA,
	.type = ((FIH_BBS_CAMERA_LOCATION_MAIN << 4) | FIH_BBS_CAMERA_MODULE_IC),
	.module_name = "Camera ic (IMX258 color)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_COLOR_EEPROM,
	.type = ((FIH_BBS_CAMERA_LOCATION_MAIN << 4) | FIH_BBS_CAMERA_MODULE_EEPROM),
	.module_name = "EEPROM (IMX258 color)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_COLOR_ACTUATOR,
	.type = ((FIH_BBS_CAMERA_LOCATION_MAIN << 4) | FIH_BBS_CAMERA_MODULE_ACTUATOR),
	.module_name = "Actuator (IMX258 color)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_COLOR_OIS,
	.type = ((FIH_BBS_CAMERA_LOCATION_MAIN << 4) | FIH_BBS_CAMERA_MODULE_OIS),
	.module_name = "OIS (IMX258 color)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_MONO_CAMERA,
	.type = ((FIH_BBS_CAMERA_LOCATION_SUB << 4) | FIH_BBS_CAMERA_MODULE_IC),
	.module_name = "Camera ic (IMX258 mono)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_MONO_EEPROM,
	.type = ((FIH_BBS_CAMERA_LOCATION_SUB << 4) | FIH_BBS_CAMERA_MODULE_EEPROM),
	.module_name = "EEPROM (IMX258 mono)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_MONO_ACTUATOR,
	.type = ((FIH_BBS_CAMERA_LOCATION_SUB << 4) | FIH_BBS_CAMERA_MODULE_ACTUATOR),
	.module_name = "Actuator (IMX258 mono)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_FRONT_CAMERA,
	.type = ((FIH_BBS_CAMERA_LOCATION_FRONT << 4) | FIH_BBS_CAMERA_MODULE_IC),
	.module_name = "Camera ic (IMX258 front)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_FRONT_EEPROM,
	.type = ((FIH_BBS_CAMERA_LOCATION_FRONT << 4) | FIH_BBS_CAMERA_MODULE_EEPROM),
	.module_name = "EEPROM (IMX258 front)",
	},
	{
	.i2c_addr = FIH_I2C_ADDR_IMX258_FRONT_ACTUATOR,
	.type = ((FIH_BBS_CAMERA_LOCATION_FRONT << 4) | FIH_BBS_CAMERA_MODULE_ACTUATOR),
	.module_name = "Actuator (IMX258 front)",
	},
};


/**********************************************************************
                         Public Function                              *
**********************************************************************/

void fih_bbs_camera_msg(int module, int error_code)
{
  char param1[32]; // Camera ic, actuator, EEPROM, OIS,¡K
  char param2[64]; // I2C flash error, power up fail, ¡K

	switch (module) {
		case FIH_BBS_CAMERA_MODULE_IC: strcpy(param1, "Camera ic"); break;
		case FIH_BBS_CAMERA_MODULE_EEPROM: strcpy(param1, "EEPROM"); break;
		case FIH_BBS_CAMERA_MODULE_ACTUATOR: strcpy(param1, "Actuator"); break;
		case FIH_BBS_CAMERA_MODULE_OIS: strcpy(param1, "OIS"); break;
		default: strcpy(param1, "Unknown"); break;
  }

	switch (error_code) {
		case FIH_BBS_CAMERA_ERRORCODE_POWER_UP: strcpy(param2, "Power up fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_POWER_DW: strcpy(param2, "Power down fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR: strcpy(param2, "MCLK error"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_READ: strcpy(param2, "i2c read err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE: strcpy(param2, "i2c write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ: strcpy(param2, "i2c seq write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_UNKOWN: strcpy(param2, "unknow error"); break;
		default: strcpy(param2, "Unknown"); break;
	}

  printk("BBox::UPD;88::%s::%s\n", param1, param2);

  return ;
}
EXPORT_SYMBOL(fih_bbs_camera_msg);

void fih_bbs_camera_msg_by_type(int type, int error_code)
{
  char param1[32]; // Camera ic, actuator, EEPROM, OIS,¡K
  char param2[64]; // I2C flash error, power up fail, ¡K
  int isList=0;
  int i=0;

  for(i=0; i<(sizeof(fih_camera_list)/sizeof(struct fih_bbs_camera_module_list)); i++)
  {
    if(type == fih_camera_list[i].type)
    {
      strcpy(param1, fih_camera_list[i].module_name);
      isList = 1;
      break;
    }
  }

  //Do not find in list.
  if(!isList)
  {
    strcpy(param1, "Unknown ");
  }

	switch (error_code) {
		case FIH_BBS_CAMERA_ERRORCODE_POWER_UP: strcpy(param2, "Power up fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_POWER_DW: strcpy(param2, "Power down fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR: strcpy(param2, "MCLK error"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_READ: strcpy(param2, "i2c read err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE: strcpy(param2, "i2c write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ: strcpy(param2, "i2c seq write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_UNKOWN: strcpy(param2, "unknow error"); break;
		default: strcpy(param2, "Unknown"); break;
	}

  printk("BBox::UPD;88::%s::%s\n", param1, param2);

  return ;
}
EXPORT_SYMBOL(fih_bbs_camera_msg_by_type);

void fih_bbs_camera_msg_by_addr(int addr, int error_code)
{
  char param1[32]; // Camera ic, actuator, EEPROM, OIS,¡K
  char param2[64]; // I2C flash error, power up fail, ¡K
  int isList=0;
  int i=0;

  for(i=0; i<(sizeof(fih_camera_list)/sizeof(struct fih_bbs_camera_module_list)); i++)
  {
    if(addr == (fih_camera_list[i].i2c_addr>>1))
    {
      strcpy(param1, fih_camera_list[i].module_name);
      isList = 1;
      break;
    }
  }

  //Do not find in list.
  if(!isList)
    strcpy(param1, "Unknown module");

	switch (error_code) {
		case FIH_BBS_CAMERA_ERRORCODE_POWER_UP: strcpy(param2, "Power up fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_POWER_DW: strcpy(param2, "Power down fail"); break;
		case FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR: strcpy(param2, "MCLK error"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_READ: strcpy(param2, "i2c read err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE: strcpy(param2, "i2c write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ: strcpy(param2, "i2c seq write err"); break;
		case FIH_BBS_CAMERA_ERRORCODE_UNKOWN: strcpy(param2, "unknow error"); break;
		default: strcpy(param2, "Unknown"); break;
	}

  printk("BBox::UPD;88::%s::%s\n", param1, param2);

  return ;
}
EXPORT_SYMBOL(fih_bbs_camera_msg_by_addr);

/************************** End Of File *******************************/
