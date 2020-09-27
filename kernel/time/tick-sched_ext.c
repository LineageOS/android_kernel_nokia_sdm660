/*
 * Copyright(C) 2011-2017 Foxconn International Holdings, Ltd. All rights reserved.
 * Copyright (C) 2011 Foxconn.  All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/percpu.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/rq_stats.h>

#include <asm/irq_regs.h>

#include "tick-internal.h"

#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
/* for linux pt_regs struct */
#include <linux/ptrace.h>
#include <linux/thermal.h>

/* total cpu number*/
#ifdef CONFIG_NR_CPUS
#define CPU_NUMS CONFIG_NR_CPUS
#else
#define CPU_NUMS 8
#endif

#define show_msg(fmt, args...)		pr_info(fmt, ##args)

static u64 next_checked_jiffies = 0;

static u64 last_checked_tick[CPU_NUMS] = {0};
static u64 last_busy_tick[CPU_NUMS]  = {0};
static u64 last_iowait_tick[CPU_NUMS]  = {0};

static unsigned int debug_cpu_usage_enable = 0;

#define CPU_USAGE_CHECK_INTERVAL_MS 2000  /* 2000ms */ 

static unsigned int debug_cpu_usage_interval = CPU_USAGE_CHECK_INTERVAL_MS;

const char *freq_tag = "FREQQ";
const char *cpu_usage_tag = "CPUSG";

#include <linux/workqueue.h>
struct delayed_work		update_tz_info_work;

#define TZ_INFO_INTERVAL_MS	10000 /* 10s */

#define ARM_TZ_INFO_WORK_CNT (TZ_INFO_INTERVAL_MS/CPU_USAGE_CHECK_INTERVAL_MS)

struct show_thermal_sensor
{ 
  char *alias; 
  char *type; 
  int divide; 
}; 

static struct show_thermal_sensor fih_therm_sensor_list[] = 
            { {"AOSS", "tsens_tz_sensor0", 10},
              {"CPUSS.1", "tsens_tz_sensor1", 10},
              {"CPUSS.0", "tsens_tz_sensor2", 10},
              {"CPU.1.0", "tsens_tz_sensor3", 10},
              {"CPU.1.1", "tsens_tz_sensor4", 10},
              {"CPU.1.2", "tsens_tz_sensor5", 10},
              {"CPU.1.3", "tsens_tz_sensor6", 10},
              {"CPUSS.2", "tsens_tz_sensor7", 10},
              {"GPUSS", "tsens_tz_sensor8", 10},
              {"VIDEO", "tsens_tz_sensor9", 10},
              {"MDM_CORE", "tsens_tz_sensor10", 10},
              {"CAMERA", "tsens_tz_sensor11", 10},
              {"QUIET", "quiet_therm", 1},
              {"BAT", "battery", 1000},
              {"PM660", "pm660_tz", 1000},
              {"PM660L", "pm660l_tz", 1000},
              {"XO", "xo_therm", 1},
              {"PA", "pa_therm0", 1},
              {"MSM", "msm_therm", 1},
              {"EMMC", "emmc_therm", 1},
            };

static void update_tz_info(struct work_struct *work)
{
    char buf[250];
    char *s = buf;
    int i =0;
    struct thermal_zone_device *tzd;
    int temperature,ret;

    buf[0] = '\0';
    for(i = 0; i < ARRAY_SIZE(fih_therm_sensor_list); i++) {
        tzd = thermal_zone_get_zone_by_name(fih_therm_sensor_list[i].type);

        if (!IS_ERR(tzd)) {
            ret = thermal_zone_get_temp(tzd, &temperature);
            
            if (!ret) {
		if (fih_therm_sensor_list[i].divide==10) {
	                s += snprintf(s, sizeof(buf) - (size_t)(s-buf), "%s=%d.%d ", 
                        fih_therm_sensor_list[i].alias, temperature/10, temperature%10);
		} else {
			temperature /= fih_therm_sensor_list[i].divide;
	                s += snprintf(s, sizeof(buf) - (size_t)(s-buf), "%s=%d ", 
                        fih_therm_sensor_list[i].alias, temperature);
		}
            }
        }
    }

    show_msg("TINFO:%s\n", buf);
}

