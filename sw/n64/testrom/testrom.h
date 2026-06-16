/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 JPZV
 */

#pragma once

#include <stdint.h>

// SRAM constants
#define SRAM_256KBIT_SIZE         	0x00008000
#define SRAM_768KBIT_SIZE         	0x00018000
#define SRAM_1MBIT_SIZE           	0x00020000
#define SRAM_BANK_SIZE            	SRAM_256KBIT_SIZE
#define SRAM_256KBIT_BANKS        	1
#define SRAM_768KBIT_BANKS        	3
#define SRAM_1MBIT_BANKS          	4

void pi_read_raw(void *dest, uint32_t base, uint32_t offset, uint32_t len);
void pi_write_raw(const void *src, uint32_t base, uint32_t offset, uint32_t len);