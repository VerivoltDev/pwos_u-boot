// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 PHYTEC America, LLC - https://www.phytec.com
 *
 * Based on board/ti/am64x/evm.c
 *
 */

#include <common.h>
#include <asm/io.h>
#include <env.h>
#include <env_internal.h>
#include <spl.h>
#include <fdt_support.h>
#include <jffs2/load_kernel.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <dm/uclass.h>
#include <k3-ddrss.h>
#include <extension_board.h>
#include <malloc.h>
#include <mtd_node.h>
#include <asm/gpio.h>


#include "../common/am64_som_detection.h"

DECLARE_GLOBAL_DATA_PTR;

#define EEPROM_ADDR             0x50
#define EEPROM_ADDR_FALLBACK    -1

#define AM6X_DISABLE_ETH_PHY_OVERLAY "k3-am6-phycore-disable-eth-phy.dtbo"
#define AM6X_DISABLE_SPI_NOR_OVERLAY "k3-am6-phycore-disable-spi-nor.dtbo"
#define AM6X_DISABLE_RTC_OVERLAY "k3-am6-phycore-disable-rtc.dtbo"

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

#if defined(CONFIG_SPL_BUILD)
#if defined(CONFIG_K3_AM64_DDRSS)
static void fixup_ddr_driver_for_ecc(struct spl_image_info *spl_image)
{
	struct udevice *dev;
	int ret;

	dram_init_banksize();

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("Cannot get RAM device for ddr size fixup: %d\n", ret);

	ret = k3_ddrss_ddr_fdt_fixup(dev, spl_image->fdt_addr, gd->bd);
	if (ret)
		printf("Error fixing up ddr node for ECC use! %d\n", ret);
}
#else
static void fixup_memory_node(struct spl_image_info *spl_image)
{
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int bank;
	int ret;

	dram_init();
	dram_init_banksize();

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		start[bank] =  gd->bd->bi_dram[bank].start;
		size[bank] = gd->bd->bi_dram[bank].size;
	}

	/* dram_init functions use SPL fdt, and we must fixup u-boot fdt */
	ret = fdt_fixup_memory_banks(spl_image->fdt_addr, start, size, CONFIG_NR_DRAM_BANKS);
	if (ret)
		printf("Error fixing up memory node! %d\n", ret);
}
#endif

void spl_perform_fixups(struct spl_image_info *spl_image)
{
#if defined(CONFIG_K3_AM64_DDRSS)
	fixup_ddr_driver_for_ecc(spl_image);
#else
	fixup_memory_node(spl_image);
#endif

}
#endif

int qspi_fixup(void *blob, struct phytec_eeprom_data *data)
{
	ofnode node;
	int ret;

	if (!data)
		return 0;

	if (!phytec_am64_is_qspi(data))
		return 0;

	if (blob) {
		do_fixup_by_compat_u32(blob, "jedec,spi-nor", "spi-tx-bus-width", 1, 0);
		do_fixup_by_compat_u32(blob, "jedec,spi-nor", "spi-rx-bus-width", 4, 0);
	} else {
		node = ofnode_by_compatible(ofnode_null(), "jedec,spi-nor");
		if (!ofnode_valid(node))
			return -1;
		ret = ofnode_write_u32(node, "spi-tx-bus-width", 1);
		if (ret < 0)
			return ret;
		ret = ofnode_write_u32(node, "spi-rx-bus-width", 4);
		if (ret < 0)
			return ret;
	}
	return 0;
}

#if defined(CONFIG_SPL_BUILD)
int do_board_detect(void)
{
	int ret;
	struct phytec_eeprom_data data;

	ret = phytec_eeprom_data_setup(&data, 0, EEPROM_ADDR);
	if (ret || !data.valid)
		return 0;

	return qspi_fixup(NULL, &data);
}
#endif

#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
	if (!strcmp(name, "k3-am642-r5-phycore-som-2gb") || !strcmp(name, "k3-am642-phyboard-electra-rdk"))
		return 0;
	return -1;
}
#endif

/* Detect factory_reset request */
static int __maybe_unused detect_factory_reset(void)
{
	if (IS_ENABLED(CONFIG_DM_GPIO) && IS_ENABLED(CONFIG_OF_LIBFDT)) {
		struct gpio_desc desc = {0};
		char *factory_reset_gpio = "gpio@601000_55";
		int ret;

		ret = dm_gpio_lookup_name(factory_reset_gpio, &desc);
		if (ret) {
			printf("error getting GPIO lookup name: %d \t %s \n", ret, factory_reset_gpio);
			return ret;
		}

		ret = dm_gpio_request(&desc, factory_reset_gpio);
		if (ret) {
			printf("error requesting GPIO: %d\n", ret);
			goto err_free_gpio;
		}

		ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN);
		if (ret) {
			printf("error setting direction flag of GPIO: %d\n", ret);
			goto err_free_gpio;
		}

		if (dm_gpio_get_value(&desc)) {
			printf("ON ON ON ON ON ON \n ON ON ON ON ON ON \n");
		} else {
			printf("OFF OFF OFF OFF OFF OFF \n OFF OFF OFF OFF OFF OFF \n");
		}
err_free_gpio:
		dm_gpio_free(desc.dev, &desc);
		return ret;
	}

