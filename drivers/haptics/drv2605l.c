/*
** =============================================================================
** Copyright (c) 2014  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2605l.c
**
** Description:
**     DRV2605L chip driver
**
** =============================================================================
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/haptics/drv2605l.h>

static struct drv2605L_data *pDRV2605Ldata = NULL;

int getPatternValue=0;


int vibrator_get_pattern_value()
{
	return getPatternValue;
}

void vibrator_set_pattern_value(int value)
{
	getPatternValue = value;
}

/* sysfs store function for ramp step */
static ssize_t qpnp_hap_pattern_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int data, rc;
	struct timed_output_dev *to_dev = dev_get_drvdata(dev);
	struct drv2605L_data *pDrv2605Ldata = container_of(to_dev, struct drv2605L_data, to_dev);

	rc = kstrtoint(buf, 10, &data);
	if (rc)
		return rc;

	printk("qpnp_hap_pattern_store vibrator get pattern value is %d\n",data);
	vibrator_set_pattern_value(data);

	cancel_work_sync(&pDrv2605Ldata->vibrator_pattern_work);
	schedule_work(&pDrv2605Ldata->vibrator_pattern_work);

	return count;
}

static struct device_attribute qpnp_hap_attrs[] = {
	__ATTR(pattern, 0664, NULL, qpnp_hap_pattern_store),
};

static int drv2605L_reg_read(struct drv2605L_data *pDrv2605Ldata, unsigned int reg)
{
	unsigned int val;
	int ret;
	
	ret = regmap_read(pDrv2605Ldata->regmap, reg, &val);
    
	if (ret < 0)
		return ret;
	else
		return val;
}

static int drv2605L_reg_write(struct drv2605L_data *pDrv2605Ldata, unsigned char reg, char val)
{
    //printk(KERN_ERR"%s, reg=%x val=%x \n", __FUNCTION__, reg, val);
    return regmap_write(pDrv2605Ldata->regmap, reg, val);
}

static int drv2605L_bulk_read(struct drv2605L_data *pDrv2605Ldata, unsigned char reg, unsigned int count, u8 *buf)
{
	return regmap_bulk_read(pDrv2605Ldata->regmap, reg, buf, count);
}

static int drv2605L_bulk_write(struct drv2605L_data *pDrv2605Ldata, unsigned char reg, unsigned int count, const u8 *buf)
{
	return regmap_bulk_write(pDrv2605Ldata->regmap, reg, buf, count);
}

static int drv2605L_set_bits(struct drv2605L_data *pDrv2605Ldata, unsigned char reg, unsigned char mask, unsigned char val)
{
	return regmap_update_bits(pDrv2605Ldata->regmap, reg, mask, val);
}

static int drv2605L_set_go_bit(struct drv2605L_data *pDrv2605Ldata, unsigned char val)
{
	return drv2605L_reg_write(pDrv2605Ldata, GO_REG, (val&0x01));
}
#if 0
static void drv2605L_poll_go_bit(struct drv2605L_data *pDrv2605Ldata)
{
    while (drv2605L_reg_read(pDrv2605Ldata, GO_REG) == GO)
      schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}
#endif
static int drv2605L_select_library(struct drv2605L_data *pDrv2605Ldata, unsigned char lib)
{
	return drv2605L_reg_write(pDrv2605Ldata, LIBRARY_SELECTION_REG, (lib&0x07));
}

static int drv2605L_set_rtp_val(struct drv2605L_data *pDrv2605Ldata, char value)
{
	/* please be noted: in unsigned mode, maximum is 0xff, in signed mode, maximum is 0x7f */
	return drv2605L_reg_write(pDrv2605Ldata, REAL_TIME_PLAYBACK_REG, value);
}

static int drv2605L_set_waveform_sequence(struct drv2605L_data *pDrv2605Ldata, unsigned char* seq, unsigned int size)
{
	return drv2605L_bulk_write(pDrv2605Ldata, WAVEFORM_SEQUENCER_REG, (size>WAVEFORM_SEQUENCER_MAX)?WAVEFORM_SEQUENCER_MAX:size, seq);
}

static void drv2605L_change_mode(struct drv2605L_data *pDrv2605Ldata, char work_mode, char dev_mode)
{
	/* please be noted : LRA open loop cannot be used with analog input mode */
	if(dev_mode == DEV_IDLE){
		pDrv2605Ldata->dev_mode = dev_mode;
		pDrv2605Ldata->work_mode = work_mode;
	}else if(dev_mode == DEV_STANDBY){
		if(pDrv2605Ldata->dev_mode != DEV_STANDBY){
			pDrv2605Ldata->dev_mode = DEV_STANDBY;
			drv2605L_reg_write(pDrv2605Ldata, MODE_REG, MODE_STANDBY);
			schedule_timeout_interruptible(msecs_to_jiffies(WAKE_STANDBY_DELAY));
		}
		pDrv2605Ldata->work_mode = WORK_IDLE;
	}else if(dev_mode == DEV_READY){
		if((work_mode != pDrv2605Ldata->work_mode)
			||(dev_mode != pDrv2605Ldata->dev_mode)){
			pDrv2605Ldata->work_mode = work_mode;
			pDrv2605Ldata->dev_mode = dev_mode;
			if((pDrv2605Ldata->work_mode == WORK_VIBRATOR)
				||(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_ON)
				||(pDrv2605Ldata->work_mode == WORK_SEQ_RTP_ON)
				||(pDrv2605Ldata->work_mode == WORK_RTP)){
					drv2605L_reg_write(pDrv2605Ldata, MODE_REG, MODE_REAL_TIME_PLAYBACK);
				//printk(KERN_ERR"%s set to RTP mode\n", __FUNCTION__);
			}else if(pDrv2605Ldata->work_mode == WORK_AUDIO2HAPTIC){
				drv2605L_reg_write(pDrv2605Ldata, MODE_REG, MODE_AUDIOHAPTIC);
			}else if(pDrv2605Ldata->work_mode == WORK_CALIBRATION){
				drv2605L_reg_write(pDrv2605Ldata, MODE_REG, AUTO_CALIBRATION);
			}else{
				drv2605L_reg_write(pDrv2605Ldata, MODE_REG, MODE_INTERNAL_TRIGGER);
				schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
			}
		}
	}
}

static void setAudioHapticsEnabled(struct drv2605L_data *pDrv2605Ldata, int enable)
{
    if (enable)
    {
		if(pDrv2605Ldata->work_mode != WORK_AUDIO2HAPTIC){
			pDrv2605Ldata->vibrator_is_playing = YES;
			drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_READY);

			drv2605L_set_bits(pDrv2605Ldata, 
					Control1_REG, 
					Control1_REG_AC_COUPLE_MASK, 
					AC_COUPLE_ENABLED );
					
			drv2605L_set_bits(pDrv2605Ldata, 
					Control3_REG, 
					Control3_REG_PWMANALOG_MASK, 
					INPUT_ANALOG);	

			drv2605L_change_mode(pDrv2605Ldata, WORK_AUDIO2HAPTIC, DEV_READY);
			switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_AUDIO2HAPTIC);
		}
    } else {
        // Chip needs to be brought out of standby to change the registers
		if(pDrv2605Ldata->work_mode == WORK_AUDIO2HAPTIC){
			pDrv2605Ldata->vibrator_is_playing = NO;
			drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_READY);
						
			drv2605L_set_bits(pDrv2605Ldata, 
					Control1_REG, 
					Control1_REG_AC_COUPLE_MASK, 
					AC_COUPLE_DISABLED );
					
			drv2605L_set_bits(pDrv2605Ldata, 
					Control3_REG, 
					Control3_REG_PWMANALOG_MASK, 
					INPUT_PWM);	
					
			switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_IDLE);		
			drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY); // Disable audio-to-haptics
		}
    }
}

