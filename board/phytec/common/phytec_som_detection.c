// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 PHYTEC Messtechnik GmbH
 * Author: Teresa Remmet <t.remmet@phytec.de>
 */

#include <common.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/sys_proto.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <i2c.h>
#include <u-boot/crc.h>
#include <malloc.h>

#include "phytec_som_detection.h"
#include "am64_som_detection.h"
#include "am62_som_detection.h"
#include "am62a_som_detection.h"

struct phytec_eeprom_data eeprom_data;

#if IS_ENABLED(CONFIG_PHYTEC_SOM_DETECTION)

int phytec_eeprom_data_setup_fallback(struct phytec_eeprom_data *data,
				      int bus_num, int addr, int addr_fallback)
{
	int ret;

	ret = phytec_eeprom_data_init(data, bus_num, addr);
	if (ret && addr_fallback >= 0) {
		pr_err("%s: init failed. Trying fall back address 0x%x\n",
		       __func__, addr_fallback);
		ret = phytec_eeprom_data_init(data, bus_num, addr_fallback);
	}

	if (ret)
		pr_err("%s: EEPROM data init failed\n", __func__);

	return ret;
}

int phytec_eeprom_data_setup(struct phytec_eeprom_data *data,
			     int bus_num, int addr)
{
	int ret;

	ret = phytec_eeprom_data_init(data, bus_num, addr);
	if (ret)
		pr_err("%s: EEPROM data init failed\n", __func__);

	return ret;
}

int phytec_eeprom_read(u8 *data, int bus_num, int addr,
		       int size, int offset)
{
	int ret;

#if CONFIG_IS_ENABLED(DM_I2C)
	struct udevice *dev;

	ret = i2c_get_chip_for_busnum(bus_num, addr, 2, &dev);
	if (ret) {
		pr_err("%s: i2c EEPROM not found: %i.\n", __func__, ret);
		return ret;
	}

	ret = dm_i2c_read(dev, offset, data, size);
	if (ret) {
		pr_err("%s: Unable to read EEPROM data: %i\n", __func__, ret);
		return ret;
	}
#else
	i2c_set_bus_num(bus_num);
	ret = i2c_read(addr, offset, 2, data, size);
#endif
	return ret;
}

int phytec_eeprom_data_init_v2(struct phytec_eeprom_data *data)
{
	int ret;
	unsigned int crc;
	u8 som;
	char *opt;

	crc = crc8(0, (const unsigned char *)&data->data,
		   PHYTEC_API2_DATA_LEN);
	debug("%s: crc: %x\n", __func__, crc);

	if (crc) {
		pr_err("%s: CRC mismatch. EEPROM data is unusable\n",
		       __func__);
		goto err;
	}

	data->valid = true;
	som = data->data.data.data_api2.som_no;
	debug("%s: som id: %u\n", __func__, som);
	opt = phytec_get_opt(data);
	if (!opt)
		goto err;

	ret = -1;
	if (IS_ENABLED(CONFIG_PHYTEC_AM64_SOM_DETECTION))
		ret = phytec_am64_detect(som, opt);
	if (IS_ENABLED(CONFIG_PHYTEC_AM62_SOM_DETECTION))
		ret = phytec_am62_detect(som, opt);
	if (IS_ENABLED(CONFIG_PHYTEC_AM62A_SOM_DETECTION))
		ret = phytec_am62a_detect(som, opt);
	if (!ret)
		return 0;

	pr_err("%s: SoM ID does not match. Wrong EEPROM data?\n", __func__);
err:
	data->valid = false;
	return -1;
}

#if IS_ENABLED(CONFIG_PHYTEC_SOM_DETECTION_BLOCKS)

int phytec_eeprom_data_init_v3_block(struct phytec_eeprom_data *data,
				     struct phytec_api3_block_header *header,
				     u8 *payload)
{
	struct phytec_api3_element *element = NULL;
	struct phytec_api3_element *list_iterator;

	if (!header)
		return -1;
	if (!payload)
		return -1;

	debug("%s: block type: %i\n", __func__, header->block_type);
	switch (header->block_type) {
	case PHYTEC_API3_BLOCK_MAC:
		element = phytec_blocks_init_mac(header, payload);
		break;
	default:
		debug("%s: Unknown block type %i\n", __func__,
		      header->block_type);
	}
	if (!element)
		return -1;

	if (!data->data.block_head) {
		data->data.block_head = element;
		return 0;
	}

	list_iterator = data->data.block_head;
	while (list_iterator && list_iterator->next)
		list_iterator = list_iterator->next;
	list_iterator->next = element;

	return 0;
}