/*
	int res = dm_gpio_set_value(&desc, 1);
	mdelay(50);
	if (res) {
		debug("%s: Error while setting GPIO %d (err = %d)\n",
		dev->name, i, res);
		res;
	}
*/
}	


/* Functions borrowed from am642_init.c */
static u32 __get_backup_bootmedia(u32 main_devstat)
{
	u32 bkup_bootmode =
	    (main_devstat & MAIN_DEVSTAT_BACKUP_BOOTMODE_MASK) >>
	    MAIN_DEVSTAT_BACKUP_BOOTMODE_SHIFT;
	u32 bkup_bootmode_cfg =
	    (main_devstat & MAIN_DEVSTAT_BACKUP_BOOTMODE_CFG_MASK) >>
	    MAIN_DEVSTAT_BACKUP_BOOTMODE_CFG_SHIFT;

	switch (bkup_bootmode) {
	case BACKUP_BOOT_DEVICE_UART:
		return BOOT_DEVICE_UART;

	case BACKUP_BOOT_DEVICE_DFU:
		if (bkup_bootmode_cfg & MAIN_DEVSTAT_BACKUP_USB_MODE_MASK)
			return BOOT_DEVICE_USB;
		return BOOT_DEVICE_DFU;

	case BACKUP_BOOT_DEVICE_ETHERNET:
		return BOOT_DEVICE_ETHERNET;

	case BACKUP_BOOT_DEVICE_MMC:
		if (bkup_bootmode_cfg)
			return BOOT_DEVICE_MMC2;
		return BOOT_DEVICE_MMC1;

	case BACKUP_BOOT_DEVICE_SPI:
		return BOOT_DEVICE_SPI;

	case BACKUP_BOOT_DEVICE_I2C:
		return BOOT_DEVICE_I2C;
	};

	return BOOT_DEVICE_RAM;
}

static u32 __get_primary_bootmedia(u32 main_devstat)
{
	u32 bootmode = (main_devstat & MAIN_DEVSTAT_PRIMARY_BOOTMODE_MASK) >>
	    MAIN_DEVSTAT_PRIMARY_BOOTMODE_SHIFT;
	u32 bootmode_cfg =
	    (main_devstat & MAIN_DEVSTAT_PRIMARY_BOOTMODE_CFG_MASK) >>
	    MAIN_DEVSTAT_PRIMARY_BOOTMODE_CFG_SHIFT;

	switch (bootmode) {
	case BOOT_DEVICE_OSPI:
		fallthrough;
	case BOOT_DEVICE_QSPI:
		fallthrough;
	case BOOT_DEVICE_XSPI:
		fallthrough;
	case BOOT_DEVICE_SPI:
		return BOOT_DEVICE_SPI;

	case BOOT_DEVICE_ETHERNET_RGMII:
		fallthrough;
	case BOOT_DEVICE_ETHERNET_RMII:
		return BOOT_DEVICE_ETHERNET;

	case BOOT_DEVICE_EMMC:
		return BOOT_DEVICE_MMC1;

	case BOOT_DEVICE_MMC:
		if ((bootmode_cfg & MAIN_DEVSTAT_PRIMARY_MMC_PORT_MASK) >>
		     MAIN_DEVSTAT_PRIMARY_MMC_PORT_SHIFT)
			return BOOT_DEVICE_MMC2;
		return BOOT_DEVICE_MMC1;

	case BOOT_DEVICE_DFU:
		if ((bootmode_cfg & MAIN_DEVSTAT_PRIMARY_USB_MODE_MASK) >>
		    MAIN_DEVSTAT_PRIMARY_USB_MODE_SHIFT)
			return BOOT_DEVICE_USB;
		return BOOT_DEVICE_DFU;

	case BOOT_DEVICE_NOBOOT:
		return BOOT_DEVICE_RAM;
	}

	return bootmode;
}

u32 get_boot_device(void)
{
	u32 devstat = readl(CTRLMMR_MAIN_DEVSTAT);
	u32 bootindex = *(u32 *)(CONFIG_SYS_K3_BOOT_PARAM_TABLE_INDEX);
	u32 bootmedia;

	if (bootindex == K3_PRIMARY_BOOTMODE)
		bootmedia = __get_primary_bootmedia(devstat);
	else
		bootmedia = __get_backup_bootmedia(devstat);

	return bootmedia;
}

