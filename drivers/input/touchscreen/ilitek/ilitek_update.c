#include "ilitek.h"
extern unsigned char ILITEK_CTPM_FW[];
extern struct i2c_data i2c;
extern void ilitek_i2c_irq_disable(void);
extern void ilitek_i2c_irq_enable(void);
extern char fih_touch_ilitek[32];
extern bool i2c_retry;
int ilitek_i2c_write_update(struct i2c_client *client, uint8_t * cmd, int length)
{
	int ret = 0, i;
	struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = length, .buf = cmd,}
	};

	ret = ilitek_i2c_transfer(client, msgs, 1);
	if(ret < 0 && i2c_retry)
	{
		printk(ILITEK_ERROR_LEVEL "%s, i2c write error, ret %d\n", __func__, ret);
		for(i = 0; i < length; i++)
			printk("cmd[%d]:0x%2X\n", i, cmd[i]);
	}
	return ret;
}

static int ilitek_i2c_read_update(struct i2c_client *client, uint8_t *data, int length)
{
	int ret = 0;

	struct i2c_msg msgs_ret[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
	};


	ret = ilitek_i2c_transfer(client, msgs_ret, 1);
	if(ret < 0){
		printk(ILITEK_ERROR_LEVEL "%s, i2c read error, ret %d\n", __func__, ret);
	}

	return ret;
}
static int inwrite(unsigned int address)
{
	uint8_t outbuff[64];
	int data, ret;
	outbuff[0] = 0x25;
	outbuff[1] = (char)((address & 0x000000FF) >> 0);
	outbuff[2] = (char)((address & 0x0000FF00) >> 8);
	outbuff[3] = (char)((address & 0x00FF0000) >> 16);
	ret = ilitek_i2c_write_update(i2c.client, outbuff, 4);
	//udelay(10);
	ret = ilitek_i2c_read_update(i2c.client, outbuff, 4);
	data = (outbuff[0] + outbuff[1] * 256 + outbuff[2] * 256 * 256 + outbuff[3] * 256 * 256 * 256);
	//printk("%s, data=0x%x, outbuff[0]=%x, outbuff[1]=%x, outbuff[2]=%x, outbuff[3]=%x\n", __func__, data, outbuff[0], outbuff[1], outbuff[2], outbuff[3]);
	return data;
}

int outwrite(unsigned int address, unsigned int data, int size)
{
	int ret = 0, i;
	char outbuff[64];
	outbuff[0] = 0x25;
	outbuff[1] = (char)((address & 0x000000FF) >> 0);
	outbuff[2] = (char)((address & 0x0000FF00) >> 8);
	outbuff[3] = (char)((address & 0x00FF0000) >> 16);
	for(i = 0; i < size; i++)
	{
		outbuff[i + 4] = (char)(data >> (8 * i));
	}
	ret = ilitek_i2c_write_update(i2c.client, outbuff, size + 4);
	return ret;
}

static int inwrite_1byte(unsigned int address) {
	char outbuff[64];
	int data, ret;
	outbuff[0] = 0x25;
	outbuff[1] = (char)((address & 0x000000FF) >> 0);
	outbuff[2] = (char)((address & 0x0000FF00) >> 8);
	outbuff[3] = (char)((address & 0x00FF0000) >> 16);
	ret = ilitek_i2c_write_update(i2c.client, outbuff, 4);
	//udelay(10);
	ret = ilitek_i2c_read_update(i2c.client, outbuff, 1);
	data = (outbuff[0]);
	return data;
}
int vfICERegister_Read(int inAdd)
{
	int i;
	int inTimeCount = 100;
	unsigned char buf[4];
	outwrite(0x41000, 0x3B | (inAdd << 8),4);
	outwrite(0x041004, 0x66AA5500, 4);
	// Check Flag
	// Check Busy Flag
	for(i = 0; i < inTimeCount; i++)
	{
		buf[0] = inwrite_1byte(0x41011);

		if ((buf[0] & 0x01) == 0)
		{
			break;
		}
		mdelay(5);
	}
	return inwrite_1byte(0x41012);
}