int phytec_eeprom_data_init_v3(struct phytec_eeprom_data *data,
			       int bus_num, int addr)
{
	int ret, i;
	struct phytec_api3_header header;
	unsigned int crc;
	u8 *payload;
	int block_addr;
	struct phytec_api3_block_header *block_header;

	if (!data)
		return -1;

	ret = phytec_eeprom_read((uint8_t *)&header, bus_num, addr,
				 PHYTEC_API3_DATA_HEADER_LEN,
				 PHYTEC_API2_DATA_LEN);
	if (ret) {
		pr_err("%s: Failed to read API v3 data header.\n", __func__);
		goto err;
	}

	crc = crc8(0, (const unsigned char *)&header,
		   PHYTEC_API3_DATA_HEADER_LEN);
	debug("%s: crc: %x\n", __func__, crc);
	if (crc) {
		pr_err("%s: CRC mismatch. API3 header is unusable.\n",
		       __func__);
		goto err;
	}

	debug("%s: data length: %i\n", __func__, header.data_length);
	payload = malloc(header.data_length);
	if (!payload) {
		pr_err("%s: Unable to allocate memory\n", __func__);
		goto err_payload;
	}

	ret = phytec_eeprom_read(payload, bus_num, addr, header.data_length,
				 PHYTEC_API3_DATA_HEADER_LEN +
				 PHYTEC_API2_DATA_LEN);
	if (ret) {
		pr_err("%s: Failed to read API v3 data payload.\n", __func__);
		goto err_payload;
	}

	block_addr = 0;
	debug("%s: block count: %i\n", __func__, header.block_count);
	for (i = 0; i < header.block_count; i++) {
		debug("%s: block_addr: %i\n", __func__, block_addr);
		block_header = (struct phytec_api3_block_header *)
			&payload[block_addr];
		crc = crc8(0, (const unsigned char *)block_header,
			   PHYTEC_API3_BLOCK_HEADER_LEN);

		debug("%s: crc: %x\n", __func__, crc);
		if (crc) {
			pr_err("%s: CRC mismatch. API3 block header is unusable\n",
			       __func__);
			goto err_payload;
		}

		ret = phytec_eeprom_data_init_v3_block(data, block_header,
			&payload[block_addr + PHYTEC_API3_BLOCK_HEADER_LEN]);
		/* Ignore failed block initialization and continue. */
		if (ret)
			debug("%s: Unable to create block with index %i.\n",
			      __func__, i);

		block_addr = block_header->next_block;
	}

	free(payload);
	data->valid = true;
	return 0;
err_payload:
	free(payload);
err:
	data->valid = false;
	return -1;
}

#else

inline int phytec_eeprom_data_init_v3(struct phytec_eeprom_data *data,
				      int bus_num, int addr)
{
	return 0;
}

#endif

int phytec_eeprom_data_init(struct phytec_eeprom_data *data,
			    int bus_num, int addr)
{
	int ret, i;
	u8 *ptr;

	if (!data)
		data = &eeprom_data;

	ret = phytec_eeprom_read((u8 *)data, bus_num, addr,
				 PHYTEC_API2_DATA_LEN, 0);
	if (ret)
		goto err;
	data->data.block_head = NULL;

	if (data->data.api_rev == 0xff) {
		pr_err("%s: EEPROM is not flashed. Prototype?\n", __func__);
		goto err;
	}

	ptr = (u8 *)data;
	for (i = 0; i < PHYTEC_API2_DATA_LEN; ++i)
		if (ptr[i] != 0x0)
			break;

	if (i == PHYTEC_API2_DATA_LEN) {
		pr_err("%s: EEPROM data is all zero. Erased?\n", __func__);
		goto err;
	}

	if (data->data.api_rev >= PHYTEC_API_REV2) {
		ret = phytec_eeprom_data_init_v2(data);
		if (ret)
			goto err;
	}

	if (IS_ENABLED(CONFIG_PHYTEC_SOM_DETECTION_BLOCKS))
		if (data->data.api_rev >= PHYTEC_API_REV3) {
			ret = phytec_eeprom_data_init_v3(data, bus_num, addr);
			if (ret)
				goto err;
		}

	data->valid = true;
	return 0;
err:
	data->valid = false;
	return -1;
}