static void play_effect(struct drv2605L_data *pDrv2605Ldata)
{
	switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_SEQUENCE_PLAYBACK);
	drv2605L_change_mode(pDrv2605Ldata, WORK_SEQ_PLAYBACK, DEV_READY);
    drv2605L_set_waveform_sequence(pDrv2605Ldata, pDrv2605Ldata->sequence, WAVEFORM_SEQUENCER_MAX);
	pDrv2605Ldata->vibrator_is_playing = YES;
    drv2605L_set_go_bit(pDrv2605Ldata, GO);

    while((drv2605L_reg_read(pDrv2605Ldata, GO_REG) == GO) && (pDrv2605Ldata->should_stop == NO)){
        schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
	}
	
	if(pDrv2605Ldata->should_stop == YES){
		drv2605L_set_go_bit(pDrv2605Ldata, STOP);
	}
  
    if (pDrv2605Ldata->audio_haptics_enabled){
        setAudioHapticsEnabled(pDrv2605Ldata, YES);
    } else {
        drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY);
		switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_IDLE);		
		pDrv2605Ldata->vibrator_is_playing = NO;
		wake_unlock(&pDrv2605Ldata->wklock);
    }
}

static void play_Pattern_RTP(struct drv2605L_data *pDrv2605Ldata)
{
	if(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_ON){
		drv2605L_change_mode(pDrv2605Ldata, WORK_PATTERN_RTP_OFF, DEV_READY);
		if(pDrv2605Ldata->repeat_times == 0){
			drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY);
			pDrv2605Ldata->vibrator_is_playing = NO;
			switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_IDLE);	
			wake_unlock(&pDrv2605Ldata->wklock);
		}else{
			hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)pDrv2605Ldata->silience_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	}else if(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_OFF){
		pDrv2605Ldata->repeat_times--;
		drv2605L_change_mode(pDrv2605Ldata, WORK_PATTERN_RTP_ON, DEV_READY);
		hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)pDrv2605Ldata->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
}

static void play_Seq_RTP(struct drv2605L_data *pDrv2605Ldata)
{
	if(pDrv2605Ldata->RTPSeq.RTPindex < pDrv2605Ldata->RTPSeq.RTPCounts){
		int RTPTime = pDrv2605Ldata->RTPSeq.RTPData[pDrv2605Ldata->RTPSeq.RTPindex] >> 8;
		int RTPVal = pDrv2605Ldata->RTPSeq.RTPData[pDrv2605Ldata->RTPSeq.RTPindex] & 0x00ff ;
			
		pDrv2605Ldata->vibrator_is_playing = YES;
		pDrv2605Ldata->RTPSeq.RTPindex++;
		drv2605L_change_mode(pDrv2605Ldata, WORK_SEQ_RTP_ON, DEV_READY);
		drv2605L_set_rtp_val(pDrv2605Ldata,  RTPVal);
							
		hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)RTPTime * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}else{
		drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY);
		pDrv2605Ldata->vibrator_is_playing = NO;
		switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_IDLE);	
		wake_unlock(&pDrv2605Ldata->wklock);
	}
}

