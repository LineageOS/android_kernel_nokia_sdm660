#ifndef E2P_H
#define E2P_H

#define OK			0
#define FAILED			1

#define E2P_VERSION		"1.0"
#define SUCCESS			0
#define INVALID_OPT		1
#define ERR_HEADER_VERSION	2
#define ERR_OPENFAILED		3
#define ERR_XDIGIT		4
#define ERR_E2PID		5
#define ERR_OPENFD		6
#define ERR_FAC_INFO		7
#define ERR_ALLOC		8
#define ERR_FAC_DATA		9
#define ERR_PARAM		0xa
#define ERR_DOW_SIZE		0xb
#define ERR_EXCEED_RANGE	0xc
#define ERR_DATA_ZERO		0xd
#define ERR_GET_FD		0xe

#define ERR_NOMATCH		0xff

#define E2P_OFFSET		0
#define BT_WIFI_MAC_ADDR_LEN	6

#define FIH_E2P_PARTITION	"/dev/block/bootdevice/by-name/deviceinfo"
#define E2P_RAW_SIZE		1024

#define E2P_HEADER_LEN		8
#define E2P_TAIL_LEN		8
#define E2P_BT_MAC_MAX_LEN	6
#define E2P_WIFI_MAC_MAX_LEN	6
#define E2P_PID_MAX_LEN		32
#define E2P_PID2_MAX_LEN	32
#define E2P_CPID_MAX_LEN	32
#define E2P_CPID2_MAX_LEN	32
#define E2P_CSWID_MAX_LEN	32
#define E2P_DOM_MAX_LEN		14
#define E2P_FACTORY_INFO_LEN	128
#define E2P_IMEI_MAX_LEN	16
#define E2P_SIMCONFIG_MAX_LEN   8

#define E2P_HEADER_MAGIC	"FIHE2P_B"
#define E2P_TAIL_MAGIC		"FIHE2P_E"

#define LENBUF			1024

//#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))

typedef enum
{
	e2p_id_pid  = 0,
	e2p_id_bt_mac,
	e2p_id_wifi_mac,
	e2p_id_DOM,
	e2p_id_MAX,
	e2p_id_pid2,
	e2p_id_Cpid,
	e2p_id_Cpid2,
	e2p_id_CSWID,
	e2p_id_factoryinfo,
	e2p_id_simconfig,
} E2P_ID;

typedef struct{
	char e2p_header[E2P_HEADER_LEN];
	unsigned int e2p_size;
	unsigned int version;
	char pid[E2P_PID_MAX_LEN];
	char bt_mac[E2P_BT_MAC_MAX_LEN];
	char wifi_mac[E2P_WIFI_MAC_MAX_LEN];
	char date_of_manu[E2P_DOM_MAX_LEN];
	char e2p_tail[E2P_TAIL_LEN];
	unsigned int StructVersion;
	char pid2[E2P_PID_MAX_LEN];
	char Cpid[E2P_PID_MAX_LEN];
	char Cpid2[E2P_PID_MAX_LEN];
	char CSWID[E2P_CSWID_MAX_LEN];
	char factory_info[E2P_FACTORY_INFO_LEN];
	char imei[E2P_IMEI_MAX_LEN];
	char imei2[E2P_IMEI_MAX_LEN];
    char wifi_mac2[E2P_WIFI_MAC_MAX_LEN];
		char simconfig[E2P_SIMCONFIG_MAX_LEN];
} FIH_CSP_E2P_V02;

/* FACTORY_DATA_OFFSET: 512K+512 byte, E2P_FACTORY_DATA_SIZE:512K-512 byte */
#define E2P_FACTORY_DATA_INFO_OFFSET	((544)*(1024))
#define E2P_FACTORY_DATA_INFO_SIZE	(512)
#define E2P_FACTORY_DATA_OFFSET		((E2P_FACTORY_DATA_INFO_OFFSET) + (E2P_FACTORY_DATA_INFO_SIZE))
#define E2P_FACTORY_DATA_SIZE		(((480)*(1024)) - (E2P_FACTORY_DATA_INFO_SIZE))
#define E2P_FACTORY_DATA_HEADER_LEN	8
#define E2P_FACTORY_DATA_TAIL_LEN	8
#define E2P_FACTORY_DATA_HEADER_MAGIC	"FIHFAC_H"
#define E2P_FACTORY_DATA_TAIL_MAGIC	"FIHFAC_T"

typedef struct {
	char header[E2P_FACTORY_DATA_HEADER_LEN];
	unsigned int size;
	char tail[E2P_FACTORY_DATA_TAIL_LEN];
} FIH_FACTORY_DATA_INFO_V01;

#endif