int exit_ice_mode(void)
{
	int ret = 0;
	unsigned char buf[4] = {0};
	//ilitek_i2c_irq_disable();
#ifndef SET_RESET
	if(outwrite(0x04004C, 0x2120, 2) < 0)
		return -1;
	mdelay(10);
	i2c_retry = false;
	outwrite(0x04004E, 0x01, 1);
	i2c_retry = true;
	mdelay(50);
	//ilitek_i2c_irq_enable();
	return 0;
#endif

	mdelay(10);
	buf[0] = 0x1b;
	buf[1] = 0x62;
	buf[2] = 0x10;
	buf[3] = 0x18;
	ilitek_i2c_write_update(i2c.client, buf, 4);
	ilitek_i2c_reset();
	return ret;
}
/*
   description
   upgrade firmware
   prarmeters

   return
   status
*/
int ilitek_upgrade_firmware(void)
{
	int upgrade_status = 0, i = 0, j = 0, k = 0, update_len = 0, upgradefwflag = 0, inTimeCount = 1000;
	unsigned char buf[512] = {0};
	unsigned long ap_startaddr = 0, df_startaddr = 0, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0, temp = 0, ic_checksum = 0;
	unsigned char firmware_ver[4];
	int retry = 0;
	if(i2c.suspend_flag == 1)
	{
		printk("system is suspend mode\n");
		return 5;
	}
	ap_startaddr = ( ILITEK_CTPM_FW[0] << 16 ) + ( ILITEK_CTPM_FW[1] << 8 ) + ILITEK_CTPM_FW[2];
	ap_endaddr = ( ILITEK_CTPM_FW[3] << 16 ) + ( ILITEK_CTPM_FW[4] << 8 ) + ILITEK_CTPM_FW[5];
	ap_checksum = ( ILITEK_CTPM_FW[6] << 16 ) + ( ILITEK_CTPM_FW[7] << 8 ) + ILITEK_CTPM_FW[8];
	df_startaddr = ( ILITEK_CTPM_FW[9] << 16 ) + ( ILITEK_CTPM_FW[10] << 8 ) + ILITEK_CTPM_FW[11];
	df_endaddr = ( ILITEK_CTPM_FW[12] << 16 ) + ( ILITEK_CTPM_FW[13] << 8 ) + ILITEK_CTPM_FW[14];
	df_checksum = ( ILITEK_CTPM_FW[15] << 16 ) + ( ILITEK_CTPM_FW[16] << 8 ) + ILITEK_CTPM_FW[17];
	firmware_ver[0] = ILITEK_CTPM_FW[18];
	firmware_ver[1] = ILITEK_CTPM_FW[19];
	firmware_ver[2] = ILITEK_CTPM_FW[20];
	firmware_ver[3] = ILITEK_CTPM_FW[21];
	update_len = (( ILITEK_CTPM_FW[26] << 16 ) + ( ILITEK_CTPM_FW[27] << 8 ) + ILITEK_CTPM_FW[28]) + (( ILITEK_CTPM_FW[29] << 16 ) + ( ILITEK_CTPM_FW[30] << 8 ) + ILITEK_CTPM_FW[31]);
	printk("ap_startaddr=0x%lX, ap_endaddr=0x%lX, ap_checksum=0x%lX,update page len=%d\n", ap_startaddr, ap_endaddr, ap_checksum, UPDATE_PAGE_LEN);
	printk("df_startaddr=0x%lX, df_endaddr=0x%lX, df_checksum=0x%lX\n", df_startaddr, df_endaddr, df_checksum);
	buf[0] = 0x10;
	if(ilitek_i2c_write_read(i2c.client, buf, 1, 0, buf, 3) < 0)
		return 2;
	if (buf[1] >= 0x80) {
		buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
		if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 4) < 0)
			return 2;
		for(i = 0; i < 4; i++)
		{
			i2c.firmware_ver[i] = buf[i];
		}
	for(i = 0; i < 4; i++)
	{
		printk("i2c.firmware_ver[%d] = %d, firmware_ver[%d] = %d\n", i, i2c.firmware_ver[i], i, ILITEK_CTPM_FW[i + 18]);
		if(i2c.firmware_ver[i] == 0xFF)
		{
			printk("FW version is 0xFF\n");
			break;
		}
		else if(i2c.firmware_ver[i] != ILITEK_CTPM_FW[i + 18] )
		{
			break;
		}
		else if((i == 3) && (i2c.firmware_ver[3] == ILITEK_CTPM_FW[3 + 18]))
		{
				upgradefwflag = 1;
		}
	}
	}
	else
		printk("FW not ready, need update FW\n");

