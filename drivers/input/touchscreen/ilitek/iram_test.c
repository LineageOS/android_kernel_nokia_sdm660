#include "ilitek.h"
#include "ilitek_iram.h"
#include <linux/gpio.h>
extern int outwrite(unsigned int address, unsigned int data, int size);
extern struct i2c_data i2c;
extern void ilitek_i2c_irq_disable(void);
extern void ilitek_i2c_irq_enable(void);
extern int ilitek_i2c_write_read(struct i2c_client *client, uint8_t *cmd, int write_length, int delay, uint8_t *data, int read_length);
extern int ilitek_i2c_write_update(struct i2c_client *client, uint8_t * cmd, int length);
extern bool i2c_retry;
extern int Iramtest_status;
int ilitek_i2c_IRam_test(void)
{
	int i = 0, k = 0;
	unsigned char buf[512] = {0};
	int IramTestEndAddr, Write_len = UPDATE_PAGE_LEN;
	//inital Iramtest_status
	Iramtest_status = 8;	//0 - Pass, 1 - Short fail, 2 - Open fail, 3 - All node fail, 4 - i2c transfer fail, 5 - file error, 6 - NOBK fail, 7 - STDEV fail
	if(i2c.suspend_flag == 1)
	{
		printk("system is suspend mode\n");
		return 5;
	}
	IramTestEndAddr = sizeof(IRAM_TEST);
	printk("IRam test size: %d\n", IramTestEndAddr);
	//reset TP
	printk(" TP reset\n");
	ilitek_i2c_irq_disable();
#ifdef SET_RESET
	gpio_direction_output(i2c.reset_gpio, 1);
	msleep(1);
	gpio_direction_output(i2c.reset_gpio, 0);
	mdelay(5);
	//mdelay(1);
	gpio_direction_output(i2c.reset_gpio, 1);
#else
	if(outwrite(0x181062, 0x0, 0) < 0)
		return 2;
	if(outwrite(0x04004C, 0x2120, 2) < 0)
		return 2;
	mdelay(10);
	i2c_retry = false;
	outwrite(0x04004E, 0x01, 1);
	i2c_retry = true;
#endif
	mdelay(1);
	//entry ice mode
	if(outwrite(0x181062, 0x0, 0) < 0)
		return 2;
	udelay(500);
	//disable watch dog
	if(outwrite(0x05200C, 0x0000, 2) < 0)
		return 2;
	udelay(500);
	if(outwrite(0x052020, 0x01, 1) < 0)
		return 2;
	udelay(500);
	if(outwrite(0x052020, 0x00, 1) < 0)
		return 2;
	udelay(500);
	if(outwrite(0x042000, 0x0F154900, 4) < 0)
		return 2;
	udelay(500);
	if(outwrite(0x042014, 0x02, 1) < 0)
		return 2;
	udelay(500);
	if(outwrite(0x042000, 0x00000000, 4) < 0)
		return 2;
	udelay(500);
	//write test hex to iram
	for(i = 0; i < IramTestEndAddr; i += UPDATE_PAGE_LEN) {
		if(i + UPDATE_PAGE_LEN > IramTestEndAddr)
			Write_len = IramTestEndAddr % UPDATE_PAGE_LEN;
		buf[0] = 0x25;
		buf[3] = (char)((i  & 0x00FF0000) >> 16);
		buf[2] = (char)((i  & 0x0000FF00) >> 8);
		buf[1] = (char)((i  & 0x000000FF));
		for(k = 0; k < Write_len; k++)
		{
			buf[4 + k] = IRAM_TEST[i + k];
		}
		if(ilitek_i2c_write_update(i2c.client, buf, Write_len + 4) < 0) {
			//printk("%s, write data error, address = 0x%X, start_addr = 0x%X, end_addr = 0x%X\n", __func__, (int)i, (int)ap_startaddr, (int)ap_endaddr);
			return 2;
		}
		mdelay(3);
	}
	//ice mode code reset
	if(outwrite(0x040040, 0xAE, 1) < 0)
		return 2;
		mdelay(1);
	//exit ice mode
	buf[0] = 0x1b;
	buf[1] = 0x62;
	buf[2] = 0x10;
	buf[3] = 0x18;
	ilitek_i2c_write_update(i2c.client, buf, 4);
	mdelay(500);
	//check iram status
	if(gpio_get_value(i2c.irq_gpio))
	{
		Iramtest_status = 0;
	}

	//reset
#ifdef SET_RESET
	gpio_direction_output(i2c.reset_gpio, 1);
	msleep(1);
	gpio_direction_output(i2c.reset_gpio, 0);
	mdelay(5);
	//mdelay(1);
	gpio_direction_output(i2c.reset_gpio, 1);
#else
	if(outwrite(0x181062, 0x0, 0) < 0)
		return 2;
	if(outwrite(0x04004C, 0x2120, 2) < 0)
		return -1;
	mdelay(10);
	i2c_retry = false;
	outwrite(0x04004E, 0x01, 1);
	i2c_retry = true;
#endif
	ilitek_i2c_irq_enable();
	return 0;
}