void __maybe_unused phytec_print_som_info(struct phytec_eeprom_data *data)
{
	struct phytec_api2_data *api2;
	char pcb_sub_rev;
	unsigned int ksp_no, sub_som_type1, sub_som_type2;

	if (!data)
		data = &eeprom_data;

	if (!data->valid || data->data.api_rev < PHYTEC_API_REV2)
		return;

	api2 = &data->data.data.data_api2;

	/* Calculate PCB subrevision */
	pcb_sub_rev = api2->pcb_sub_opt_rev & 0x0f;
	pcb_sub_rev = pcb_sub_rev ? ((pcb_sub_rev - 1) + 'a') : ' ';

	/* print standard product string */
	if (api2->som_type <= 1) {
		printf("SoM: %s-%03u-%s.%s PCB rev: %u%c\n",
		       phytec_som_type_str[api2->som_type], api2->som_no,
		       api2->opt, api2->bom_rev, api2->pcb_rev, pcb_sub_rev);
		return;
	}
	/* print KSP/KSM string */
	if (api2->som_type <= 3) {
		ksp_no = (api2->ksp_no << 8) | api2->som_no;
		printf("SoM: %s-%u ",
		       phytec_som_type_str[api2->som_type], ksp_no);
	/* print standard product based KSP/KSM strings */
	} else {
		switch (api2->som_type) {
		case 4:
			sub_som_type1 = 0;
			sub_som_type2 = 3;
			break;
		case 5:
			sub_som_type1 = 0;
			sub_som_type2 = 2;
			break;
		case 6:
			sub_som_type1 = 1;
			sub_som_type2 = 3;
			break;
		case 7:
			sub_som_type1 = 1;
			sub_som_type2 = 2;
			break;
		default:
			pr_err("%s: Invalid SoM type: %i", __func__, api2->som_type);
			return;
		};

		printf("SoM: %s-%03u-%s-%03u ",
		       phytec_som_type_str[sub_som_type1],
		       api2->som_no, phytec_som_type_str[sub_som_type2],
		       api2->ksp_no);
	}

	printf("Option: %s BOM rev: %s PCB rev: %u%c\n", api2->opt,
	       api2->bom_rev, api2->pcb_rev, pcb_sub_rev);
}

char * __maybe_unused phytec_get_opt(struct phytec_eeprom_data *data)
{
	char *opt;

	if (!data)
		data = &eeprom_data;
	if (!data->valid)
		return NULL;

	if (data->data.api_rev < PHYTEC_API_REV2)
		opt = data->data.data.data_api0.opt;
	else
		opt = data->data.data.data_api2.opt;

	return opt;
}

struct extension *phytec_add_extension(const char *name, const char *overlay,
				       const char *other)
{
	struct extension *extension;

	if (strlen(overlay) > sizeof(extension->overlay)) {
		pr_err("Overlay name %s is longer than %i.\n", overlay,
		       (int)sizeof(extension->overlay));
		return NULL;
	}

	extension = calloc(1, sizeof(struct extension));
	snprintf(extension->name, sizeof(extension->name), name);
	snprintf(extension->overlay, sizeof(extension->overlay), overlay);
	snprintf(extension->other, sizeof(extension->other), other);
	snprintf(extension->owner, sizeof(extension->owner), "PHYTEC");

	return extension;
}

struct phytec_api3_element * __maybe_unused phytec_get_block_head(
					       struct phytec_eeprom_data *data)
{
	if (!data)
		data = &eeprom_data;
	if (!data->valid)
		return NULL;

	return data->data.block_head;
}

#else

inline int phytec_eeprom_data_setup_fallback(struct phytec_eeprom_data *data,
					     int bus_num, int addr,
					     int addr_fallback)
{
	return PHYTEC_EEPROM_INVAL;
}

inline int phytec_eeprom_data_setup(struct phytec_eeprom_data *data,
				    int bus_num, int addr)
{
	return PHYTEC_EEPROM_INVAL;
}

inline int phytec_eeprom_data_init(struct phytec_eeprom_data *data,
				   int bus_num, int addr)
{
	return PHYTEC_EEPROM_INVAL;
}

inline void __maybe_unused phytec_print_som_info(
					       struct phytec_eeprom_data *data)
{
}

inline char * __maybe_unused phytec_get_opt(struct phytec_eeprom_data *data)
{
	return NULL;
}

struct extension *phytec_add_extension(const char *name, const char *overlay,
				       const char *other)
{
	return NULL;
}

inline struct phytec_api3_element * __maybe_unused phytec_get_block_head(
					       struct phytec_eeprom_data *data)
{
	return NULL;
}

#endif
