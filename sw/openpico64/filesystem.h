/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 JPZV
 */

#pragma once

#include "n64_defs.h"

void fs_init(void);
bool fs_is_game_loaded();
char* fs_get_current_dir_path();
char** fs_get_current_dir_content();
uint16_t fs_get_current_dir_content_count();
char* fs_get_current_game_path();
uint16_t fs_get_current_status();
uint16_t fs_get_game_chunk(uint32_t addr, bool with_cache);