static void get_cpu_usage(int cpu, long *usage, long *iowait_usage)
{
	u64 curr_tick = 0;
	u64 curr_busy_tick = 0;
	u64 curr_iowait_tick = 0;
	long total_tick = 0;
	long busy_tick = 0;
	long iowait_tick = 0;

	*usage = 0;
	*iowait_usage = 0;

	if (cpu >= CPU_NUMS) {
		return;
	}
	
	/* get the current time */
	curr_tick = get_jiffies_64();

	/* get this cpu's busy time */
	curr_busy_tick  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	curr_busy_tick += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	curr_busy_tick += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	curr_busy_tick += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	curr_busy_tick += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	curr_busy_tick += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	curr_iowait_tick =  kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
		
	/* Calculate the time interval */
	total_tick = curr_tick - last_checked_tick[cpu];
	busy_tick = curr_busy_tick - last_busy_tick[cpu];
	iowait_tick = curr_iowait_tick - last_iowait_tick[cpu];

	/* record last current and busy time */
	last_busy_tick[cpu] = curr_busy_tick;
	last_checked_tick[cpu] = curr_tick;
	last_iowait_tick[cpu] = curr_iowait_tick;

	next_checked_jiffies = curr_tick + (debug_cpu_usage_interval*HZ)/1000;
	
	/* Calculate the busy rate */
/*
	if (total_time >= busy_time && total_time > 0) {
		busy_time = 100 * busy_time;
		do_div(busy_time, total_time);
		*usage = busy_time;

		iowait_time = 100 * iowait_time;
		do_div(iowait_time, total_time);
		*iowait_usage = iowait_time;
	}
*/
	if (total_tick >= busy_tick && total_tick > 0) {
		busy_tick = 100 * busy_tick;
		do_div(busy_tick, total_tick);
		*usage = busy_tick;
	}

	if (total_tick >= iowait_tick && total_tick > 0) {
		iowait_tick = 100 * iowait_tick;
		do_div(iowait_tick, total_tick);
		*iowait_usage = iowait_tick;
	}
}

#define MAX_REG_LOOP_CHAR	512

static int debug_cpu_usage_enable_set(const char *val, struct kernel_param *kp)
{
	return param_set_int(val, kp);
}

module_param_call(debug_cpu_usage_enable, debug_cpu_usage_enable_set, param_get_int, &debug_cpu_usage_enable, 0644);

static int debug_cpu_usage_interval_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;
	
	if (debug_cpu_usage_interval < 1000) {
		debug_cpu_usage_interval = 1000;
	}
	return ret;
}

module_param_call(debug_cpu_usage_interval, debug_cpu_usage_interval_set, param_get_int, &debug_cpu_usage_interval, 0644);

extern void cpufreq_quick_get_infos(unsigned int cpu, unsigned int *min, unsigned int *max, unsigned int *cur);
extern void kgsl_pwr_quick_get_infos(unsigned int *min, unsigned int *max, unsigned *curr, unsigned *therml);
extern void quick_get_cooling_device_freq(unsigned int *curr);
/*
* cpu_info_msg: output message
* msg_len: the size of output message.
* output message is included cpu's usage, real cpu frequecy and cpu governor's current cpu frequency.
*/
static void get_cpu_usage_and_freq(char * cpu_info_msg, int msg_len)
{
	unsigned int cpu_freq_min = 0;
	unsigned int cpu_freq_max = 0;
	unsigned int cpu_curr_freq = 0;
	unsigned int gpu_freq_min = 0;
	unsigned int gpu_freq_max = 0;
	unsigned int gpu_curr_freq = 0;
	unsigned int gpu_therm_freq = 0;
	unsigned int cooling_device[2] = {0, 0};
	long cpu_usage = 0;
	long iowait_usage = 0;

	int cpu;

	int len = msg_len; 
	int str_len = 0;
	int tmp_len = 0;

	if (!cpu_info_msg || msg_len <= 0) {
		return;
	}

	/*CPU0 Frequency*/
	cpufreq_quick_get_infos(0, &cpu_freq_min, &cpu_freq_max, &cpu_curr_freq);
	cpu_freq_max /= 1000;
	cpu_freq_min /= 1000;
	cpu_curr_freq /= 1000; /*cpu governor's current cpu frequency*/
	tmp_len = snprintf(cpu_info_msg, len, "%s:[CPU0 min=%u max=%u curr=%u]",freq_tag, cpu_freq_min, cpu_freq_max, cpu_curr_freq);
	str_len += tmp_len;
	len -= tmp_len;

	/*CPU6 Frequency*/
	cpufreq_quick_get_infos(6, &cpu_freq_min, &cpu_freq_max, &cpu_curr_freq);
	cpu_freq_max /= 1000;
	cpu_freq_min /= 1000;
	cpu_curr_freq /= 1000; /*cpu governor's current cpu frequency*/
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[CPU6 min=%u max=%u curr=%u]",cpu_freq_min, cpu_freq_max, cpu_curr_freq);
	str_len += tmp_len;
	len -= tmp_len;

	quick_get_cooling_device_freq(cooling_device);
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[COOL0=%u][COOL1=%u]",cooling_device[0]/1000, cooling_device[1]/1000);
	str_len += tmp_len;
	len -= tmp_len;

	/*GPU Frequency*/
	kgsl_pwr_quick_get_infos(&gpu_freq_min, &gpu_freq_max, &gpu_curr_freq, &gpu_therm_freq);
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[GPU min=%u max=%u curr=%u therm=%u]\n%s:",
				gpu_freq_min, gpu_freq_max, gpu_curr_freq, gpu_therm_freq, cpu_usage_tag);
	str_len += tmp_len;
	len -= tmp_len;

	for_each_present_cpu(cpu) {
		get_cpu_usage(cpu, &cpu_usage, &iowait_usage); /*get cpu usage*/

		tmp_len = snprintf((cpu_info_msg + str_len), len, "[C%d%s:%3ld%% IOW=%3ld%%]", 
					cpu, (cpu_is_offline(cpu) ? " xfx" : ""), cpu_usage, iowait_usage);
		
		str_len += tmp_len;
		len -= tmp_len;
		
		if(len <= 0 || str_len >= msg_len) {
			break;
		}
	}

	if(len > 0 && str_len < msg_len) {
		snprintf((cpu_info_msg + str_len), len, "C%d:", smp_processor_id()); /*this cpu is handling this timer interrupt*/
	}
}

