/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 * Copyright (c) 2026 JPZV
 */

#pragma once

// N64DD control registers
#define CART_DOM2_ADDR1_START       0x05000000
#define CART_DOM2_ADDR1_END         0x05FFFFFF

// N64DD IPL ROM
#define CART_DOM1_ADDR1_START       0x06000000
#define CART_DOM1_ADDR1_END         0x07FFFFFF

// Cartridge SRAM
#define CART_DOM2_ADDR2_START       0x08000000
#define CART_DOM2_ADDR2_END         0x0FFFFFFF

// Define the highest supported banked SRAM addresses
#define CART_SRAM_START             CART_DOM2_ADDR2_START
#define CART_SRAM_END               (CART_DOM2_ADDR2_START + 0x100000 - 1)

// Cartridge ROM
#define CART_DOM1_ADDR2_START       0x10000000
#define CART_DOM1_ADDR2_END         0x1FBFFFFF


// SD Content
#define SD_ADDR_START               0x1FD00000

#define SD_PATH_ADDR_START          SD_ADDR_START
#define SD_PATH_LENGTH              256
#define SD_PATH_ADDR_END            (SD_PATH_ADDR_START + SD_PATH_LENGTH + 1)

#define SD_GAME_PATH_ADDR_START     0x1FD01000
#define SD_GAME_PATH_LENGTH         256
#define SD_GAME_PATH_ADDR_END       (SD_GAME_PATH_ADDR_START + SD_GAME_PATH_LENGTH + 1)

#define SD_STATUS_ADDR_START        0x1FD02000
#define SD_STATUS_ADDR_END          (SD_STATUS_ADDR_START + 2)

#define SD_DIR_LIST_LEN_ADDR_START  0x1FD03000
#define SD_DIR_LIST_LEN_ADDR_END    (SD_DIR_LIST_LEN_ADDR_START + 2)

#define SD_DIR_LIST_ADDR_START      0x1FD04000
#define SD_DIR_LIST_LENGTH          0x20000
#define SD_DIR_LIST_ADDR_END        (SD_DIR_LIST_ADDR_START + SD_DIR_LIST_LENGTH)

#define SD_ADDR_END                 (SD_DIR_LIST_LEN_ADDR_END + 0x100000)


// Unknown
#define CART_DOM1_ADDR3_START       SD_ADDR_END
#define CART_DOM1_ADDR3_END         0x7FFFFFFF


typedef enum
{
    FS_OK,
    FS_STARTING,
    FS_SD_SELECTED,
    FS_LOADING_CONTENT,
    FS_INIT_DRIVER,
    FS_MOUNTING,
    FS_LOADING_FIRST_ITEM,
    FS_CLEANING_CONTENT_ARRAY,
    FS_COUNTING_CONTENT,
    FS_CREATING_CONTENT_ARRAY,
    // Every error MUST be below this line
    FS_TASK_ENTRY, // Funny thing: This line SHOULD only be called once at the very beginning of the code.
                   // So, it's an error if we get that status
    FS_FAILED_MOUNTING,
    FS_FAILED_LOADING_FIRST_ITEM_COUNT,
    FS_FAILED_LOADING_FIRST_ITEM_LIST,
    FS_UNKNOWN
} FS_STATUS;

typedef enum
{
    FS_GAME_OK,
    FS_GAME_STARTING,
    FS_GAME_OPENING,
    FS_GAME_WAITING_RESET,
    // Every error MUST be below this line
    FS_GAME_OPEN_FAILED,
    FS_GAME_UNKNOWN
} FS_GAME_STATUS;