static void vibrator_off(struct drv2605L_data *pDrv2605Ldata)
{
    if (pDrv2605Ldata->vibrator_is_playing) {
		if(pDrv2605Ldata->audio_haptics_enabled == YES){
			setAudioHapticsEnabled(pDrv2605Ldata, YES);
		}else{
			pDrv2605Ldata->vibrator_is_playing = NO;
			drv2605L_set_go_bit(pDrv2605Ldata, STOP);
			drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY);
			switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_IDLE);

			wake_unlock(&pDrv2605Ldata->wklock);
            #if 0
            printk(KERN_ERR" reg 0x00=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x00));
            printk(KERN_ERR" reg 0x01=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x01));
            printk(KERN_ERR" reg 0x02=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x02));
            printk(KERN_ERR" reg 0x03=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x03));
            printk(KERN_ERR" reg 0x04=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x04));
            printk(KERN_ERR" reg 0x05=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x05));
            printk(KERN_ERR" reg 0x06=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x06));
            printk(KERN_ERR" reg 0x07=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x07));
            printk(KERN_ERR" reg 0x08=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x08));
            printk(KERN_ERR" reg 0x09=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x09));
            printk(KERN_ERR" reg 0x0A=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0A));
            printk(KERN_ERR" reg 0x0B=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0B));
            printk(KERN_ERR" reg 0x0C=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0C));
            printk(KERN_ERR" reg 0x0D=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0D));
            printk(KERN_ERR" reg 0x0E=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0E));
            printk(KERN_ERR" reg 0x0F=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x0F));
            printk(KERN_ERR" reg 0x10=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x10));
            printk(KERN_ERR" reg 0x11=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x11));
            printk(KERN_ERR" reg 0x12=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x12));
            printk(KERN_ERR" reg 0x13=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x13));
            printk(KERN_ERR" reg 0x14=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x14));
            printk(KERN_ERR" reg 0x15=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x15));
            printk(KERN_ERR" reg 0x16=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x16));
            printk(KERN_ERR" reg 0x17=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x17));
            printk(KERN_ERR" reg 0x18=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x18));
            printk(KERN_ERR" reg 0x19=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x19));
            printk(KERN_ERR" reg 0x1A=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1A));
            printk(KERN_ERR" reg 0x1B=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1B));
            printk(KERN_ERR" reg 0x1C=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1C));
            printk(KERN_ERR" reg 0x1D=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1D));
            printk(KERN_ERR" reg 0x1E=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1E));
            printk(KERN_ERR" reg 0x1F=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x1F));
            printk(KERN_ERR" reg 0x20=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x20));
            printk(KERN_ERR" reg 0x21=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x21));
            printk(KERN_ERR" reg 0x22=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x22));
            #endif
		}
    }
}

static void drv2605L_stop(struct drv2605L_data *pDrv2605Ldata)
{
	if(pDrv2605Ldata->vibrator_is_playing){
		if(pDrv2605Ldata->work_mode == WORK_AUDIO2HAPTIC){
			setAudioHapticsEnabled(pDrv2605Ldata, NO);		
		}else if((pDrv2605Ldata->work_mode == WORK_VIBRATOR)
				||(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_ON)
				||(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_OFF)
				||(pDrv2605Ldata->work_mode == WORK_SEQ_RTP_ON)
				||(pDrv2605Ldata->work_mode == WORK_SEQ_RTP_OFF)
				||(pDrv2605Ldata->work_mode == WORK_RTP)){
			vibrator_off(pDrv2605Ldata);
		}else if(pDrv2605Ldata->work_mode == WORK_SEQ_PLAYBACK){
		}else{
			printk(KERN_ERR"%s, err mode=%d \n", __FUNCTION__, pDrv2605Ldata->work_mode);
		}
	}
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2605L_data *pDrv2605Ldata = container_of(dev, struct drv2605L_data, to_dev);

    if (hrtimer_active(&pDrv2605Ldata->timer)) {
        ktime_t r = hrtimer_get_remaining(&pDrv2605Ldata->timer);
        return ktime_to_ms(r);
    }

    return 0;
}
#define SIN_C_MASK
#define ASIN_C_MASK
#define DELAY_MASK

#if 0
static int vibrator_pattern_enable(struct drv2605L_data *pDrv2605Ldata, unsigned int duty, unsigned int SIN_C, unsigned int delay_steps, unsigned int ANTI_SIN_C)
{
	int err = 0;
	unsigned int sin_usec, anti_sin_usec, T_time, tdelay_usec;
	struct drv2605_platform_data *pDrv2605Platdata = &pDrv2605Ldata->PlatData;
	T_time = 1000000/pDrv2605Platdata->actuator.LRAFreq;
	sin_usec = SIN_C * T_time;
	anti_sin_usec = ANTI_SIN_C * T_time;
	/*one delay_step meam half cycle*/
	tdelay_usec = delay_steps*T_time/2;
	
	printk(KERN_DEBUG"%s: sin_usec(%d), delay (%d), anti_sin__usec(%d)\n", __func__, sin_usec, tdelay_usec, anti_sin_usec);
	
	drv2605L_set_rtp_val(pDrv2605Ldata, 0x7f);
	drv2605L_change_mode(pDrv2605Ldata, WORK_RTP, DEV_READY);
	//mdelay(sin_usec/1000); // output duration : (1 / 150) * 3 = 20ms
	udelay(sin_usec);
	
	drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_READY);
	udelay(tdelay_usec);
	
	drv2605L_set_rtp_val(pDrv2605Ldata, 0x81);
	drv2605L_change_mode(pDrv2605Ldata, WORK_RTP, DEV_READY);
	//mdelay(anti_sin__usec/1000); // output duration : (1 / 150)  = 6.667ms
	udelay(anti_sin_usec);
	
	drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_READY);
	
	return err;
}
#endif
static void vibrator_pattern_enable_peek2(struct drv2605L_data *pDrv2605Ldata)
{	
	int ready_status,retry_cnt=0;
	//printk(KERN_ERR"%s \n", __func__);
	//drv2605L_reg_write(pDrv2605Ldata,0x01,0x85);
	drv2605L_reg_write(pDrv2605Ldata,0x01,0x80);
	for (retry_cnt = 0; retry_cnt < 50; retry_cnt++) {
	    mdelay(1);
	    ready_status=drv2605L_reg_read(pDrv2605Ldata, 0x01);
	    if (ready_status != 0x40)
	        printk(KERN_ERR" reg 0x01=%x, retry \n", ready_status);
	    else
	        break;
	}

	drv2605L_reg_write(pDrv2605Ldata,0x01,0x05);
	drv2605L_reg_write(pDrv2605Ldata,0x1a,0x80);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x55); //set 1/3 max voltage
	drv2605L_reg_write(pDrv2605Ldata,0x1d,0x81);
	//pulse1 +  12ms
	drv2605L_reg_write(pDrv2605Ldata,0x20,0x31);
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(12);
	//pulse2 +  10ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(10);
	//pulse3 +  7ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x2A); //set 1/6 max voltage
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(7);
	//pulse4 +  7ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x15); //set 1/12  max voltage
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(7);
	//pulse5 +  5ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x02); //set 1 % max voltage
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(5);
	//STOP
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x00);

	//restore reg value
	drv2605L_reg_write(pDrv2605Ldata,0x01,0x40);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x3E);
	drv2605L_reg_write(pDrv2605Ldata,0x1a,0xA8);
	drv2605L_reg_write(pDrv2605Ldata,0x1d,0x80);
	drv2605L_reg_write(pDrv2605Ldata,0x20,0x43);
	//drv2605L_reg_write(pDrv2605Ldata,0x21,0xC9);
}
static void vibrator_pattern_enable_peek(struct drv2605L_data *pDrv2605Ldata)
{	
	int ready_status,retry_cnt=0;
    	//printk(KERN_ERR"%s \n", __func__);
                #if 0
 	drv2605L_reg_write(pDrv2605Ldata, 0x1a, 0x80);
	drv2605L_reg_write(pDrv2605Ldata,0x1d,0x81);
	drv2605L_reg_write(pDrv2605Ldata,0x20,0xff);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0xff);
	drv2605L_reg_write(pDrv2605Ldata,0x01,0x05);
	//pulse1 +  2.5ms
 	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(2);
 	udelay(500);
	//pulse2 -  2.2ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(2);
 	udelay(200);
	//pulse3 +  2.7ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(2);
 	udelay(700);
	//pulse4 +  1.0ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
 	udelay(1000);
	//pulse5 +  2.0ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(2);
	//pulse6 +  2.2ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(2);
 	udelay(200);
	//pulse7 +  1.6ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(1);
 	udelay(600);
	//STOP
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x00);

            #endif
	//drv2605L_reg_write(pDrv2605Ldata,0x01,0x85);
	drv2605L_reg_write(pDrv2605Ldata,0x01,0x80);
	for (retry_cnt = 0; retry_cnt < 50; retry_cnt++) {
	    mdelay(1);
	    ready_status=drv2605L_reg_read(pDrv2605Ldata, 0x01);
	    if (ready_status != 0x40)
	        printk(KERN_ERR" reg 0x01=%x, retry \n", ready_status);
	    else
	        break;
	}

	drv2605L_reg_write(pDrv2605Ldata,0x01,0x05);
	drv2605L_reg_write(pDrv2605Ldata,0x1a,0x80);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0xff);
	drv2605L_reg_write(pDrv2605Ldata,0x1d,0x81);
	//pulse1 +  12ms
	drv2605L_reg_write(pDrv2605Ldata,0x20,0x31);
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(12);
	//pulse2 +  10ms
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(10);
	//pulse3 +  7ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x7f);
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x7f);
	mdelay(7);
	//pulse4 +  7ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x3f);
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(7);
	//pulse5 +  5ms
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x08);
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x81);
	mdelay(5);
	//STOP
	drv2605L_reg_write(pDrv2605Ldata,0x02,0x00);

	//restore reg value
	drv2605L_reg_write(pDrv2605Ldata,0x01,0x40);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x3E);
	drv2605L_reg_write(pDrv2605Ldata,0x1a,0xA8);
	drv2605L_reg_write(pDrv2605Ldata,0x1d,0x80);
	drv2605L_reg_write(pDrv2605Ldata,0x20,0x43);
	//drv2605L_reg_write(pDrv2605Ldata,0x21,0xC9);
}

static void vibrator_set_normal_vibration_B2N(struct drv2605L_data *pDrv2605Ldata)
{
	printk("into vibrator_set_normal_vibration_B2N\n");
	drv2605L_reg_write(pDrv2605Ldata,0x1D,0x81);
	drv2605L_reg_write(pDrv2605Ldata,0x20,0x31);
	drv2605L_reg_write(pDrv2605Ldata,0x17,0x5C);
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	struct drv2605L_data *pDrv2605Ldata = container_of(dev, struct drv2605L_data, to_dev);

	pDrv2605Ldata->should_stop = YES;	
	hrtimer_cancel(&pDrv2605Ldata->timer);
	cancel_work_sync(&pDrv2605Ldata->vibrator_work);

	mutex_lock(&pDrv2605Ldata->lock);
	
	drv2605L_stop(pDrv2605Ldata);

	if (value > 0) {

		if (value ==16){
		    drv2605L_reg_write(pDrv2605Ldata,0x17,0xDF);
		    value=6;
		}
		//printk(KERN_ERR" reg 0x17=%x \n", drv2605L_reg_read(pDrv2605Ldata, 0x17));
		if(pDrv2605Ldata->audio_haptics_enabled == NO){
			wake_lock(&pDrv2605Ldata->wklock);
		}
#if 1	/*use RTP vibrator*/
		drv2605L_set_rtp_val(pDrv2605Ldata, 0x7f);
		//drv2605L_reg_write(pDrv2605Ldata,0x17,0x3E);  // modify OD_CLAMP to control voltage
		drv2605L_reg_write(pDrv2605Ldata,0x1b,0x96); //control output Waveform frequency
		drv2605L_change_mode(pDrv2605Ldata, WORK_RTP, DEV_READY);
        //now, device in ready mode set to RTP mode

		vibrator_set_normal_vibration_B2N(pDrv2605Ldata);
#else
		drv2605L_change_mode(pDrv2605Ldata, WORK_VIBRATOR, DEV_READY);
#endif
		pDrv2605Ldata->vibrator_is_playing = YES;
		switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_RTP_PLAYBACK);			

		value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
        hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }

	mutex_unlock(&pDrv2605Ldata->lock);
}

static void fih_vibrator_enable( struct timed_output_dev *dev, int value)
{
	//int wave_num;
	//unsigned int SIN_C, delay_steps, ANTI_SIN_C, duty=0;
	//printk(KERN_ERR"%s,value=%d \n", __FUNCTION__,value);

	if(value>=0)
		vibrator_enable(dev, value);
        #if 0
	if(value<0){
		wave_num = -1*value;
		if(wave_num==999)
		{
			vibrator_pattern_enable_peek(pDrv2605Ldata);
		}
		else
		{
			SIN_C = (wave_num/100)%10;
			delay_steps = (wave_num/10)%10;
			ANTI_SIN_C = (wave_num)%10;
			printk(KERN_DEBUG"%s: wave_num(%d:%d:%d:%d)\n", __func__, duty, SIN_C, delay_steps, ANTI_SIN_C);
			if(SIN_C > 0)
				vibrator_pattern_enable(pDrv2605Ldata, duty, SIN_C, delay_steps, ANTI_SIN_C);
			else
				printk(KERN_ERR"%s: wrong pattern enable(%d:%d:%d:%d)\n", __func__, duty, SIN_C, delay_steps, ANTI_SIN_C);
		}
	}
        #endif
}
static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2605L_data *pDrv2605Ldata = container_of(timer, struct drv2605L_data, timer);

    schedule_work(&pDrv2605Ldata->vibrator_work);
	
    return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2605L_data *pDrv2605Ldata = container_of(work, struct drv2605L_data, vibrator_work);
	//printk(KERN_ERR"%s ,work_mode = %x \n", __func__,pDrv2605Ldata->work_mode);
	mutex_lock(&pDrv2605Ldata->lock);
	if((pDrv2605Ldata->work_mode == WORK_VIBRATOR)
		||(pDrv2605Ldata->work_mode == WORK_RTP)){
	       //printk(KERN_ERR"%s call vibrator_off\n", __func__);
		vibrator_off(pDrv2605Ldata);
	}else if(pDrv2605Ldata->work_mode == WORK_SEQ_PLAYBACK){
		play_effect(pDrv2605Ldata);
	}else if((pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_ON)||(pDrv2605Ldata->work_mode == WORK_PATTERN_RTP_OFF)){
		play_Pattern_RTP(pDrv2605Ldata);
	}else if((pDrv2605Ldata->work_mode == WORK_SEQ_RTP_ON)||(pDrv2605Ldata->work_mode == WORK_SEQ_RTP_OFF)){
		play_Seq_RTP(pDrv2605Ldata);
	}
	mutex_unlock(&pDrv2605Ldata->lock);
}

static void vibrator_pattern_work_routine(struct work_struct *work)
{
	struct drv2605L_data *pDrv2605Ldata = container_of(work, struct drv2605L_data, vibrator_pattern_work);
	int pattern_value=0;
	pattern_value = vibrator_get_pattern_value();
	//printk("vibrator_pattern_work_routine pattern_value = %d \n",pattern_value);
	mutex_lock(&pDrv2605Ldata->lock);
	if(pattern_value==1)
	{
		vibrator_pattern_enable_peek2(pDrv2605Ldata);
	}
	else if(pattern_value==999)
	{
		vibrator_pattern_enable_peek(pDrv2605Ldata);
	}
	mutex_unlock(&pDrv2605Ldata->lock);
	//printk("vibrator_pattern_work_routine pattern_value Done\n");
}

static int dev2605L_open (struct inode * i_node, struct file * filp)
{
	if(pDRV2605Ldata == NULL){
		return -ENODEV;
	}

	filp->private_data = pDRV2605Ldata;
	return 0;
}

static ssize_t dev2605L_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2605L_data *pDrv2605Ldata = (struct drv2605L_data *)filp->private_data;
	int ret = 0;

	if(pDrv2605Ldata->ReadLen > 0){
		ret = copy_to_user(buff, pDrv2605Ldata->ReadBuff, pDrv2605Ldata->ReadLen);
		if (ret != 0){
			printk(KERN_ERR"%s, copy_to_user err=%d \n", __FUNCTION__, ret);
		}else{
			ret = pDrv2605Ldata->ReadLen;
		}
		pDrv2605Ldata->ReadLen = 0;
	}
	
    return ret;
}

static bool isforDebug(int cmd){
	return ((cmd == HAPTIC_CMDID_REG_WRITE)
		||(cmd == HAPTIC_CMDID_REG_READ)
		||(cmd == HAPTIC_CMDID_REG_SETBIT));
}

static ssize_t dev2605L_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2605L_data *pDrv2605Ldata = (struct drv2605L_data *)filp->private_data;
	
	if(isforDebug(buff[0])){
	}else{
		pDrv2605Ldata->should_stop = YES;	
		hrtimer_cancel(&pDrv2605Ldata->timer);
		cancel_work_sync(&pDrv2605Ldata->vibrator_work);
	}
	
    mutex_lock(&pDrv2605Ldata->lock);
	
	if(isforDebug(buff[0])){
	}else{
		drv2605L_stop(pDrv2605Ldata);
	}
	
    switch(buff[0])
    {
        case HAPTIC_CMDID_PLAY_SINGLE_EFFECT:
        case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE:
		{	
            memset(&pDrv2605Ldata->sequence, 0, WAVEFORM_SEQUENCER_MAX);
            if (!copy_from_user(&pDrv2605Ldata->sequence, &buff[1], len - 1))
            {
				if(pDrv2605Ldata->audio_haptics_enabled == NO){
					wake_lock(&pDrv2605Ldata->wklock);
				}
				pDrv2605Ldata->should_stop = NO;
				drv2605L_change_mode(pDrv2605Ldata, WORK_SEQ_PLAYBACK, DEV_IDLE);
                schedule_work(&pDrv2605Ldata->vibrator_work);
            }
            break;
        }
        case HAPTIC_CMDID_PLAY_TIMED_EFFECT:
        {	
            unsigned int value = 0;
            value = buff[2];
            value <<= 8;
            value |= buff[1];
		
            if (value > 0)
            {
				if(pDrv2605Ldata->audio_haptics_enabled == NO){			
					wake_lock(&pDrv2605Ldata->wklock);
				}
				switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_RTP_PLAYBACK);
				pDrv2605Ldata->vibrator_is_playing = YES;
  				value = (value > MAX_TIMEOUT)?MAX_TIMEOUT:value;
				drv2605L_change_mode(pDrv2605Ldata, WORK_RTP, DEV_READY);
				
				hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            }
            break;
        }

       case HAPTIC_CMDID_PATTERN_RTP:
        {
			unsigned char strength = 0;

			pDrv2605Ldata->vibration_time = (int)((((int)buff[2])<<8) | (int)buff[1]);
			pDrv2605Ldata->silience_time = (int)((((int)buff[4])<<8) | (int)buff[3]);
			strength = buff[5];
			pDrv2605Ldata->repeat_times = buff[6];
			
            if(pDrv2605Ldata->vibration_time > 0){
				if(pDrv2605Ldata->audio_haptics_enabled == NO){
					wake_lock(&pDrv2605Ldata->wklock);			
				}
				switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_RTP_PLAYBACK);
				pDrv2605Ldata->vibrator_is_playing = YES;
                if(pDrv2605Ldata->repeat_times > 0)
					pDrv2605Ldata->repeat_times--;
                if (pDrv2605Ldata->vibration_time > MAX_TIMEOUT)
                    pDrv2605Ldata->vibration_time = MAX_TIMEOUT;
				drv2605L_change_mode(pDrv2605Ldata, WORK_PATTERN_RTP_ON, DEV_READY);
				drv2605L_set_rtp_val(pDrv2605Ldata, strength);
				
                hrtimer_start(&pDrv2605Ldata->timer, ns_to_ktime((u64)pDrv2605Ldata->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            }
            break;
        }		
 		
		case HAPTIC_CMDID_RTP_SEQUENCE:
		{
            memset(&pDrv2605Ldata->RTPSeq, 0, sizeof(struct RTP_Seq));
			if(((len-1)%2) == 0){
				pDrv2605Ldata->RTPSeq.RTPCounts = (len-1)/2;
				if((pDrv2605Ldata->RTPSeq.RTPCounts <= MAX_RTP_SEQ)&&(pDrv2605Ldata->RTPSeq.RTPCounts>0)){
					if(copy_from_user(pDrv2605Ldata->RTPSeq.RTPData, &buff[1], pDrv2605Ldata->RTPSeq.RTPCounts*2) != 0){
						printk(KERN_ERR"%s, rtp_seq copy seq err\n", __FUNCTION__);	
						break;
					}
					
					if(pDrv2605Ldata->audio_haptics_enabled == NO){
						wake_lock(&pDrv2605Ldata->wklock);
					}
					switch_set_state(&pDrv2605Ldata->sw_dev, SW_STATE_RTP_PLAYBACK);
					drv2605L_change_mode(pDrv2605Ldata, WORK_SEQ_RTP_OFF, DEV_IDLE);
					schedule_work(&pDrv2605Ldata->vibrator_work);
				}else{
					printk(KERN_ERR"%s, rtp_seq count error,maximum=%d\n", __FUNCTION__,MAX_RTP_SEQ);
				}
			}else{
				printk(KERN_ERR"%s, rtp_seq len error\n", __FUNCTION__);
			}
			break;
		}
		
        case HAPTIC_CMDID_STOP:
        {
            break;
        }
		
        case HAPTIC_CMDID_AUDIOHAPTIC_ENABLE:
        {
			if(pDrv2605Ldata->audio_haptics_enabled == NO){
				wake_lock(&pDrv2605Ldata->wklock);
			}
			pDrv2605Ldata->audio_haptics_enabled = YES;
			setAudioHapticsEnabled(pDrv2605Ldata, YES);
            break;
        }
		
        case HAPTIC_CMDID_AUDIOHAPTIC_DISABLE:
        {
			if(pDrv2605Ldata->audio_haptics_enabled == YES){
				pDrv2605Ldata->audio_haptics_enabled = NO;
				wake_unlock(&pDrv2605Ldata->wklock);	
			}
            break;
        }
		
		case HAPTIC_CMDID_REG_READ:
		{
			if(len == 2){
				pDrv2605Ldata->ReadLen = 1;
				pDrv2605Ldata->ReadBuff[0] = drv2605L_reg_read(pDrv2605Ldata, buff[1]);
			}else if(len == 3){
				pDrv2605Ldata->ReadLen = (buff[2]>MAX_READ_BYTES)?MAX_READ_BYTES:buff[2];
				drv2605L_bulk_read(pDrv2605Ldata, buff[1], pDrv2605Ldata->ReadLen, pDrv2605Ldata->ReadBuff);
			}else{
				printk(KERN_ERR"%s, reg_read len error\n", __FUNCTION__);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_WRITE:
		{
			if((len-1) == 2){
				drv2605L_reg_write(pDrv2605Ldata, buff[1], buff[2]);	
			}else if((len-1)>2){
				unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
				if(data != NULL){
					if(copy_from_user(data, &buff[2], len-2) != 0){
						printk(KERN_ERR"%s, reg copy err\n", __FUNCTION__);	
					}else{
						drv2605L_bulk_write(pDrv2605Ldata, buff[1], len-2, data);
					}
					kfree(data);
				}
			}else{
				printk(KERN_ERR"%s, reg_write len error\n", __FUNCTION__);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_SETBIT:
		{
			int i=1;			
			for(i=1; i< len; ){
				drv2605L_set_bits(pDrv2605Ldata, buff[i], buff[i+1], buff[i+2]);
				i += 3;
			}
			break;
		}		
    default:
		printk(KERN_ERR"%s, unknown HAPTIC cmd\n", __FUNCTION__);
      break;
    }

    mutex_unlock(&pDrv2605Ldata->lock);

    return len;
}


static struct file_operations fops =
{
	.open = dev2605L_open,
    .read = dev2605L_read,
    .write = dev2605L_write,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void drv2605L_early_suspend(struct early_suspend *h){
	struct drv2605L_data *pDrv2605Ldata = container_of(h, struct drv2605L_data, early_suspend); 

	pDrv2605Ldata->should_stop = YES;	
	hrtimer_cancel(&pDrv2605Ldata->timer);
	cancel_work_sync(&pDrv2605Ldata->vibrator_work);
	
	mutex_lock(&pDrv2605Ldata->lock);	
	
	drv2605L_stop(pDrv2605Ldata);
	if(pDrv2605Ldata->audio_haptics_enabled == YES){
		wake_unlock(&pDrv2605Ldata->wklock);
	}
	
	mutex_unlock(&pDrv2605Ldata->lock);
    return ;
}

void drv2605L_late_resume(struct early_suspend *h) {
	struct drv2605L_data *pDrv2605Ldata = container_of(h, struct drv2605L_data, early_suspend); 
	
	mutex_lock(&pDrv2605Ldata->lock);	
	if(pDrv2605Ldata->audio_haptics_enabled == YES){
		wake_lock(&pDrv2605Ldata->wklock);
		setAudioHapticsEnabled(pDrv2605Ldata, YES);
	}
	mutex_unlock(&pDrv2605Ldata->lock);
    return ; 
 }
 #endif
 
static int Haptics_init(struct drv2605L_data *pDrv2605Ldata)
{
    int reval = -ENOMEM;
   
    pDrv2605Ldata->version = MKDEV(0,0);
    reval = alloc_chrdev_region(&pDrv2605Ldata->version, 0, 1, HAPTICS_DEVICE_NAME);
    if (reval < 0)
    {
        printk(KERN_ALERT"drv2605: error getting major number %d\n", reval);
        goto fail0;
    }

    pDrv2605Ldata->class = class_create(THIS_MODULE, HAPTICS_DEVICE_NAME);
    if (!pDrv2605Ldata->class)
    {
        printk(KERN_ALERT"drv2605: error creating class\n");
        goto fail1;
    }

    pDrv2605Ldata->device = device_create(pDrv2605Ldata->class, NULL, pDrv2605Ldata->version, NULL, HAPTICS_DEVICE_NAME);
    if (!pDrv2605Ldata->device)
    {
        printk(KERN_ALERT"drv2605: error creating device 2605L\n");
        goto fail2;
    }

    cdev_init(&pDrv2605Ldata->cdev, &fops);
    pDrv2605Ldata->cdev.owner = THIS_MODULE;
    pDrv2605Ldata->cdev.ops = &fops;
    reval = cdev_add(&pDrv2605Ldata->cdev, pDrv2605Ldata->version, 1);
    if (reval)
    {
        printk(KERN_ALERT"drv2605: fail to add cdev\n");
        goto fail3;
    }

	pDrv2605Ldata->sw_dev.name = "haptics";
	reval = switch_dev_register(&pDrv2605Ldata->sw_dev);
	if (reval < 0) {
		printk(KERN_ALERT"drv2605: fail to register switch\n");
		goto fail4;
	}	
	
	pDrv2605Ldata->to_dev.name = "vibrator";
	pDrv2605Ldata->to_dev.get_time = vibrator_get_time;
	pDrv2605Ldata->to_dev.enable = fih_vibrator_enable;//vibrator_enable;

    if (timed_output_dev_register(&(pDrv2605Ldata->to_dev)) < 0)
    {
        printk(KERN_ALERT"drv2605: fail to create timed output dev\n");
        goto fail3;
    }
	if(pDrv2605Ldata->PlatData.support_pattern==true)
	{
		int i=0;
		printk("support_pattern is true create virtual file\n");
		for (i = 0; i < ARRAY_SIZE(qpnp_hap_attrs); i++)
		{
				int rc = sysfs_create_file(&(pDrv2605Ldata->to_dev).dev->kobj,&qpnp_hap_attrs[i].attr);
				if (rc < 0)
					pr_err("sysfs creation failed\n");
		}
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
    pDrv2605Ldata->early_suspend.suspend = drv2605L_early_suspend;
	pDrv2605Ldata->early_suspend.resume = drv2605L_late_resume;
	pDrv2605Ldata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	register_early_suspend(&pDrv2605Ldata->early_suspend);
#endif

    hrtimer_init(&pDrv2605Ldata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pDrv2605Ldata->timer.function = vibrator_timer_func;
    INIT_WORK(&pDrv2605Ldata->vibrator_work, vibrator_work_routine);
    INIT_WORK(&pDrv2605Ldata->vibrator_pattern_work, vibrator_pattern_work_routine);
    wake_lock_init(&pDrv2605Ldata->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pDrv2605Ldata->lock);

    return 0;

fail4:
	switch_dev_unregister(&pDrv2605Ldata->sw_dev);
fail3:
	device_destroy(pDrv2605Ldata->class, pDrv2605Ldata->version);
fail2:
    class_destroy(pDrv2605Ldata->class);	
fail1:
    unregister_chrdev_region(pDrv2605Ldata->version, 1);	
fail0:
    return reval;
}

static void dev_init_platform_data(struct drv2605L_data *pDrv2605Ldata)
{
	struct drv2605_platform_data *pDrv2605Platdata = &pDrv2605Ldata->PlatData;
	struct actuator_data actuator = pDrv2605Platdata->actuator;
	struct audio2haptics_data a2h = pDrv2605Platdata->a2h;
	unsigned char temp = 0;

	drv2605L_select_library(pDrv2605Ldata, actuator.g_effect_bank);
	
	//OTP memory saves data from 0x16 to 0x1a
	if(pDrv2605Ldata->OTP == 0) {
		if(actuator.rated_vol != 0){
			drv2605L_reg_write(pDrv2605Ldata, RATED_VOLTAGE_REG, actuator.rated_vol);
		}else{
			printk(KERN_ERR"%s, ERROR Rated ZERO\n", __FUNCTION__);
		}

		if(actuator.over_drive_vol != 0){
			drv2605L_reg_write(pDrv2605Ldata, OVERDRIVE_CLAMP_VOLTAGE_REG, actuator.over_drive_vol);
		}else{
			printk(KERN_ERR"%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
		}
		
		drv2605L_set_bits(pDrv2605Ldata, 
						FEEDBACK_CONTROL_REG, 
						FEEDBACK_CONTROL_DEVICE_TYPE_MASK
							|FEEDBACK_CONTROL_FB_BRAKE_MASK 
							|FEEDBACK_CONTROL_LOOP_GAIN_MASK,
						(((actuator.device_type == LRA)?FEEDBACK_CONTROL_MODE_LRA:FEEDBACK_CONTROL_MODE_ERM)
							|FB_BRAKE_FACTOR
							|LOOP_GAIN)
						);
	}else{
		printk(KERN_ERR"%s, OTP programmed\n", __FUNCTION__);
	}
	
	if(actuator.device_type == LRA){
		unsigned char DriveTime = 5*(1000 - actuator.LRAFreq)/actuator.LRAFreq;
		drv2605L_set_bits(pDrv2605Ldata, 
				Control1_REG, 
				Control1_REG_DRIVE_TIME_MASK, 
				DriveTime);	
		printk(KERN_ERR"%s, LRA = %d, DriveTime=0x%x\n", __FUNCTION__, actuator.LRAFreq, DriveTime);
	}
	
	if(pDrv2605Platdata->loop == OPEN_LOOP){
		temp = BIDIR_INPUT_BIDIRECTIONAL;
	}else{
		if(pDrv2605Platdata->BIDIRInput == UniDirectional){
			temp = BIDIR_INPUT_UNIDIRECTIONAL;
		}else{
			temp = BIDIR_INPUT_BIDIRECTIONAL;
		}
	}

	drv2605L_set_bits(pDrv2605Ldata, 
				Control2_REG, 
				Control2_REG_BIDIR_INPUT_MASK|BLANKING_TIME_MASK|IDISS_TIME_MASK, 
				temp|BLANKING_TIME|IDISS_TIME);	
				
	if((pDrv2605Platdata->loop == CLOSE_LOOP)&&(actuator.device_type == LRA))
	{
		drv2605L_set_bits(pDrv2605Ldata, 
				Control2_REG, 
				AUTO_RES_SAMPLE_TIME_MASK, 
				AUTO_RES_SAMPLE_TIME_300us);
	}
		
	if((pDrv2605Platdata->loop == OPEN_LOOP)&&(actuator.device_type == LRA))
	{
		temp = LRA_OpenLoop_Enabled;
	}
	else if((pDrv2605Platdata->loop == OPEN_LOOP)&&(actuator.device_type == ERM))
	{
		temp = ERM_OpenLoop_Enabled;
	}
	else
	{
		temp = ERM_OpenLoop_Disable|LRA_OpenLoop_Disable;
	}

	if((pDrv2605Platdata->loop == CLOSE_LOOP) &&(pDrv2605Platdata->BIDIRInput == UniDirectional))
	{
		temp |= RTP_FORMAT_UNSIGNED;
		drv2605L_reg_write(pDrv2605Ldata, REAL_TIME_PLAYBACK_REG, 0xff);
	}
	else
	{
		if(pDrv2605Platdata->RTPFormat == Signed)
		{
			temp |= RTP_FORMAT_SIGNED;
			drv2605L_reg_write(pDrv2605Ldata, REAL_TIME_PLAYBACK_REG, 0x7f);
		}
		else
		{
			temp |= RTP_FORMAT_UNSIGNED;
			drv2605L_reg_write(pDrv2605Ldata, REAL_TIME_PLAYBACK_REG, 0xff);
		}
	}
	drv2605L_set_bits(pDrv2605Ldata, 
					Control3_REG, 
					Control3_REG_LOOP_MASK|Control3_REG_FORMAT_MASK, 
					temp);	

	drv2605L_set_bits(pDrv2605Ldata, 
			Control4_REG, 
			Control4_REG_CAL_TIME_MASK|Control4_REG_ZC_DET_MASK,
			AUTO_CAL_TIME|ZC_DET_TIME
			);
										
	drv2605L_set_bits(pDrv2605Ldata, 
			Control5_REG, 
			BLANK_IDISS_MSB_MASK,
			BLANK_IDISS_MSB_CLEAR
			);
			
	if(actuator.device_type == LRA)
	{
		/* please refer to the equations in DRV2604L data sheet */
		unsigned int temp = 9846 * actuator.LRAFreq;
		unsigned char R20 = (unsigned char)(100000000 / temp); 
		drv2605L_reg_write(pDrv2605Ldata, LRA_OPENLOOP_PERIOD_REG, R20);
	}
	//for audio to haptics
	if(pDrv2605Platdata->GpioTrigger == 0)	//not used as external trigger
	{
		drv2605L_reg_write(pDrv2605Ldata, AUDIO_HAPTICS_MIN_INPUT_REG, a2h.a2h_min_input);
		drv2605L_reg_write(pDrv2605Ldata, AUDIO_HAPTICS_MAX_INPUT_REG, a2h.a2h_max_input);
		drv2605L_reg_write(pDrv2605Ldata, AUDIO_HAPTICS_MIN_OUTPUT_REG, a2h.a2h_min_output);
		drv2605L_reg_write(pDrv2605Ldata, AUDIO_HAPTICS_MAX_OUTPUT_REG, a2h.a2h_max_output);
	}
}

#ifdef CONFIG_OF
static int drv2605L_parse_dt(struct device *dev,
			    struct drv2605_platform_data *pDrv2605Platdata)
{
	struct device_node *np = dev->of_node;
	unsigned int use_enable, use_trigger;
	int error;
	int pattern_enable=0;


	use_enable = of_property_read_bool(np, "ti,use-enable");

	if(use_enable)
		pDrv2605Platdata->GpioEnable = of_get_named_gpio_flags(np, "ti,enable-gpio", 0, NULL);
	else
		pDrv2605Platdata->GpioEnable = 0;

	dev_err(dev, "%s: dts: ti,enable-gpio=%d \n", __func__, pDrv2605Platdata->GpioEnable);

	use_trigger = of_property_read_bool(np, "ti,use-interrupt");

	if(use_trigger)
		pDrv2605Platdata->GpioTrigger = of_get_named_gpio_flags(np, "ti,trigger-gpio", 0, NULL);
	else
		pDrv2605Platdata->GpioTrigger = 0 ;

	dev_err(dev, "%s: dts: ti,trigger-gpio=%d \n", __func__, pDrv2605Platdata->GpioTrigger);

	error = of_property_read_u32(np, "loop-mode", &pDrv2605Platdata->loop);
	if (error) {
		dev_err(dev, "%s: No entry for loop-mode\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: loop-mode=%d \n", __func__, pDrv2605Platdata->loop);

	error = of_property_read_u32(np, "rtp-format", &pDrv2605Platdata->RTPFormat);
	if (error) {
		dev_err(dev, "%s: No entry for rtp-format", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: rtp-format=%d \n", __func__, pDrv2605Platdata->BIDIRInput);

	error = of_property_read_u32(np, "bidir-input", &pDrv2605Platdata->BIDIRInput);
	if (error) {
		dev_err(dev, "%s: No entry for bidir-input\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: bidir-input=%d \n", __func__, pDrv2605Platdata->BIDIRInput);

	error = of_property_read_u32(np, "actuator-device-type", &pDrv2605Platdata->actuator.device_type);
	if (error) {
		dev_err(dev, "%s: No entry for actuator-device-type\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: actuator-device-type=%d \n", __func__, pDrv2605Platdata->actuator.device_type);

	error = of_property_read_u8(np, "actuator-rated-voltage", &pDrv2605Platdata->actuator.rated_vol);
	if (error) {
		dev_err(dev, "%s: No entry for actuator-rated-voltage\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: actuator-rated-voltage=%d \n", __func__, pDrv2605Platdata->actuator.rated_vol);

	error = of_property_read_u8(np, "actuator-library-sel", &pDrv2605Platdata->actuator.g_effect_bank);
	if (error) {
		dev_err(dev, "%s: No entry for actuator-library-sel\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: actuator-library-sel=%d \n", __func__, pDrv2605Platdata->actuator.g_effect_bank);

	error = of_property_read_u8(np, "actuator-overdrive-voltage", &pDrv2605Platdata->actuator.over_drive_vol);
	if (error) {
		dev_err(dev, "%s: No entry for actuator-overdrive-voltage\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: actuator-overdrive-voltage=%d \n", __func__, pDrv2605Platdata->actuator.over_drive_vol);

	error = of_property_read_u8(np, "actuator-lra-freq", &pDrv2605Platdata->actuator.LRAFreq);
	if (error) {
		dev_err(dev, "%s: No entry for actuator-lra-freq\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: actuator-lra-freq=%d \n", __func__, pDrv2605Platdata->actuator.LRAFreq);

	error = of_property_read_u8(np, "a2h-min-input-vol", &pDrv2605Platdata->a2h.a2h_min_input);
	if (error) {
		dev_err(dev, "%s: No entry for a2h-min-input-vol\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: a2h-min-input-vol=%d \n", __func__, pDrv2605Platdata->a2h.a2h_min_input);

	error = of_property_read_u8(np, "a2h-max-input-vol", &pDrv2605Platdata->a2h.a2h_max_input);
	if (error) {
		dev_err(dev, "%s: No entry for a2h-max-input-vol\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: a2h-max-input-vol=%d \n", __func__, pDrv2605Platdata->a2h.a2h_max_input);

	error = of_property_read_u8(np, "a2h-min-output-vol", &pDrv2605Platdata->a2h.a2h_min_output);
	if (error) {
		dev_err(dev, "%s: No entry for a2h-min-output-vol\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: a2h-min-output-vol=%d \n", __func__, pDrv2605Platdata->a2h.a2h_min_output);

	error = of_property_read_u8(np, "a2h-max-output-vol", &pDrv2605Platdata->a2h.a2h_max_output);
	if (error) {
		dev_err(dev, "%s: No entry for a2h-max-output-vol\n", __func__);
		return error;
	}

	dev_err(dev, "%s: dts: a2h-max-output-vol=%d \n", __func__, pDrv2605Platdata->a2h.a2h_max_output);
	
	pattern_enable = of_property_read_bool(np, "fih,enable-pattern");

	if(pattern_enable)
		pDrv2605Platdata->support_pattern = true;
	else
		pDrv2605Platdata->support_pattern = false;
	
	printk("support pattern is %d\n",pDrv2605Platdata->support_pattern);

	return error;
}
#endif
#if 0
static int dev_auto_calibrate(struct drv2605L_data *pDrv2605Ldata)
{
	int err = 0, status=0;
	
	drv2605L_change_mode(pDrv2605Ldata, WORK_CALIBRATION, DEV_READY);
	drv2605L_set_go_bit(pDrv2605Ldata, GO);
			
	/* Wait until the procedure is done */
	drv2605L_poll_go_bit(pDrv2605Ldata);
	/* Read status */
	status = drv2605L_reg_read(pDrv2605Ldata, STATUS_REG);

	printk(KERN_ERR"%s, calibration status =0x%x\n", __FUNCTION__, status);

	/* Read calibration results */
	drv2605L_reg_read(pDrv2605Ldata, AUTO_CALI_RESULT_REG);
	drv2605L_reg_read(pDrv2605Ldata, AUTO_CALI_BACK_EMF_RESULT_REG);
	drv2605L_reg_read(pDrv2605Ldata, FEEDBACK_CONTROL_REG);
	
	return err;
}
#endif
static struct regmap_config drv2605L_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int drv2605L_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2605L_data *pDrv2605Ldata;
	struct drv2605_platform_data *pDrv2605Platdata = client->dev.platform_data;
	
	int err = 0;
	int status = 0;
	printk(KERN_ERR"%s \n", __FUNCTION__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR"%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2605Ldata = devm_kzalloc(&client->dev, sizeof(struct drv2605L_data), GFP_KERNEL);
	if (pDrv2605Ldata == NULL){
		printk(KERN_ERR"%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}
	
#ifdef CONFIG_OF
	if(!pDrv2605Platdata){
		pDrv2605Platdata = devm_kzalloc(&client->dev, sizeof(struct drv2605_platform_data), GFP_KERNEL);
		if (pDrv2605Platdata == NULL){
			printk(KERN_ERR"%s:no memory\n", __FUNCTION__);
			return -ENOMEM;
		}
	}

	drv2605L_parse_dt(&client->dev, pDrv2605Platdata);
#endif

	pDrv2605Ldata->regmap = devm_regmap_init_i2c(client, &drv2605L_i2c_regmap);
	if (IS_ERR(pDrv2605Ldata->regmap)) {
		err = PTR_ERR(pDrv2605Ldata->regmap);
		printk(KERN_ERR"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	memcpy(&pDrv2605Ldata->PlatData, pDrv2605Platdata, sizeof(struct drv2605_platform_data));
	i2c_set_clientdata(client,pDrv2605Ldata);

	if(pDrv2605Ldata->PlatData.GpioTrigger){
		err = gpio_request(pDrv2605Ldata->PlatData.GpioTrigger,HAPTICS_DEVICE_NAME"Trigger");
		if(err < 0){
			printk(KERN_ERR"%s: GPIO request Trigger error\n", __FUNCTION__);				
			goto exit_gpio_request_failed;
		}
	}

	if(pDrv2605Ldata->PlatData.GpioEnable){
		err = gpio_request(pDrv2605Ldata->PlatData.GpioEnable,HAPTICS_DEVICE_NAME"Enable");
		if(err < 0){
			printk(KERN_ERR"%s: GPIO request enable error\n", __FUNCTION__);					
			goto exit_gpio_request_failed;
		}

	    /* Enable power to the chip */
	    gpio_direction_output(pDrv2605Ldata->PlatData.GpioEnable, 1);

	    /* Wait 30 us */
	    udelay(30);
	}

	err = drv2605L_reg_read(pDrv2605Ldata, STATUS_REG);
	if(err < 0){
		printk(KERN_ERR"%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	}else{
		printk(KERN_ERR"%s, i2c status (0x%x)\n", __FUNCTION__, err);
		status = err;
	}
	/* Read device ID */
	pDrv2605Ldata->device_id = (status & DEV_ID_MASK);
	switch (pDrv2605Ldata->device_id)
	{
		case DRV2605_VER_1DOT1:
		printk(KERN_ERR"drv2605 driver found: drv2605 v1.1.\n");
		break;
		case DRV2605_VER_1DOT0:
		printk(KERN_ERR"drv2605 driver found: drv2605 v1.0.\n");
		break;
		case DRV2604:
		printk(KERN_ALERT"drv2605 driver found: drv2604.\n");
		break;
		case DRV2604L:
		printk(KERN_ALERT"drv2604 driver found: drv2604L.\n");
		break;
		case DRV2605L:
		printk(KERN_ALERT"drv2604 driver found: drv2605L.\n");
		break;		
		default:
		printk(KERN_ERR"drv2605 driver found: unknown.\n");
		break;
	}

	if(pDrv2605Ldata->device_id != DRV2605L)
	{
		printk(KERN_ERR"%s, status(0x%x),device_id(%d) fail\n",
			__FUNCTION__, status, pDrv2605Ldata->device_id);
		goto exit_gpio_request_failed;
	}

	drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_READY);
	schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
	
	pDrv2605Ldata->OTP = drv2605L_reg_read(pDrv2605Ldata, Control4_REG) & Control4_REG_OTP_MASK;
	
	dev_init_platform_data(pDrv2605Ldata);
	
    /*
	if(pDrv2605Ldata->OTP == 0){
		err = dev_auto_calibrate(pDrv2605Ldata);
		if(err < 0){
			printk(KERN_ERR"%s, ERROR, calibration fail\n",	__FUNCTION__);
		}
	}
*/
    /* Put hardware in standby */
    drv2605L_change_mode(pDrv2605Ldata, WORK_IDLE, DEV_STANDBY);

    Haptics_init(pDrv2605Ldata);
	
	pDRV2605Ldata = pDrv2605Ldata;
    printk(KERN_ERR"drv2605 probe succeeded\n");

    return 0;

exit_gpio_request_failed:
	if(pDrv2605Ldata->PlatData.GpioTrigger){
		gpio_free(pDrv2605Ldata->PlatData.GpioTrigger);
	}

	if(pDrv2605Ldata->PlatData.GpioEnable){
		gpio_free(pDrv2605Ldata->PlatData.GpioEnable);
	}
	
    printk(KERN_ERR"%s failed, err=%d\n",__FUNCTION__, err);
	return err;
}

static int drv2605L_remove(struct i2c_client* client)
{
	struct drv2605L_data *pDrv2605Ldata = i2c_get_clientdata(client);

    device_destroy(pDrv2605Ldata->class, pDrv2605Ldata->version);
    class_destroy(pDrv2605Ldata->class);
    unregister_chrdev_region(pDrv2605Ldata->version, 1);

	if(pDrv2605Ldata->PlatData.GpioTrigger)
		gpio_free(pDrv2605Ldata->PlatData.GpioTrigger);

	if(pDrv2605Ldata->PlatData.GpioEnable)
		gpio_free(pDrv2605Ldata->PlatData.GpioEnable);

#ifdef CONFIG_HAS_EARLYSUSPEND		
	unregister_early_suspend(&pDrv2605Ldata->early_suspend);
#endif
	
    printk(KERN_ALERT"drv2605 remove");
	
    return 0;
}

static struct i2c_device_id drv2605L_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, drv2605L_id_table);

#ifdef CONFIG_OF
static const struct of_device_id drv260x_of_match[] = {
	{ .compatible = "ti,drv2605l", },
	{ }
};
MODULE_DEVICE_TABLE(of, drv260x_of_match);
#endif

static struct i2c_driver drv2605L_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		    .owner = THIS_MODULE,
		    .of_match_table = of_match_ptr(drv260x_of_match),
    },
    .id_table = drv2605L_id_table,
    .probe = drv2605L_probe,
    .remove = drv2605L_remove,
};

static int __init drv2605L_init(void)
{
	return i2c_add_driver(&drv2605L_driver);
}

static void __exit drv2605L_exit(void)
{
	i2c_del_driver(&drv2605L_driver);
}

module_init(drv2605L_init);
module_exit(drv2605L_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
