/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Kaili Hill
 * Copyright (c) 2026 JPZV
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <libdragon.h>

#include "git_info.h"
#include "n64_defs.h"
#include "shell.h"
#include "testrom.h"

/*
TODO
Load selected rom into memory and start
*/

#define ENTRY_BUFFER_SIZE 256

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 240

/* Layout */
#define ROW_HEIGHT          12
#define MARGIN_PADDING      20
#define ROW_SELECTION_WIDTH SCREEN_WIDTH
#define MENU_BAR_HEIGHT     15
#define LIST_TOP_PADDING    (MENU_BAR_HEIGHT + ROW_HEIGHT)

#define DEBUG_MENU_WIDTH    (250)
#define DEBUG_MENU_HEIGHT   80
#define DEBUG_TEXT_LENGTH   100

#define FS_STATUS           ((*current_status) & 0xFF)
#define FS_GAME_STATUS      ((*current_status) >> 8)

/* Functions */
bool str_ends_with(const char *str, const char *suffix);

/* Colors */
color_t DEBUG_COLOR = {.r = 0xFF,.g = 0x00,.b = 0x00,.a = 0x0F };    // 0xFF0000, RED
color_t MENU_BAR_COLOR = {.r = 0x82,.g = 0x00,.b = 0x2E,.a = 0x00 };    // 0x82002E, Berry
color_t SELECTION_COLOR = {.r = 0x00,.g = 0x67,.b = 0xC7,.a = 0x00 };    // 0x0067C7, Bright Blue

static uint32_t debug_counter = 0;

static char **entries = NULL;
static uint16_t num_entries = 0;

// DON'T MAKE THESE AS STATIC!!! I hate memory management (╯‵□′)╯︵┻━┻
uint16_t tmp_num_entries = 0;
char __attribute__((aligned(16))) entry_buf[ENTRY_BUFFER_SIZE];

char __attribute__((aligned(16))) current_path_tmp[SD_PATH_LENGTH];
char __attribute__((aligned(16))) current_path[SD_PATH_LENGTH];
uint16_t *current_status;

static int currently_selected = 0;
static int first_visible = 0;
static int max_on_screen = 16;
bool game_error_displayed = false;

char error_message[SD_PATH_LENGTH / 2];
bool show_debug_info = true;

/* Assume default font size*/
static int calculate_num_rows_per_page(void)
{
    // ---top of screen---
    // menu bar
    // padding + more files above indicator space
    // list of files...
    // ...
    // more file indicator space
    // ---bottom of screen---

    // Start by subtracting the menu bar height, and paddings from the total height
    int availableHeight = SCREEN_HEIGHT - MENU_BAR_HEIGHT - (ROW_HEIGHT * 2);

    // Since there is currently no other dynamic portion, we can use a simple calculation to find rows that will fit in the space
    int rows = availableHeight / ROW_HEIGHT;

    return rows;
}

void change_dir(const char *path)
{
    pi_write_raw(path, SD_PATH_ADDR_START, 0, SD_PATH_LENGTH);
}

void change_game(const char *game_name)
{
    strcpy(current_path_tmp, current_path);
    strcat(current_path_tmp, game_name);
    pi_write_raw(current_path_tmp, SD_GAME_PATH_ADDR_START, 0, SD_GAME_PATH_LENGTH);
}

static void clear_entries()
{
    if (entries != NULL)
    {
        for (size_t i = 0; i < num_entries; i++)
        {
            if (entries[i] != NULL)
            {
                free(entries[i]);
                entries[i] = NULL;
            }
        }
        free(entries);
        entries = NULL;
    }
}

static void draw_debug_box(display_context_t display)
{
    static int x = SCREEN_WIDTH - DEBUG_MENU_WIDTH,
               y = MENU_BAR_HEIGHT,
               width = DEBUG_MENU_WIDTH,
               height = DEBUG_MENU_HEIGHT;
    graphics_draw_box(display, x, y, width, height, graphics_convert_color(DEBUG_COLOR));

    static char debugTextBuffer[DEBUG_TEXT_LENGTH];
    snprintf(debugTextBuffer, DEBUG_TEXT_LENGTH, "debug_counter=%lu\nnum_entries=%d\nstatus=0x%X (%p)\n", debug_counter, num_entries, *current_status, current_status);
    graphics_draw_text(display, x + 5, y + 5, debugTextBuffer);

    memset(debugTextBuffer, 0, DEBUG_TEXT_LENGTH);
    snprintf(debugTextBuffer, DEBUG_TEXT_LENGTH, "currently_selected=%d\nfirst_visible=%d\nmax_per_page=%d", currently_selected, first_visible, max_on_screen);
    graphics_draw_text(display, x + 5, y + 30, debugTextBuffer);
}