static void show_cpu_info_and_address(char *cpu_info_msg)
{
	char *pmsg;
	struct task_struct * p = current;
	struct pt_regs *regs = get_irq_regs();

	pmsg = strstr(cpu_info_msg, cpu_usage_tag);
	*pmsg = 0;
	show_msg("%s", cpu_info_msg);
	*pmsg = cpu_usage_tag[0];

	if (likely(p != 0)) {
		if (regs != 0) {
			if (regs->pc <= TASK_SIZE) { /* User space */
				struct mm_struct *mm = p->mm;
				struct vm_area_struct *vma;
				struct file *map_file = NULL;

				/* Parse vma information */
				vma = find_vma(mm, regs->pc);
				if (vma != NULL) {
					map_file = vma->vm_file;
				
					if (map_file) {  /* memory-mapped file */
						show_msg("%sLR=0x%08llx PC=0x%08llx[U][%4d][%s][%s+0x%lx]\n",
							pmsg, regs->compat_lr, regs->pc, (unsigned int) p->pid,	p->comm, 
							map_file->f_path.dentry->d_iname, (unsigned long)regs->pc - vma->vm_start);
					} else {
						const char *name = arch_vma_name(vma);
						if (!name) {
							if (mm) {
								if (vma->vm_start <= mm->start_brk && vma->vm_end >= mm->brk) {
									name = "heap";
								} else if (vma->vm_start <= mm->start_stack && vma->vm_end >= mm->start_stack) {
									name = "stack";
								}
							} else {
								name = "vdso";
							}
						}
						show_msg("%sLR=0x%08llx PC=0x%08llx[U][%4d][%s][%s]\n",
							pmsg, regs->compat_lr, regs->pc, (unsigned int) p->pid,	p->comm, name);
					}
				}
			} else { /* Kernel space */
				char symbolbuf[64]="???";

				#ifdef CONFIG_KALLSYMS
				sprint_symbol(symbolbuf, regs->pc);
				#endif
				show_msg("%sLR=0x%08llx PC=0x%08llx[K][%4d][%s][%s]\n",	
					pmsg, regs->regs[30], regs->pc,	(unsigned int) p->pid, p->comm,	symbolbuf);
					
/*
				#ifdef CONFIG_KALLSYMS
				print_symbol("[%s]\n", regs->pc);
				#else
				printk(KERN_INFO "\n");
				#endif
*/

			}
		} else { /* Can't get PC & RA address */
			show_msg("%s[%s]\n", pmsg, p->comm);
		}
	} else { /* Can't get process information */
		show_msg("%sERROR: p=0x%08lx regs=0x%08lx\n", pmsg, (long)p, (long)regs);
	}
}

void count_cpu_time(void)
{
	if (next_checked_jiffies == 0) {
		// delay 20 seconds
		next_checked_jiffies = get_jiffies_64() + 20*HZ;
		show_msg("next_checked_jiffies = %llu", next_checked_jiffies);
	}
	
	if (!unlikely(debug_cpu_usage_enable))
		return;

	if (time_after_eq64(get_jiffies_64(), next_checked_jiffies)) {

		char cpu_info_msg[512] = {0};
		int len = sizeof(cpu_info_msg);
		
		get_cpu_usage_and_freq(cpu_info_msg, len);

		show_cpu_info_and_address(cpu_info_msg);

		if (debug_cpu_usage_enable & 2) {
			static int times = 0;
			static int need_init_tz_info_work = 1;
            
			// echo 2 > /sys/module/tick_sched_ext/parameters/debug_cpu_usage_enable
			if (need_init_tz_info_work) {
				INIT_DELAYED_WORK(&update_tz_info_work, update_tz_info);
				need_init_tz_info_work = 0;
			}

			if ((times % ARM_TZ_INFO_WORK_CNT) == 0) {
				// schedule next queue
				schedule_delayed_work(&update_tz_info_work, round_jiffies_relative(msecs_to_jiffies(0)));
				times = 0;
			}
			times ++;
		}	
	}
}

