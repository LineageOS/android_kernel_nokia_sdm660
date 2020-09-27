#ifndef __FIH_POFF_H
#define __FIH_POFF_H

/* QC: PMIC */
#define FIH_POFF_SOFT          0x00000001
#define FIH_POFF_PS_HOLD       0x00000002
#define FIH_POFF_PMIC_WD       0x00000004  /* PMIC Watchdog */
#define FIH_POFF_GP1           0x00000008  /* Keypad_Reset1 */
#define FIH_POFF_GP2           0x00000010  /* Keypad_Reset2 */
#define FIH_POFF_SIMULT_N      0x00000020  /* Simultaneous KPDPWR_N and RESIN_N */
#define FIH_POFF_RESIN_N       0x00000040
#define FIH_POFF_KPDPWR_N      0x00000080

#define FIH_POFF_RAW_XVDD_SHD  0x00000400
#define FIH_POFF_RAW_DVDD_SHD  0x00000800
#define FIH_POFF_IMM_XVDD_SD   0x00001000
#define FIH_POFF_S3_RESET      0x00002000
#define FIH_POFF_FAULT_SEQ     0x00004000
#define FIH_POFF_POFF_SEQ      0x00008000

#define FIH_POFF_OTST3         0x80000000  /* Over Temperature */
#define FIH_POFF_UVLO          0x40000000  /* Under Voltage */
#define FIH_POFF_OVLO          0x20000000  /* Over Voltage */

#endif

