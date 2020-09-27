/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

enum step_chg_cfg_idx {
	STEP_CHG_CFG = 0,
	JEITA_FCC_CFG,
	JEITA_FV_CFG,
};

int qcom_step_chg_init(bool, bool);
void qcom_step_chg_deinit(void);

int fih_set_step_chg_cfg(int *cfg, int cfg_len, int mode);
int fih_set_step_chg_hysteresis(int hysteresis, int mode);
#endif /* __STEP_CHG_H__ */