#if IS_ENABLED(CONFIG_ENV_IS_IN_FAT) || IS_ENABLED(CONFIG_ENV_IS_IN_MMC)
int mmc_get_env_dev(void)
{
	u32 boot_device = get_boot_device();

	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
		return 1;
	};

	return CONFIG_SYS_MMC_ENV_DEV;
}
#endif

enum env_location env_get_location(enum env_operation op, int prio)
{
	u32 boot_device = get_boot_device();

	if (prio)
		return ENVL_UNKNOWN;

	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
	case BOOT_DEVICE_MMC2:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_FAT))
			return ENVL_FAT;
		if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
			return ENVL_MMC;
	case BOOT_DEVICE_SPI:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_SPI_FLASH))
			return ENVL_SPI_FLASH;
	default:
		return ENVL_NOWHERE;
	};
}

#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
	u32 boot_device = get_boot_device();

	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		env_set_ulong("mmcdev", 0);
		env_set("boot", "mmc");
		break;
	case BOOT_DEVICE_MMC2:
		env_set_ulong("mmcdev", 1);
		env_set("boot", "mmc");
		break;
	case BOOT_DEVICE_SPI:
		env_set("boot", "spi");
		break;
	case BOOT_DEVICE_ETHERNET:
		env_set("boot", "net");
		break;
	};

	if (IS_ENABLED(CONFIG_PHYTEC_SOM_DETECTION_BLOCKS)) {
		struct phytec_api3_element *block_element;
		struct phytec_eeprom_data data;
		int ret;

		ret = phytec_eeprom_data_setup(&data, 0, EEPROM_ADDR);
		if (ret || !data.valid)
			return 0;

		PHYTEC_API3_FOREACH_BLOCK(block_element, &data) {
			switch (block_element->block_type) {
			case PHYTEC_API3_BLOCK_MAC:
				phytec_blocks_add_mac_to_env(block_element);
				break;
			default:
				debug("%s: Unknown block type %i\n", __func__,
				      block_element->block_type);
			}
		}
	}

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_CMD_EXTENSION)
int extension_board_scan(struct list_head *extension_list)
{
	struct extension *extension = NULL;
	struct phytec_eeprom_data data;
	int count = 0;
	int ret;

	ret = phytec_eeprom_data_setup(&data, 0, EEPROM_ADDR);
	if (ret || !data.valid)
		return count;

	phytec_print_som_info(&data);

	if (phytec_get_am64_eth(&data) == 0) {
		extension = phytec_add_extension("Disable eth phy",
						 AM6X_DISABLE_ETH_PHY_OVERLAY, "");
		list_add_tail(&extension->list, extension_list);
		count++;
	}

	if (phytec_get_am64_spi(&data) == PHYTEC_EEPROM_VALUE_X) {
		extension = phytec_add_extension("Disable SPI NOR Flash",
						 AM6X_DISABLE_SPI_NOR_OVERLAY, "");
		list_add_tail(&extension->list, extension_list);
		count++;
	}

	if (phytec_get_am64_rtc(&data) == 0) {
		extension = phytec_add_extension("Disable RTC",
						 AM6X_DISABLE_RTC_OVERLAY, "");
		list_add_tail(&extension->list, extension_list);
		count++;
	}

	return count;
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;
	struct phytec_eeprom_data data;

	ret = phytec_eeprom_data_setup(&data, 0, EEPROM_ADDR);
	if (ret || !data.valid)
		return 0;
	fdt_copy_fixed_partitions(blob);
	return qspi_fixup(blob, &data);
}
#endif

#if IS_ENABLED(CONFIG_OF_BOARD_FIXUP)
int board_fix_fdt(void *blob)
{
	int ret;
	struct phytec_eeprom_data data;

	ret = phytec_eeprom_data_setup(&data, 0, EEPROM_ADDR);
	if (ret || !data.valid)
		return 0;
	return qspi_fixup(NULL, &data);
}
#endif


#ifdef CONFIG_SPL_BOARD_INIT
#define CTRLMMR_USB0_PHY_CTRL	0x43004008
#define CORE_VOLTAGE		0x80000000

void spl_board_init(void)
{
	u32 val;

	/* Set USB PHY core voltage to 0.85V */
	val = readl(CTRLMMR_USB0_PHY_CTRL);
	val &= ~(CORE_VOLTAGE);
	writel(val, CTRLMMR_USB0_PHY_CTRL);

	/* Init DRAM size for R5/A53 SPL */
	dram_init_banksize();
	detect_factory_reset();
}
#endif
