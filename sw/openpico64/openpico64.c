/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 * Copyright (c) 2024 JPZV
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/irq.h"

#include "stdio_async_uart.h"

#include "git_info.h"
#include "filesystem.h"
#include "n64_pi_task.h"
#include "openpico64_pins.h"
#include "sram.h"
#include "utils.h"

#define UART_TX_PIN (25) /* On-Board LED */ // NOTE: Will not work becase PIO is between 4 and 26
#define UART_RX_PIN (23) /* not available on the pico */
#define UART_ID     uart0
#define BAUD_RATE   115200

#define ENABLE_N64_PI 1

// Priority 0 = lowest, 3 = highest
#define SRAM_TASK_PRIORITY     (3UL)

static StaticTask_t sram_task;
static StackType_t sram_task_stack[4 * 1024 / sizeof(StackType_t)];

uint32_t g_flash_jedec_id;

/*

Profiling results:

Time between ~N64_READ and bit output on AD0

133 MHz old code:
    ROM:  1st _980_ ns, 2nd 500 ns
    SRAM: 1st  500  ns, 2nd 510 ns

133 MHz new code:
    ROM:  1st _300_ ns, 2nd 280 ns
    SRAM: 1st  320  ns, 2nd 320 ns

266 MHz new code:
    ROM:  1st  180 ns, 2nd 180 ns (sometimes down to 160, but only worst case matters)
    SRAM: 1st  160 ns, 2nd 160 ns

*/

// FreeRTOS boilerplate
void vApplicationGetTimerTaskMemory(StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, uint32_t * pulTimerTaskStackSize)
{
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void sram_task_entry(__unused void *params)
{
	printf("sram_task_entry\n");

	// Load SRAM backup from external flash
	// TODO: How do we detect if it's uninitialized (config area in flash?),
	//       or maybe we don't have to care?
	sram_load_from_flash();

	// Save SRAM backup to external flash
	// We do it by checking the times it's used (either by READ or WRITE)

	// We start the last counter by 255 in order to force a save to flash at the start
	uint8_t last_sram_counter = sram_counter;
	while (true)
	{
		if (last_sram_counter != sram_counter)
		{
			sram_save_to_flash();
			last_sram_counter = sram_counter;
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
	
	vTaskDelete(NULL);
}

void vLaunch(void)
{
	xTaskCreateStatic(sram_task_entry, "sram_task", configMINIMAL_STACK_SIZE, NULL, SRAM_TASK_PRIORITY, sram_task_stack, &sram_task);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();
}

#include "rom_vars.h"

uint32_t flash_get_jedec_id(void)
{
	const uint8_t read_jedec_id = 0x9f;
	uint8_t txbuf[4] = { read_jedec_id };
	uint8_t rxbuf[4] = { 0 };
	txbuf[0] = read_jedec_id;
	flash_do_cmd(txbuf, rxbuf, 4);

	return rxbuf[1] | (rxbuf[2] << 8) | (rxbuf[3] << 16);
}

int main(void)
{
	// First, let's probe the Flash ID
	g_flash_jedec_id = flash_get_jedec_id();

	// Overclock!
	// The external flash should be rated to 133MHz,
	// but since it's used with a 2x clock divider,
	// 266 MHz is safe in this regard.

	// set_sys_clock_khz(133000, true);
	set_sys_clock_khz(266000, true);	// Required for SRAM @ 200ns

	// Init GPIOs before starting the second core and FreeRTOS
	for (int i = 0; i <= 27; i++) {
		gpio_init(i);
	}

	// Set up ROM mapping table
	if (memcmp(picocart_header, "picocartcompress", 16) == 0) {
		// Copy rom compressed map from flash into RAM
		memcpy(rom_mapping, flash_rom_mapping, MAPPING_TABLE_LEN * sizeof(uint16_t));
	} else {
		for (int i = 0; i < MAPPING_TABLE_LEN; i++) {
			rom_mapping[i] = i;
		}
	}

	// Init UART on pin 25/23
	stdio_async_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
	printf("OpenPico64 Boot (git rev %08x)\r\n", GIT_REV);
	printf("  CPU_FREQ_MHZ=%d\n", CONFIG_CPU_FREQ_MHZ);
	printf("  ROM_HEADER_OVERRIDE=%08lX\n", CONFIG_ROM_HEADER_OVERRIDE);

#if ENABLE_N64_PI
	// Launch the N64 PI implementation in the second core
	// Note! You have to power reset the pico after flashing it with a jlink,
	//       otherwise multicore doesn't work properly.
	//       Alternatively, attach gdb to openocd, run `mon reset halt`, `c`.
	//       It seems this works around the issue as well.
	multicore_launch_core1(n64_pi_run);
#endif

	// Start FreeRTOS on Core0
	vLaunch();

	return 0;
}