//	if (ic2120) {
Retry:
		if(outwrite(0x181062, 0x0, 0) < 0)
			return 2;
		//disable watch dog
		outwrite(0x05200C, 0x0000, 2);
		outwrite(0x052020, 0x01, 1);
		outwrite(0x052020, 0x00, 1);

		outwrite(0x42000, 0x0F154900, 4);
		outwrite(0x42014, 0x02, 1);
		outwrite(0x42000, 0x00000000, 4);
		//------------------------------
		if(outwrite(0x041000, 0xab, 1) < 0)
			return 2;
		if(outwrite(0x041004, 0x66aa5500, 4) < 0)
			return 2;
		if(outwrite(0x04100d, 0x00, 1) < 0)
			return 2;
		if(outwrite(0x04100b, 0x03, 1) < 0)
			return 2;
		if(outwrite(0x041009, 0x0000, 2) < 0)
			return 2;
		if(upgradefwflag == 1)
		{
			//get checksum
			outwrite(0x4100B, 0x23, 1);
			outwrite(0x41009, ap_endaddr, 2);
			outwrite(0x41000, 0x3B | (ap_startaddr << 8), 4);
			outwrite(0x041004, 0x66AA5500, 4);
			for(i = 0; i < inTimeCount; i++)
			{
				buf[0] = inwrite_1byte(0x41011);

				if ((buf[0] & 0x01) == 0)
				{
					break;
				}
				mdelay(100);
			}
			ic_checksum = inwrite(0x41018);

			if(ic_checksum != ap_checksum)
			{
				printk("IC ichecksum: 0x%lx, hex checksum:0x%lx\n", ic_checksum, ap_checksum);
				upgradefwflag = 0;
			}
		}
		if(upgradefwflag == 1)
		{
			printk("ilitek_upgrade_firmware Do not need update\n");
			if(exit_ice_mode() < 0)
				return 2;
			return 4;
		}
		mdelay(5);
		for (i = 0; i <= 0xd000;i += 0x1000) {
			//printk("%s, i = %X\n", __func__, i);
			if(outwrite(0x041000, 0x06, 1) < 0)
				return 2;
			mdelay(3);
			if(outwrite(0x041004, 0x66aa5500, 4) < 0)
				return 2;
			mdelay(3);
			temp = (i << 8) + 0x20;
			if(outwrite(0x041000, temp, 4) < 0)
				return 2;
			mdelay(3);
			if(outwrite(0x041004, 0x66aa5500, 4) < 0)
				return 2;
			mdelay(20);
			for (j = 0; j < 50; j++) {
				if(outwrite(0x041000, 0x05, 1) < 0)
					return 2;
				if(outwrite(0x041004, 0x66aa5500, 4) < 0)
					return 2;
				mdelay(1);
				buf[0] = inwrite(0x041013);
				//printfinfo("%s, buf[0] = %X\n", __func__, buf[0]);
				if (buf[0] == 0) {
					break;
				}
				else {

					mdelay(2);
				};
			}
		}
		mdelay(100);
		for(i = ap_startaddr; i < ap_endaddr; i += UPDATE_PAGE_LEN) {
			//printk("%s, i = %X\n", __func__, i);
			if(outwrite(0x041000, 0x06, 1) < 0)
				return 2;
			//mdelay(1);
			if(outwrite(0x041004, 0x66aa5500, 4) < 0)
				return 2;
			//mdelay(1);
			temp = (i << 8) + 0x02;
			if(outwrite(0x041000, temp, 4) < 0)
				return 2;
			//mdelay(1);
			if(outwrite(0x041004, 0x66aa5500 + UPDATE_PAGE_LEN - 1, 4) < 0)
				return 2;
			//mdelay(1);
			buf[0] = 0x25;
			buf[3] = (char)((0x041020  & 0x00FF0000) >> 16);
			buf[2] = (char)((0x041020  & 0x0000FF00) >> 8);
			buf[1] = (char)((0x041020  & 0x000000FF));
			for(k = 0; k < UPDATE_PAGE_LEN; k++)
			{
				buf[4 + k] = ILITEK_CTPM_FW[i + 32 + k];
			}


			if(ilitek_i2c_write_update(i2c.client, buf, UPDATE_PAGE_LEN + 4) < 0) {
				printk("%s, write data error, address = 0x%X, start_addr = 0x%X, end_addr = 0x%X\n", __func__, (int)i, (int)ap_startaddr, (int)ap_endaddr);
				return 2;
			}

			upgrade_status = (i * 100) / update_len;
			//printk("%cupgrade firmware(ap code), %02d%c", 0x0D, upgrade_status, '%');
			mdelay(3);
		}
		//get checksum
		outwrite(0x4100B, 0x23, 1);
		outwrite(0x41009, ap_endaddr, 2);
		outwrite(0x41000, 0x3B | (ap_startaddr << 8), 4);
		outwrite(0x041004, 0x66AA5500, 4);
		for(i = 0; i < inTimeCount; i++)
		{
			buf[0] = inwrite_1byte(0x41011);

			if ((buf[0] & 0x01) == 0)
			{
				break;
			}
			mdelay(100);
		}
		ic_checksum = inwrite(0x41018);
		if(ic_checksum != ap_checksum)
		{
			printk("IC ichecksum: 0x%lx, hex checksum:0x%lx, checksum error\n", ic_checksum, ap_checksum);
			if (retry < 2) {
				retry++;
				goto Retry;
			}
			else
			{
				printk("upgrade FW Fail\n");
				exit_ice_mode();
				return 3;
			}
		}
		printk("%cupgrade firmware(ap code), %02d%c", 0x0D, 100, '%');
		if(exit_ice_mode() < 0)
			return 2;

		printk("upgrade ILITEK_IOCTL_I2C_RESET end end\n");
		//mdelay(100);
		buf[0] = 0x10;
		if(ilitek_i2c_write_read(i2c.client, buf, 1, 0, buf, 3) < 0)
			return 2;
		printk("%s, buf = %X, %X, %X\n", __func__, buf[0], buf[1], buf[2]);
		if (buf[1] >= 0x80) {
			printk("upgrade FW Pass\n");
		}else {
			if (retry < 2) {
				retry++;
				goto Retry;
			}
			else
			{
				printk("upgrade FW Fail\n");
				return 3;
			}
		}
	//}
	mdelay(100);

	//Win add for touch firmware version
	buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	if(ilitek_i2c_write_read(i2c.client, buf, 1, 10, buf, 4) < 0)
	{
		return -1;
	}
	//Win add for touch firmware version
	snprintf(fih_touch_ilitek, PAGE_SIZE, "ilitek-V%d%d%d%02d\n", buf[0], buf[1], buf[2], buf[3]);
	#if PLATFORM_FUN == PLATFORM_D1C
	fih_info_set_touch(fih_touch_ilitek);
	//Win add for touch firmware version
	#endif
	//Win add for touch firmware version
	return 0;
}

