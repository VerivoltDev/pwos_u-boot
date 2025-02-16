/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * K3: AM62 SoC definitions, structures etc.
 *
 * Copyright (C) 2020-2022 Texas Instruments Incorporated - https://www.ti.com/
 *	Suman Anna <s-anna@ti.com>
 */

#ifndef __ASM_ARCH_AM62_HARDWARE_H
#define __ASM_ARCH_AM62_HARDWARE_H

#include <config.h>
#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

#define PADCFG_MMR0_BASE			0x04080000
#define PADCFG_MMR1_BASE			0x000f0000
#define CTRL_MMR0_BASE				0x00100000
#define MCU_CTRL_MMR0_BASE			0x04500000
#define WKUP_CTRL_MMR0_BASE			0x43000000

#define CTRLMMR_WKUP_JTAG_DEVICE_ID		(WKUP_CTRL_MMR0_BASE + 0x18)
#define JTAG_DEV_ID_MASK			GENMASK(31, 18)
#define JTAG_DEV_ID_SHIFT			18
#define JTAG_DEV_CORE_NR_MASK			GENMASK(21, 19)
#define JTAG_DEV_CORE_NR_SHIFT			19
#define JTAG_DEV_GPU_MASK			BIT(18)
#define JTAG_DEV_GPU_SHIFT			18
#define JTAG_DEV_FEATURES_MASK			GENMASK(17, 13)
#define JTAG_DEV_FEATURES_SHIFT			13
#define JTAG_DEV_SECURITY_MASK			BIT(12)
#define JTAG_DEV_SECURITY_SHIFT			12
#define JTAG_DEV_SAFETY_MASK			BIT(11)
#define JTAG_DEV_SAFETY_SHIFT			11
#define JTAG_DEV_SPEED_MASK			GENMASK(10, 6)
#define JTAG_DEV_SPEED_SHIFT			6
#define JTAG_DEV_TEMP_MASK			GENMASK(5, 3)
#define JTAG_DEV_TEMP_SHIFT			3
#define JTAG_DEV_PKG_MASK			GENMASK(2, 0)
#define JTAG_DEV_PKG_SHIFT			0

#define JTAG_DEV_FEATURE_NO_PRU			0x4

#define CTRLMMR_MAIN_DEVSTAT			(WKUP_CTRL_MMR0_BASE + 0x30)
#define MAIN_DEVSTAT_PRIMARY_BOOTMODE_MASK	GENMASK(6, 3)
#define MAIN_DEVSTAT_PRIMARY_BOOTMODE_SHIFT	3
#define MAIN_DEVSTAT_PRIMARY_BOOTMODE_CFG_MASK	GENMASK(9, 7)
#define MAIN_DEVSTAT_PRIMARY_BOOTMODE_CFG_SHIFT	7
#define MAIN_DEVSTAT_BACKUP_BOOTMODE_MASK	GENMASK(12, 10)
#define MAIN_DEVSTAT_BACKUP_BOOTMODE_SHIFT	10
#define MAIN_DEVSTAT_BACKUP_BOOTMODE_CFG_MASK	BIT(13)
#define MAIN_DEVSTAT_BACKUP_BOOTMODE_CFG_SHIFT	13
#define RST_CTRL_ESM_ERROR_RST_EN_Z_MASK	(~BIT(17))

/* Primary Bootmode MMC Config macros */
#define MAIN_DEVSTAT_PRIMARY_MMC_PORT_MASK	0x4
#define MAIN_DEVSTAT_PRIMARY_MMC_PORT_SHIFT	2
#define MAIN_DEVSTAT_PRIMARY_MMC_FS_RAW_MASK	0x1
#define MAIN_DEVSTAT_PRIMARY_MMC_FS_RAW_SHIFT	0

/* Primary Bootmode USB Config macros */
#define MAIN_DEVSTAT_PRIMARY_USB_MODE_SHIFT	1
#define MAIN_DEVSTAT_PRIMARY_USB_MODE_MASK	0x02

/* Backup Bootmode USB Config macros */
#define MAIN_DEVSTAT_BACKUP_USB_MODE_MASK	0x01

#define MCU_CTRL_LFXOSC_CTRL			(MCU_CTRL_MMR0_BASE + 0x8038)
#define MCU_CTRL_LFXOSC_TRIM			(MCU_CTRL_MMR0_BASE + 0x803c)
#define MCU_CTRL_LFXOSC_32K_DISABLE_VAL		BIT(7)

#define MCU_CTRL_DEVICE_CLKOUT_32K_CTRL		(MCU_CTRL_MMR0_BASE + 0x8058)
#define MCU_CTRL_DEVICE_CLKOUT_LFOSC_SELECT_VAL	(0x3)

#define CTRLMMR_MCU_RST_CTRL			(MCU_CTRL_MMR0_BASE + 0x18170)

#define ROM_EXTENDED_BOOT_DATA_INFO		0x43c3f1e0
#define K3_BOOT_PARAM_TABLE_INDEX_OCRAM         0x7000F290

/*
 * During the boot process ROM will kill anything that writes to OCSRAM.
 * This means the wakeup SPL cannot use this region during boot. To
 * complicate things, TIFS will set a firewall between HSM RAM and the
 * main domain.
 *
 * So, during the wakeup SPL, we will need to store the EEPROM data
 * somewhere in HSM RAM, and the main domain's SPL will need to store it
 * somewhere in OCSRAM
 */
#ifdef CONFIG_CPU_V7R
#define TI_SRAM_SCRATCH_BOARD_EEPROM_START	0x43c30000
#else
 #define TI_SRAM_SCRATCH_BOARD_EEPROM_START	0x70000001
#endif /* CONFIG_CPU_V7R */

#endif /* __ASM_ARCH_AM62_HARDWARE_H */