static void draw_header_bar(display_context_t display, int fileCount)
{
    static int x = 0,
               y = 0,
               width = SCREEN_WIDTH,
               height = MENU_BAR_HEIGHT;
    graphics_draw_box(display, x, y, width, height, graphics_convert_color(MENU_BAR_COLOR));

    char menuHeaderBuffer[100];
    sprintf(menuHeaderBuffer, "PicoCart64 OS (git rev %08x)\t\t\t\t%d Files", GIT_REV, fileCount);
    graphics_draw_text(display, MARGIN_PADDING, y + 4, menuHeaderBuffer);
}

static void load_entries()
{
    debug_counter++;
    clear_entries();

    if (FS_STATUS != FS_OK) return;

    tmp_num_entries = 0;
    data_cache_hit_writeback_invalidate(&tmp_num_entries, sizeof(uint16_t));
    pi_read_raw(&tmp_num_entries, SD_DIR_LIST_LEN_ADDR_START, 0, sizeof(uint16_t));
    num_entries = tmp_num_entries;
    
    if (tmp_num_entries == 0)
    {
        entries = (char**)calloc(1, sizeof(char)); //We allocate a single byte to avoid calling this function again
        return;
    }

    entries = (char**)calloc(num_entries, sizeof(char*));
    
    for (uint16_t i = 0; i < num_entries; i++)
    {
        data_cache_hit_writeback_invalidate(entry_buf, ENTRY_BUFFER_SIZE);
        // DON'T ASK ME WHY I HAVE TO DO THIS BULLS**T OF (i + 1) * 2, because I have NO F**** IDEA
        pi_read_raw(entry_buf, SD_DIR_LIST_ADDR_START, (i + 1) * 2, ENTRY_BUFFER_SIZE);
        entries[i] = strdup(entry_buf);
    }
}

void navigate(char* folder_name)
{
    if (!folder_name) return;
    if (!strcmp(folder_name, ".."))
    {
        // Go back
        uint16_t slash_pos = 1;
        uint16_t prev_slash_pos = 1;
        uint16_t i;
        for (i = 0; i < SD_PATH_LENGTH; i++)
        {
            if (current_path[i] == 0) break;
            if (current_path[i] == '/')
            {
                prev_slash_pos = slash_pos;
                slash_pos = i;
            }
        }
        memset(&(current_path[prev_slash_pos + 1]), 0, sizeof(char) * (SD_PATH_LENGTH - prev_slash_pos + 1));
    }
    else
    {
        // Go to folder
        strcat(current_path, folder_name);
    }
    change_dir(current_path);
    clear_entries(); // TODO: This line breaks the code somehow
}

/*
 * Render a list of strings and show a caret on the currently selected row
 */
static void render_list(display_context_t display, char **list, int currently_selected, int first_visible, int max_on_screen)
{
    if (list == NULL) return;

    /* If we aren't starting at the first row, draw an indicator to show more files above */
    if (first_visible > 0) {
        graphics_draw_text(display, MARGIN_PADDING, MENU_BAR_HEIGHT, "/\\");
    }

    bool is_folder = current_path[1] != 0;

    for (int i = 0; i < max_on_screen; i++) {
        int row = first_visible + i;
        // if we run out of items to draw on the screen, don't draw anything else
        if (row > (num_entries + (is_folder ? 1 : 0))) {
            break;
        }

        int x = currently_selected == row ? MARGIN_PADDING : 0; // if the current item is selected move it over so we can show a selection caret
        int y = i * ROW_HEIGHT + LIST_TOP_PADDING; // First row must start below the menu bar, so add that plus a pad

        if (currently_selected == row) {
            /* Render selection box */
            graphics_draw_box(display, 0, y - 2, ROW_SELECTION_WIDTH, 10, graphics_convert_color(SELECTION_COLOR));

            /* Now render selection caret */
            graphics_draw_text(display, MARGIN_PADDING / 2, y, "  >");

            /* Increase Margin Padding to account for added caret width */
            x += MARGIN_PADDING;
        } else {
            x += MARGIN_PADDING;
        }

        // Render the list item
        if (is_folder && row == 0)
        {
            graphics_draw_text(display, x, y, "..");            
        }
        else
        {
            graphics_draw_text(display, x, y, list[row - (is_folder ? 1 : 0)]);
        }

        /* If this is the last row and there are more files below, draw an indicator */
        if (row + 1 < num_entries && i + 1 >= max_on_screen) {
            graphics_draw_text(display, MARGIN_PADDING, y + 10, "\\/");
        }
    }
}

/*
 * Init display and controller input then render a list of strings, showing currently selected
 */
