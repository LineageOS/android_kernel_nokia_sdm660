/****************************************************************************
* File: fih_bbs_camera.c                                                    *
* Description: write camera error message to bbs log                        *
*                                                                           *
****************************************************************************/


#ifndef _FIH_BBS_CAMERA_H_
#define _FIH_BBS_CAMERA_H_

/****************************************************************************
*                               Include File                                *
****************************************************************************/

/****************************************************************************
*                         Public Constant Definition                        *
****************************************************************************/

enum FIH_BBS_CAMERA_LOCATION
{
    FIH_BBS_CAMERA_LOCATION_MAIN,
    FIH_BBS_CAMERA_LOCATION_FRONT,
    FIH_BBS_CAMERA_LOCATION_SUB,
};

enum FIH_BBS_CAMERA_MODULE
{
    FIH_BBS_CAMERA_MODULE_IC,
    FIH_BBS_CAMERA_MODULE_EEPROM,
    FIH_BBS_CAMERA_MODULE_ACTUATOR,
    FIH_BBS_CAMERA_MODULE_OIS,
};


enum FIH_BBS_CAMERA_ERRORCODE
{
    FIH_BBS_CAMERA_ERRORCODE_POWER_UP,
    FIH_BBS_CAMERA_ERRORCODE_POWER_DW,
    FIH_BBS_CAMERA_ERRORCODE_MCLK_ERR,
    FIH_BBS_CAMERA_ERRORCODE_I2C_READ,
    FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE,
    FIH_BBS_CAMERA_ERRORCODE_I2C_WRITE_SEQ,
    FIH_BBS_CAMERA_ERRORCODE_UNKOWN,
    FIH_BBS_CAMERA_ERRORCODE_MAX=255,
};

#define FIH_BBSUEC_MAIN_CAM_ID 9
#define FIH_BBSUEC_FRONT_CAM_ID 44

enum FIH_BBSUEC_CAMERA_ERRORCODE
{
    FIH_BBSUEC_CAMERA_ERRORCODE_SENSOR_I2C = 1,
    FIH_BBSUEC_CAMERA_ERRORCODE_SENSOR_POWER = 10,
    FIH_BBSUEC_CAMERA_ERRORCODE_ACTUATOR_POWER,
    FIH_BBSUEC_CAMERA_ERRORCODE_ACTUATOR_I2C,
    FIH_BBSUEC_CAMERA_ERRORCODE_EEPROM_I2C,
};

#endif

