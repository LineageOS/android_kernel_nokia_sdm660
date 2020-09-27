/*
 *  A simple function to keep useful parameters in a special partition.
 *
 *  Copyright (c) 2017-2020, zhangzhe.sharp.corp, All rights reserved.
 *
 *  Head file of sharp_param.c 
 */

#ifndef SHARP_PARAMS_H_
#define SHARP_PARAMS_H_
#define PARAMINITED             0x012E9700
#define PARAMFORMAT             0x012E49A5
#define PARAM_SIZE              0x00200000
#define PARAM_BLOCK_SIZE        512
#define NAME_LENGTH		16

//(u32)(Major*100 + Minor)
//Minor:0~99
#define PARAM_VERSION		100

enum sharp_params_index {
	PARAM_BLOCK_DEVICE = 0,
	PARAM_BLOCK_MANUFACTURE,
	PARAM_BLOCK_COMMUNICATION,
        PARAM_BLOCK_SECURITY,
        PARAM_BLOCK_MODE,
	PARAM_BLOCK_NUM,
};

const char *sharp_params_map[] = {
	[PARAM_BLOCK_DEVICE]		= "DEVICE",
	[PARAM_BLOCK_MANUFACTURE]	= "MANUFACTURE",
	[PARAM_BLOCK_COMMUNICATION]	= "COMMUNICATION",
	[PARAM_BLOCK_SECURITY]		= "SECURTIY",
	[PARAM_BLOCK_MODE]		= "MODE",
};

//Device information
struct BlockDeviceInfo {
	char 	BlockName[16];
	char    ParamVersion[4];
	char    ParamsFormat[4];
	char    ParamReserve[8];
	char 	DeviceName[16];
	char 	Reserve[464];
};

//Manufacture Related information
struct BlockManufactureInfo {
        char    BlockName[16];
        char    Reserve[496];
};

//Communication Related information
struct BlockCommunicationInfo {
        char    BlockName[16];
        char    Reserve[496];
};

//Security Related setting
struct BlockSecRelatedSetting {
        char    BlockName[16];
        char    Reserve[496];
};

//Device work mode Related information
struct BlockRunModeSetting {
        char	BlockName[16];
	char	RunMode[2];
        char	Reserve[494];
};

//for BlockRunModeSetting.RunMode
enum {
	RUN_MODE_NORMAL = 0,
	RUN_MODE_CTA,
	RUN_MODE_CTS,
	RUN_MODE_CT,
	RUN_MODE_CMCC,
};
#endif
