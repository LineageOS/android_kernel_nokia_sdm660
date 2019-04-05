#ifndef __FIH_USB_H
#define __FIH_USB_H

#include <linux/types.h>

enum {
	CHECK_COMMAND = 0,
	EXE_COMMAND_CHECK = 1,
	EXE_COMMAND = 2,
	CHECK_STATUS = 3,
};

enum {
	CHECK_STATUS_FIH_UMS = 0,
	CHECK_STATUS_MODE_SWITCH_REBOOT_FLAG = 1,
	CHECK_STATUS_REBOOT_FLAG = 2,
};

struct fih_usb_data {
	u8 *cmnd;
	void *bh_buf;
	u32 *data_size_from_cmnd;
};

#endif /* __FIH_USB_H */
