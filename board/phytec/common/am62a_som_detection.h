/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 PHYTEC Messtechnik GmbH
 * Author: Daniel Schultz <d.schultz@phytec.de>
 *         Garrett Giordano <ggiordano@phytec.com>
 */

#ifndef _PHYTEC_AM62A_SOM_DETECTION_H
#define _PHYTEC_AM62A_SOM_DETECTION_H

#include "phytec_som_detection.h"

#define PHYTEC_EEPROM_VALUE_X	0x21
#define PHYTEC_EEPROM_NOR_FLASH_64MB_QSPI	0xC

u8 phytec_am62a_detect(u8 som, char *opt);
u8 __maybe_unused phytec_get_am62a_ddr_size(struct phytec_eeprom_data *data);
u8 __maybe_unused phytec_get_am62a_spi(struct phytec_eeprom_data *data);
u8 __maybe_unused phytec_get_am62a_eth(struct phytec_eeprom_data *data);
u8 __maybe_unused phytec_get_am62a_rtc(struct phytec_eeprom_data *data);

static inline int phytec_am62a_is_qspi(struct phytec_eeprom_data *data)
{
	u8 spi = phytec_get_am62a_spi(data);

	if (spi == PHYTEC_EEPROM_VALUE_X)
		return 0;
	return spi <= PHYTEC_EEPROM_NOR_FLASH_64MB_QSPI;
}

static inline int phytec_am62a_is_ospi(struct phytec_eeprom_data *data)
{
	return phytec_get_am62a_spi(data) > PHYTEC_EEPROM_NOR_FLASH_64MB_QSPI;
}
#endif /* _PHYTEC_AM62A_SOM_DETECTION_H */
