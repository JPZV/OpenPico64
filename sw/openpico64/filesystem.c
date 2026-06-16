/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 JPZV
 */

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"

#include "f_util.h"
#include "ff.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include "sd_card.h"
#include "task.h"

#include "filesystem.h"

#define GAME_CHUNK_CACHE_SIZE   4096 // 64 * 64 = 4KiB

void watchdog_task_entry(__unused void *params);

char *fs_current_dir;

static uint16_t fs_dir_content_count = 0;
static char **fs_dir_content_array = NULL;

static StaticTask_t watchdog_task;
static StackType_t fs_watchdog_task_stack[4 * 1024 / sizeof(StackType_t)];

static sd_card_t *uSD = NULL;

char *fs_current_game_path;
static FIL *fs_current_game = NULL;
static uint32_t fs_game_current_chunk = 0;
static uint16_t fs_game_chunk_cache[GAME_CHUNK_CACHE_SIZE];
static uint32_t fs_game_chunk_cache_limits[2];

// This is used mainly for debug purposes
static uint16_t current_status = FS_UNKNOWN;

void fs_init(void)
{
    // Already initialized. We shouldn't initialize again
    if (uSD != NULL) return;

    fs_current_dir = (char*)calloc(SD_PATH_LENGTH + 1, sizeof(char));
    stpcpy(fs_current_dir, "/");
    fs_current_game_path = (char*)calloc(SD_GAME_PATH_LENGTH + 1, sizeof(char));

    current_status = FS_STARTING;
    uSD = sd_get_by_num(0);
    current_status = FS_SD_SELECTED;
    xTaskCreateStatic(watchdog_task_entry, "fs_watchdog", 1024, NULL, 2, fs_watchdog_task_stack, &watchdog_task);
}

bool fs_is_game_loaded()
{
    return fs_current_game != NULL && current_status == ((FS_GAME_WAITING_RESET << 8) | FS_OK);
}

char* fs_get_current_dir_path()
{
    return fs_current_dir;
}

char** fs_get_current_dir_content()
{
    return fs_dir_content_array;
}

uint16_t fs_get_current_dir_content_count()
{
    return fs_dir_content_count;
}

char* fs_get_current_game_path()
{
    return fs_current_game_path;
}

uint16_t fs_get_current_status()
{
    return current_status;
}

uint16_t fs_get_game_chunk(uint32_t addr, bool with_cache)
{
    if (fs_current_game == NULL)
    {
        return 0;
    }
    if (with_cache && fs_game_chunk_cache_limits[0] <= addr && addr < fs_game_chunk_cache_limits[1])
    {
        return (fs_game_chunk_cache[addr - fs_game_chunk_cache_limits[0]] << 8) | (fs_game_chunk_cache[addr - fs_game_chunk_cache_limits[0]] >> 8);
    }
    static FRESULT fr;
    if (fs_game_current_chunk != addr)
    {
        fr = f_lseek(fs_current_game, addr);
        if (fr != FR_OK)
        {
            return 0;
        }
        fs_game_current_chunk = addr;
    }
    static unsigned int buffRead = 0;
    fr = f_read(fs_current_game, &fs_game_chunk_cache, sizeof(fs_game_chunk_cache), &buffRead);
    fs_game_current_chunk += buffRead;
    fs_game_chunk_cache_limits[0] = addr;
    fs_game_chunk_cache_limits[1] = addr + (buffRead / sizeof(uint16_t));
    return (fs_game_chunk_cache[0] << 8) | (fs_game_chunk_cache[0] >> 8);
}

