/* copy from /LINUX/android/external/UpdateCDA/manufacture.h */

/* Note:
   (1) Refer to Mulberry project
   (2) Reserve a partition for store Manufacture data
*/

#ifndef _CDA_MANUFACTURE_H
#define _CDA_MANUFACTURE_H

#define MANUFACTURE_SIZE (1024*1024)
#define MANUFACTURE_OFFSET (0x100000)
#define MANUFACTURE_CALIBRATION_FILE_OFFSET (0x1000)  //4096 bytes

#define MANUF_PARTITION_NAME  "deviceinfo"
#define MANUF_FILE_LOCATION "/dev/block/bootdevice/by-name/deviceinfo"
#define MULTICDA_ID_OFFSET 4096

#define MANUF_MAX_DATA_LEN 64
#define MANUF_MAGIC_LEN 8
#define MANUF_SYNC_INFO_LEN 8
#define MANUF_VERSION_LEN 8
#define MANUF_NAME_LEN 32
#define MANUF_DATA_LEN 4
#define MANUF_HARDWAREID_LEN 16
#define MANUF_PRODUCTID_LEN 32
#define MANUF_PRODUCTID2_LEN 32
#define MANUF_BT_ADDR_LEN 12
#define MANUF_MAC_ADDR_LEN 17
#define MANUF_SECRET_NUM_LEN 32
#define MANUF_BOOTFLAG_LEN 16
#define MANUF_DOWNLOADFLAG_LEN 16
#define MANUF_ONETIMEFTMFLAG_LEN 16
#define MANUF_RESERVED_LEN 32
#define MANUF_CALIBRATION_FILENAME_LEN 128
#define MANUF_CALIBRATION_DATA_SIZE (4*1024) //KB
#define MANUF_SKUID_LEN 16

/* Length of MANUF_MAGIC_NUM need to match MANUF_MAGIC_LEN */
#define MANUF_MAGIC_STRING "MANUDONE"

/* Length of MANUF_SYNC_INFO_STRING need to match MANUF_SYNC_INFO_LEN */
#define MANUF_SYNC_INFO_STRING "MANUSYNC"

#define MANUF_VERSION_STRING "VERSION"
#define MANUF_HARDWAREID_STRING "HARDWAREID"
#define MANUF_PRODUCTID_STRING "PRODUCTID"
#define MANUF_PRODUCTID2_STRING "PRODUCTID2"
#define MANUF_BT_ADDR_STRING "BTADDR"
#define MANUF_MAC_ADDR_STRING "MACADDR"
#define MANUF_SECRET_NUM_STRING "SECRET"
#define MANUF_BOOTFLAG_STRING "BOOTFLAG"
#define MANUF_DOWNLOADFLAG_STRING "DOWNLOADFLAG"
#define MANUF_ONETIMEFTMFLAG_STRING "ONETIMEFTMFLAG"
#define MANUF_CALIBRATION_DATA_STRING "CAL_DATA"

#define MANUF_SKUID_DATA_STRING "SKUID"
#define MANUF_SKUID_CHANGE_FLAG "SKUchange"

/* status */
#define SUCCESS 0x00
//#define FAIL    0x01

#define INVALID_MODE   0x55
#define FILE_NOT_FOUND 0x03
#define FILE_CORRUPTED 0x04

/* return for PID read/write */
#define READ_MANUFACTURE_FAIL  (-1)
#define WRITE_MANUFACTURE_FAIL (-2)

/* Note : if you add a new structure, please add reserved data in this feature for extension */
struct manuf_version
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char version[MANUF_VERSION_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_hardware_id
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char hardwareid[MANUF_HARDWAREID_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_product_id
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char productid[MANUF_PRODUCTID_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_product_id_2
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char productid2[MANUF_PRODUCTID2_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_bluetooth_bt_addr
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char btaddr[MANUF_BT_ADDR_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_wifi_mac_addr
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char macaddr[MANUF_MAC_ADDR_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_wifi_secret_num
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char secret[MANUF_SECRET_NUM_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_boot_flag
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char bootflag[MANUF_BOOTFLAG_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_download_flag
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char downloadflag[MANUF_DOWNLOADFLAG_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_onetimeftm_flag
{
	char name[MANUF_NAME_LEN];
	unsigned int length;
	char onetimeftmflag[MANUF_ONETIMEFTMFLAG_LEN];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_SKU_flag
{
	char name[MANUF_NAME_LEN];
	char change_flag[MANUF_SKUID_LEN];
	char SKUID[60];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_CAVIS
{
	char Register[8];
	char Retry[8];
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_root
{
	unsigned int status;
	char reserved[MANUF_RESERVED_LEN];
};

struct manuf_data
{
	char magic[MANUF_MAGIC_LEN];
	char sync_info[MANUF_SYNC_INFO_LEN];
	struct manuf_version version;
	struct manuf_hardware_id hardwareid;
	struct manuf_product_id productid;
	struct manuf_product_id_2 productid2;
	struct manuf_bluetooth_bt_addr btaddr;
	struct manuf_wifi_mac_addr macaddr;
	struct manuf_wifi_secret_num secret;
	struct manuf_boot_flag bootflag;
	struct manuf_download_flag downloadflag;
	struct manuf_onetimeftm_flag onetimeftmflag;
	struct manuf_SKU_flag SKUflag;
	struct manuf_CAVIS CAVISflag;
	struct manuf_root rootflag;
};

struct manuf_file_data
{
	char magic[MANUF_MAGIC_LEN];
	unsigned int manuf_length;
	char filename[MANUF_CALIBRATION_FILENAME_LEN];
	char file_raw_data[MANUF_CALIBRATION_DATA_SIZE];
};

#endif /* _CDA_MANUFACTURE_H */