static void show_list(void)
{
    currently_selected = 0;
    first_visible = 0;
    max_on_screen = calculate_num_rows_per_page();
    while (1)
    {
        static display_context_t display = 0;

        /* Grab a render buffer */
        while (!(display = display_lock()));

        /* Grab controller input and move the selection up or down */
        controller_scan();
        struct controller_data keys = get_keys_down();
        int mag = 0;
        if (keys.c[0].down)
        {
            mag = 1;
            error_message[0] = 0;
        }
        else if (keys.c[0].up)
        {
            mag = -1;
            error_message[0] = 0;
        }
        if (keys.c[0].start && keys.c[0].left)
        {
            show_debug_info = !show_debug_info;
            error_message[0] = 0;
        }
        if (keys.c[0].A)
        {
            error_message[0] = 0;
            bool is_folder = current_path[1] != 0;
            if (is_folder && currently_selected == 0)
            {
                // Go back
                navigate("..");
            }
            else if (str_ends_with(entries[currently_selected - (is_folder ? 1 : 0)], "/"))
            {
                // Change dir
                navigate(entries[currently_selected - (is_folder ? 1 : 0)]);
            }
            else if (str_ends_with(entries[currently_selected - (is_folder ? 1 : 0)], ".z64"))
            {
                // Load game
                change_game(entries[currently_selected - (is_folder ? 1 : 0)]);
                game_error_displayed = false;
            }
            else
            {
                // Show error message
                strcpy(error_message, "Only .z64 roms are supported");
            }
        }

        *current_status = 0;
        data_cache_hit_writeback_invalidate(current_status, sizeof(uint16_t));
        pi_read_raw(current_status, SD_STATUS_ADDR_START, 0, sizeof(uint16_t));

        if (FS_STATUS == FS_OK && (FS_GAME_STATUS == FS_GAME_STARTING || FS_GAME_STATUS == FS_GAME_OPENING))
        {
            strcpy(error_message, "Loading game...");
        }
        if (FS_STATUS == FS_OK && FS_GAME_STATUS == FS_GAME_OPEN_FAILED && !game_error_displayed)
        {
            strcpy(error_message, "Failed to load the game!");
            game_error_displayed = true;
        }

        if (FS_STATUS == FS_OK && FS_GAME_STATUS == FS_GAME_WAITING_RESET)
        {
            graphics_fill_screen(display, graphics_make_color(0, 255, 0, 255));
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (22 * 4), SCREEN_HEIGHT / 2 - 10, "Game loaded correctly!");
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (45 * 4), SCREEN_HEIGHT / 2, "Please press the RESET button in your console");
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (25 * 4), SCREEN_HEIGHT / 2 + 10, "in order to load the game");

            /* Display DEBUG menu box with 'useful' info*/
            if (show_debug_info) draw_debug_box(display);

            display_show(display);
            continue;
        }

        /* Load entries from SD Card */
        if (entries == NULL || num_entries == 0) load_entries();

        if (entries == NULL)
        {
            graphics_fill_screen(display, graphics_make_color(255, 0, 0, 255));
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (15 * 4), SCREEN_HEIGHT / 2 - 10, "No SD available");
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (37 * 4), SCREEN_HEIGHT / 2, "Check your SD Card and reset your N64");
            static char errorTextBuffer[DEBUG_TEXT_LENGTH];
            snprintf(errorTextBuffer, DEBUG_TEXT_LENGTH, "Error code: 0x%X\n", *current_status);
            graphics_draw_text(display, SCREEN_WIDTH / 2 - (strlen(errorTextBuffer) * 4), SCREEN_HEIGHT / 2 + 10, errorTextBuffer);

            /* Display DEBUG menu box with 'useful' info*/
            if (show_debug_info) draw_debug_box(display);

            display_show(display);
            continue;
        }

        /* Clear the screen */
        graphics_fill_screen(display, 0);

        /* Draw top header bar */
        draw_header_bar(display, num_entries);

        /* Render the list of file */
        render_list(display, entries, currently_selected, first_visible, max_on_screen);

        if ((mag > 0 && currently_selected + mag < num_entries) || (mag < 0 && currently_selected > 0)) {
            currently_selected += mag;
        }
        // If we have moved the cursor to an entry not yet visible on screen, move first_visible as well
        if ((mag > 0 && currently_selected >= (first_visible + max_on_screen)) || (mag < 0 && currently_selected < first_visible && currently_selected >= 0)) {
            first_visible += mag;
        }

        /* A little text at the bottom of the screen */
        if (error_message[0] == 0)
        {
            graphics_draw_text(display, 5, 227, current_path);
        }
        else
        {
            graphics_draw_box(display, 0, 225, SCREEN_WIDTH, SCREEN_HEIGHT - 225, graphics_convert_color(DEBUG_COLOR));
            graphics_draw_text(display, 5, 227, error_message);
        }

        /* Display DEBUG menu box with 'useful' info*/
        if (show_debug_info) draw_debug_box(display);

        /* Force the backbuffer flip */
        display_show(display);
    }
}

void start_shell(void)
{
    /* Init the screen and controller */
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    controller_init();

    current_status = (uint16_t*)malloc(sizeof(uint16_t));
    *current_status = FS_UNKNOWN;
    strcpy(current_path, "/");

    change_dir(current_path);

    /* Starts the shell by rendering the list of files from the SD card */
    show_list();
}

// Source: https://stackoverflow.com/a/744822/4668642
bool str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr) return false;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}