void watchdog_task_entry(__unused void *params)
{
    char *current_dir = (char*)calloc(SD_PATH_LENGTH + 1, sizeof(char));
    static FRESULT fr;      // Return value
    static DIR dir;         // Directory object
    static FILINFO file;    // File information
    current_status = FS_TASK_ENTRY;
	while (1)
    {
        if (fs_dir_content_array == NULL || strcmp(fs_current_dir, current_dir) != 0)
        {
            current_status = FS_INIT_DRIVER;
            if (!sd_init_driver()) goto while_end;
            current_status = FS_MOUNTING;
            if (uSD->fatfs.fs_type == 0 && (fr = f_mount(&uSD->fatfs, uSD->pcName, 1)) != FR_OK)
            {
                current_status = (fr << 8) | FS_FAILED_MOUNTING;
                goto while_end;
            }

            memset(&dir, 0, sizeof(dir));
            memset(&file, 0, sizeof(file));

            current_status = FS_LOADING_FIRST_ITEM;

            strncpy(current_dir, fs_current_dir, SD_PATH_LENGTH + 1);

            fr = f_findfirst(&dir, &file, current_dir, "*");
            if (FR_OK != fr)
            {
                current_status = (fr << 8) | FS_FAILED_LOADING_FIRST_ITEM_COUNT;
                goto while_end;
            }

            current_status = FS_CLEANING_CONTENT_ARRAY;
            // This if may provoke a memory leak... I hope it doesn't happen
            if (fs_dir_content_array != NULL && fs_dir_content_count > 0)
            {
                for (uint16_t i = 0; i < fs_dir_content_count; i++) free(fs_dir_content_array[i]);
                free(fs_dir_content_array);
            }
            else if (fs_dir_content_array != NULL)
            {
                free(fs_dir_content_array);
            }

            current_status = FS_COUNTING_CONTENT;
            fs_dir_content_count = 0;
            while (fr == FR_OK && file.fname[0])
            {
                fs_dir_content_count++;
                fr = f_findnext(&dir, &file);
            }
            f_closedir(&dir);

            memset(&dir, 0, sizeof(dir));
            memset(&file, 0, sizeof(file));

            fr = f_findfirst(&dir, &file, current_dir, "*");
            if (FR_OK != fr)
            {
                current_status = (fr << 8) | FS_FAILED_LOADING_FIRST_ITEM_LIST;
                goto while_end;
            }

            current_status = FS_CREATING_CONTENT_ARRAY;
            fs_dir_content_array = (char**)calloc(fs_dir_content_count > 0 ? fs_dir_content_count : 1, sizeof(char*));
            uint16_t content_pos = 0;
            /* Repeat while an item is found */
            while (fr == FR_OK && file.fname[0] && content_pos < fs_dir_content_count)
            {
                fs_dir_content_array[content_pos] = (char*)calloc(strlen(file.fname) + 1 + (file.fattrib & AM_DIR ? 1 : 0), sizeof(char)); // '/' + '\0' = 2
                if (fs_dir_content_array[content_pos] == NULL)
                {
                    // If it fails, then we don't have more memory left!!!!
                    break;
                }
                
                strcpy(fs_dir_content_array[content_pos], file.fname);

                /* Point pcAttrib to a string that describes the file. */
                if (file.fattrib & AM_DIR)
                {
                    strcat(fs_dir_content_array[content_pos], "/");
                }

                fr = f_findnext(&dir, &file); /* Search for next item */
                content_pos++;
            }
            f_closedir(&dir);

            current_status = FS_OK;
        }

        // Only load the game when the list was loaded correctly, the game's path is not empty, and the game wasn't loaded already
        if ((current_status & 0xFF) == FS_OK && fs_current_game_path[0] && fs_current_game == NULL)
        {
            current_status = (FS_GAME_STARTING << 8) | FS_OK;
            fs_current_game = (FIL*)calloc(1, sizeof(FIL));
            fr = f_open(fs_current_game, fs_current_game_path, FA_READ);
            if (fr != FR_OK)
            {
                // In theory, this may provoke and overflow, but fr shouldn't reach over 0xF when using f_open... or that I hope for.
                current_status = ((fr <= 0xF ? fr : 0xF) << 12) | (FS_GAME_OPEN_FAILED << 8) | FS_OK;
                fs_current_game_path[0] = '\0';
                free(fs_current_game);
                fs_current_game = NULL;
                goto while_end;
            }
            fs_get_game_chunk(0, false);
            current_status = (FS_GAME_WAITING_RESET << 8) | FS_OK;
        }

        while_end:
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}