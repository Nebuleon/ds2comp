/* gui.c
 *
 * Copyright (C) 2010 dking <dking024@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "gui.h"

#include <ds2/ds.h>
#include <ds2/pm.h>
#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "draw.h"
#include "message.h"
#include "bitmap.h"

#include "minigzip.h"
#include "miniunz.h"

char main_path[PATH_MAX];

// If adding a language, make sure you update the size of the array in
// message.h too.
const char *lang[LANG_END] =
{
	"English",   // 0
	"Français",  // 1
	"Español",   // 2
	"Deutsch",   // 3
};

const char *msg[MSG_END + 1];
char msg_data[16 * 1024] __attribute__((section(".noinit")));

/******************************************************************************
*	Macro definition
 ******************************************************************************/
#define DS2COMP_VERSION "0.62"

#define LANGUAGE_PACK   "SYSTEM/language.msg"
#define APPLICATION_CONFIG_FILENAME "SYSTEM/ds2comp.cfg"

#define APPLICATION_CONFIG_HEADER  "D2CM1.0"
#define APPLICATION_CONFIG_HEADER_SIZE 7
APPLICATION_CONFIG application_config;

#define SUBMENU_ROW_NUM 8
#define FILE_LIST_ROWS 8

// These are U+05C8 and subsequent codepoints encoded in UTF-8.
// They are null-terminated.
const char HOTKEY_A_DISPLAY[] = "\xD7\x88";
const char HOTKEY_B_DISPLAY[] = "\xD7\x89";
const char HOTKEY_X_DISPLAY[] = "\xD7\x8A";
const char HOTKEY_Y_DISPLAY[] = "\xD7\x8B";
const char HOTKEY_L_DISPLAY[] = "\xD7\x8C";
const char HOTKEY_R_DISPLAY[] = "\xD7\x8D";
const char HOTKEY_START_DISPLAY[] = "\xD7\x8E";
const char HOTKEY_SELECT_DISPLAY[] = "\xD7\x8F";
// These are U+2190 and subsequent codepoints encoded in UTF-8.
const char HOTKEY_LEFT_DISPLAY[] = "\xE2\x86\x90";
const char HOTKEY_UP_DISPLAY[] = "\xE2\x86\x91";
const char HOTKEY_RIGHT_DISPLAY[] = "\xE2\x86\x92";
const char HOTKEY_DOWN_DISPLAY[] = "\xE2\x86\x93";

/******************************************************************************
 ******************************************************************************/
char g_default_rom_dir[PATH_MAX];
/******************************************************************************
 ******************************************************************************/
static void init_application_config(void);
static int load_application_config_file(void);
static int save_application_config_file(void);
static void quit(void);

/*--------------------------------------------------------
	Get GUI input
--------------------------------------------------------*/

typedef enum
{
  BUTTON_NOT_HELD,
  BUTTON_HELD_INITIAL,
  BUTTON_HELD_REPEAT
} button_repeat_state_type;

button_repeat_state_type button_repeat_state = BUTTON_NOT_HELD;
clock_t button_repeat_timestamp;
uint16_t gui_button_repeat = 0;

gui_action_type key_to_cursor(uint16_t key)
{
	switch (key)
	{
		case DS_BUTTON_UP:	return CURSOR_UP;
		case DS_BUTTON_DOWN:	return CURSOR_DOWN;
		case DS_BUTTON_LEFT:	return CURSOR_LEFT;
		case DS_BUTTON_RIGHT:	return CURSOR_RIGHT;
		case DS_BUTTON_L:	return CURSOR_LTRIGGER;
		case DS_BUTTON_R:	return CURSOR_RTRIGGER;
		case DS_BUTTON_A:	return CURSOR_SELECT;
		case DS_BUTTON_B:	return CURSOR_BACK;
		case DS_BUTTON_X:	return CURSOR_EXIT;
		case DS_BUTTON_TOUCH:	return CURSOR_TOUCH;
		default:	return CURSOR_NONE;
	}
}

static uint16_t gui_keys[] = {
	DS_BUTTON_A, DS_BUTTON_B, DS_BUTTON_X, DS_BUTTON_L, DS_BUTTON_R, DS_BUTTON_TOUCH, DS_BUTTON_UP, DS_BUTTON_DOWN, DS_BUTTON_LEFT, DS_BUTTON_RIGHT
};

gui_action_type get_gui_input(void)
{
	size_t i;
	struct DS_InputState inputdata;
	DS2_GetInputState(&inputdata);

	if (inputdata.buttons & DS_BUTTON_LID) {
		DS2_SystemSleep();
	}

	while (1) {
		switch (button_repeat_state) {
		case BUTTON_NOT_HELD:
			// Pick the first pressed button out of the gui_keys array.
			for (i = 0; i < sizeof(gui_keys) / sizeof(gui_keys[0]); i++) {
				if (inputdata.buttons & gui_keys[i]) {
					button_repeat_state = BUTTON_HELD_INITIAL;
					button_repeat_timestamp = clock();
					gui_button_repeat = gui_keys[i];
					return key_to_cursor(gui_keys[i]);
				}
			}
			return CURSOR_NONE;
		case BUTTON_HELD_INITIAL:
		case BUTTON_HELD_REPEAT:
			// If the key that was being held isn't anymore...
			if (!(inputdata.buttons & gui_button_repeat)) {
				button_repeat_state = BUTTON_NOT_HELD;
				// Go see if another key is held (try #2)
				break;
			}
			else
			{
				bool IsRepeatReady = clock() - button_repeat_timestamp >= (button_repeat_state == BUTTON_HELD_INITIAL ? BUTTON_REPEAT_START : BUTTON_REPEAT_CONTINUE);
				if (!IsRepeatReady) {
					// Temporarily turn off the key.
					// It's not its turn to be repeated.
					inputdata.buttons &= ~gui_button_repeat;
				}
				
				// Pick the first pressed button out of the gui_keys array.
				for (i = 0; i < sizeof(gui_keys) / sizeof(gui_keys[0]); i++) {
					if (inputdata.buttons & gui_keys[i]) {
						// If it's the held key,
						// it's now repeating quickly.
						button_repeat_state = gui_keys[i] == gui_button_repeat ? BUTTON_HELD_REPEAT : BUTTON_HELD_INITIAL;
						button_repeat_timestamp = clock();
						gui_button_repeat = gui_keys[i];
						return key_to_cursor(gui_keys[i]);
					}
				}
				// If it was time for the repeat but it
				// didn't occur, stop repeating.
				if (IsRepeatReady) button_repeat_state = BUTTON_NOT_HELD;
				return CURSOR_NONE;
			}
		}
	}
}

static char ProgressAction[64];
static char ProgressFilename[PATH_MAX + 1];
static unsigned int ProgressCurrentFile; // 1-based
static unsigned int ProgressTotalFiles;
static unsigned int ProgressTotalSize;
static unsigned int ProgressDoneSize;
static clock_t LastProgressUpdateTime;

void InitProgress(const char *Action, const char *Filename, unsigned int TotalSize)
{
	strcpy(ProgressAction, Action);
	strcpy(ProgressFilename, Filename);
	ProgressTotalSize = TotalSize;
	LastProgressUpdateTime = 0;

	UpdateProgress(0);
}

void InitProgressMultiFile(const char *Action, const char *Filename, unsigned int TotalFiles)
{
	strcpy(ProgressAction, Action);
	strcpy(ProgressFilename, Filename);
	ProgressTotalFiles = TotalFiles;
}

void UpdateProgressChangeFile(unsigned int CurrentFile, const char *Filename, unsigned int TotalSize)
{
	ProgressCurrentFile = CurrentFile;
	strcpy(ProgressFilename, Filename);
	ProgressTotalSize = TotalSize;
	LastProgressUpdateTime = 0; // force an update when changing files
	// if this is too slow, move it to InitProgressMultiFile above

	UpdateProgressMultiFile(0);
}

#define PROGRESS_BAR_WIDTH (ICON_PROGRESS.x)

void UpdateProgress(unsigned int DoneSize)
{
	ProgressDoneSize = DoneSize;

	clock_t Now = clock();
	if (Now - LastProgressUpdateTime >= CLOCKS_PER_SEC / 4 /* 250 milliseconds */
	 || ProgressDoneSize == ProgressTotalSize /* force update if done */) {
		LastProgressUpdateTime = Now;
		// If you want to add skinning support for the upper screen, edit this.
		DS2_FillScreen(DS_ENGINE_MAIN, COLOR_BLACK);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 48, 254, COLOR_WHITE, ProgressAction);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 64, 254, COLOR_WHITE, ProgressFilename);

		char ByteCountLine[128];
		sprintf(ByteCountLine, msg[FMT_PROGRESS_KIBIBYTE_COUNT], ProgressDoneSize / 1024, ProgressTotalSize / 1024);
		draw_string_vcenter(DS2_GetMainScreen(), 1, 114, 254, COLOR_WHITE, ByteCountLine);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 130, 254, COLOR_WHITE, msg[MSG_PROGRESS_CANCEL_WITH_B]);

		unsigned int PixelsDone = (unsigned int) (((unsigned long long) ProgressDoneSize * (unsigned long long) PROGRESS_BAR_WIDTH) / (unsigned long long) ProgressTotalSize);

		show_icon(DS2_GetMainScreen(), &ICON_NPROGRESS, (DS_SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80);
		show_partial_icon_horizontal(DS2_GetMainScreen(), &ICON_PROGRESS, (DS_SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80, PixelsDone);

		DS2_FlipMainScreen();
	}
}

void UpdateProgressMultiFile(unsigned int DoneSize)
{
	ProgressDoneSize = DoneSize;

	clock_t Now = clock();
	if (Now - LastProgressUpdateTime >= CLOCKS_PER_SEC / 4 /* 250 milliseconds */
	 || ((ProgressDoneSize == ProgressTotalSize)
	  && (ProgressCurrentFile == ProgressTotalFiles)) /* force update if done */) {
		LastProgressUpdateTime = Now;
		// If you want to add skinning support for the upper screen, edit this.
		DS2_FillScreen(DS_ENGINE_MAIN, COLOR_BLACK);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 48, 254, COLOR_WHITE, ProgressAction);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 64, 254, COLOR_WHITE, ProgressFilename);

		char ByteCountLine[128];
		sprintf(ByteCountLine, msg[FMT_PROGRESS_ARCHIVE_MEMBER_AND_KIBIBYTE_COUNT], ProgressCurrentFile, ProgressTotalFiles, ProgressDoneSize / 1024, ProgressTotalSize / 1024);
		draw_string_vcenter(DS2_GetMainScreen(), 1, 114, 254, COLOR_WHITE, ByteCountLine);

		draw_string_vcenter(DS2_GetMainScreen(), 1, 130, 254, COLOR_WHITE, msg[MSG_PROGRESS_CANCEL_WITH_B]);

		unsigned int PixelsDone = (unsigned int) (((unsigned long long) ProgressDoneSize * (unsigned long long) PROGRESS_BAR_WIDTH) / (unsigned long long) ProgressTotalSize);

		show_icon(DS2_GetMainScreen(), &ICON_NPROGRESS, (DS_SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80);
		show_partial_icon_horizontal(DS2_GetMainScreen(), &ICON_PROGRESS, (DS_SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80, PixelsDone);

		DS2_FlipMainScreen();
	}
}

void InitMessage(void)
{
	DS2_SetScreenBacklights(DS_SCREEN_BOTH);
	DS2_AwaitScreenUpdate(DS_ENGINE_SUB);
	draw_message_box(DS2_GetSubScreen());

	DS2_LowClockSpeed();
}

void FiniMessage(void)
{
	DS2_AwaitNoButtons();
	DS2_HighClockSpeed();

	DS2_SetScreenBacklights(DS_SCREEN_UPPER);

	DS2_AwaitScreenUpdate(DS_ENGINE_SUB);
	DS2_FillScreen(DS_ENGINE_SUB, COLOR_BLACK);
	DS2_UpdateScreen(DS_ENGINE_SUB);
}

uint16_t ReadInputDuringCompression(void)
{
	struct DS_InputState input;

	DS2_GetInputState(&input);

	return input.buttons & ~DS_BUTTON_LID;
}

void change_ext(char *src, char *buffer, char *extension)
{
	char *dot_position;

	strcpy(buffer, src);
	dot_position = strrchr(buffer, '.');

	if(dot_position)
		strcpy(dot_position, extension);
}

struct selector_entry {
	char* name;
	bool  is_dir;
};

/*--------------------------------------------------------
	Sort function
--------------------------------------------------------*/
static int name_sort(const void* a, const void* b)
{
	return strcasecmp(((const struct selector_entry*) a)->name,
	                  ((const struct selector_entry*) b)->name);
}

/*
 * Shows a file selector interface.
 *
 * Input:
 *   exts: An array of const char* specifying the file extension filter.
 *     If the array is NULL, or the first entry is NULL, all files are shown
 *     regardless of extension.
 *     Otherwise, only files whose extensions match any of the entries, which
 *     must start with '.', are shown.
 * Input/output:
 *   dir: On entry to the function, the initial directory to be used.
 *     On exit, if a file was selected, the directory containing the selected
 *     file; otherwise, unchanged.
 * Output:
 *   result_name: If a file was selected, this is updated with the name of the
 *     selected file without its path; otherwise, unchanged.
 * Returns:
 *   0: a file was selected.
 *   -1: the user exited the selector without selecting a file.
 *   < -1: an error occurred.
 */
int32_t load_file(const char **exts, char *result_name, char *dir)
{
	if (dir == NULL || *dir == '\0')
		return -4;

	char cur_dir[PATH_MAX];
	bool continue_dir = true;
	int32_t ret;
	size_t i;
	void* scrollers[FILE_LIST_ROWS + 1 /* for the directory name */];

	strcpy(cur_dir, dir);

	for (i = 0; i < FILE_LIST_ROWS + 1; i++) {
		scrollers[i] = NULL;
	}

	while (continue_dir) {
		DS2_HighClockSpeed();
		// Read the current directory. This loop is continued every time the
		// current directory changes.

		show_icon(DS2_GetSubScreen(), &ICON_SUBBG, 0, 0);
		show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
		show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);
		PRINT_STRING_BG(DS2_GetSubScreen(), msg[MSG_FILE_MENU_LOADING_LIST], COLOR_WHITE, COLOR_TRANS, 49, 10);
		DS2_UpdateScreen(DS_ENGINE_SUB);
		DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

		clock_t last_display = clock();

		struct selector_entry* entries;
		DIR* cur_dir_handle = NULL;
		size_t count = 1, capacity = 4 /* initially */;

		entries = malloc(capacity * sizeof(struct selector_entry));
		if (entries == NULL) {
			ret = -2;
			continue_dir = false;
			goto cleanup;
		}

		entries[0].name = strdup("..");
		if (entries[0].name == NULL) {
			ret = -1;
			continue_dir = 0;
			goto cleanup;
		}
		entries[0].is_dir = true;

		cur_dir_handle = opendir(cur_dir);
		if (cur_dir_handle == NULL) {
			ret = -1;
			continue_dir = 0;
			goto cleanup;
		}

		struct dirent* cur_entry_handle;
		struct stat    st;

		while ((cur_entry_handle = readdir(cur_dir_handle)) != NULL)
		{
			clock_t now = clock();
			bool add = false;
			char* name = cur_entry_handle->d_name;
			char path[PATH_MAX];

			snprintf(path, PATH_MAX, "%s/%s", cur_dir, name);
			stat(path, &st);

			if (now >= last_display + CLOCKS_PER_SEC / 4) {
				last_display = now;

				show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
				show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);
				char line[384], element[16];
				strcpy(line, msg[MSG_FILE_MENU_LOADING_LIST]);
				sprintf(element, " (%" PRIu32 ")", count);
				strcat(line, element);
				PRINT_STRING_BG(DS2_GetSubScreen(), line, COLOR_WHITE, COLOR_TRANS, 49, 10);
				DS2_UpdateScreenPart(DS_ENGINE_SUB, 0, ICON_TITLEICON.y);
				DS2_AwaitScreenUpdate(DS_ENGINE_SUB);
			}

			if (S_ISDIR(st.st_mode)) {
				// Add directories no matter what, except for the special
				// ones, "." and "..".
				if (!(name[0] == '.' &&
				    (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))) {
					add = true;
				}
			} else {
				if (exts == NULL || exts[0] == NULL) // Show every file
					add = true;
				else {
					// Add files only if their extension is in the list.
					char* ext = strrchr(name, '.');
					if (ext != NULL) {
						for (i = 0; exts[i] != NULL; i++) {
							if (strcasecmp(ext, exts[i]) == 0) {
								add = true;
								break;
							}
						}
					}
				}
			}

			if (add) {
				// Ensure we have enough capacity in the selector_entry array.
				if (count == capacity) {
					struct selector_entry* new_entries = realloc(entries, capacity * 2 * sizeof(struct selector_entry));
					if (new_entries == NULL) {
						ret = -2;
						continue_dir = false;
						goto cleanup;
					} else {
						entries = new_entries;
						capacity *= 2;
					}
				}

				// Then add the entry.
				entries[count].name = strdup(name);
				if (entries[count].name == NULL) {
					ret = -2;
					continue_dir = 0;
					goto cleanup;
				}

				entries[count].is_dir = S_ISDIR(st.st_mode) ? true : false;

				count++;
			}
		}

		show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
		show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);
		PRINT_STRING_BG(DS2_GetSubScreen(), msg[MSG_FILE_MENU_SORTING_LIST], COLOR_WHITE, COLOR_TRANS, 49, 10);
		DS2_UpdateScreenPart(DS_ENGINE_SUB, 0, ICON_TITLEICON.y);
		DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

		/* skip the first entry when sorting, which is always ".." */
		qsort(&entries[1], count - 1, sizeof(struct selector_entry), name_sort);
		DS2_LowClockSpeed();

		bool continue_input = true;
		size_t sel_entry = 0, prev_sel_entry = 1 /* different to show scrollers to start */;
		uint32_t dir_scroll = 0x8000; // First scroll to the left
		int32_t entry_scroll = 0;

		scrollers[0] = draw_hscroll_init(DS2_GetSubScreen(), 49, 10, 199, COLOR_TRANS,
			COLOR_WHITE, cur_dir);

		if (scrollers[0] == NULL) {
			ret = -2;
			continue_dir = false;
			goto cleanupScrollers;
		}

		// Show the current directory and get input. This loop is continued
		// every frame, because the current directory scrolls atop the screen.

		while (continue_dir && continue_input) {
			// Try to use a row set such that the selected entry is in the
			// middle of the screen.
			size_t last_entry = sel_entry + FILE_LIST_ROWS / 2, first_entry;

			// If the last row is out of bounds, put it back in bounds.
			// (In this case, the user has selected an entry in the last
			// FILE_LIST_ROWS / 2.)
			if (last_entry >= count)
				last_entry = count - 1;

			if (last_entry < FILE_LIST_ROWS - 1) {
				/* Move to the first entry unconditionally. */
				first_entry = 0;
				// If there are more than FILE_LIST_ROWS / 2 files,
				// we need to enlarge the first page.
				last_entry = FILE_LIST_ROWS - 1;
				if (last_entry >= count) // No...
					last_entry = count - 1;
			} else
				first_entry = last_entry - (FILE_LIST_ROWS - 1);

			// Update scrollers.
			// a) If a different item has been selected, remake entry
			//    scrollers, resetting the formerly selected entry to the
			//    start and updating the selection color.
			if (sel_entry != prev_sel_entry) {
				// Preserve the directory scroller.
				for (i = 1; i < FILE_LIST_ROWS + 1; i++) {
					draw_hscroll_over(scrollers[i]);
					scrollers[i] = NULL;
				}
				for (i = first_entry; i <= last_entry; i++) {
					uint16_t color = (i == sel_entry)
						? COLOR_ACTIVE_ITEM
						: COLOR_INACTIVE_ITEM;
					scrollers[i - first_entry + 1] = hscroll_init(DS2_GetSubScreen(),
						FILE_SELECTOR_NAME_X,
						GUI_ROW1_Y + (i - first_entry) * GUI_ROW_SY + TEXT_OFFSET_Y,
						FILE_SELECTOR_NAME_SX,
						COLOR_TRANS,
						color,
						entries[i].name);
					if (scrollers[i - first_entry + 1] == NULL) {
						ret = -2;
						continue_dir = 0;
						goto cleanupScrollers;
					}
				}

				prev_sel_entry = sel_entry;
			}

			// b) Must we update the directory scroller?
			if ((dir_scroll & 0xFF) >= 0x20) {
				if (dir_scroll & 0x8000) {  /* scrolling to the left */
					if (draw_hscroll(scrollers[0], -1) == 0) dir_scroll = 0;
				} else {  /* scrolling to the right */
					if (draw_hscroll(scrollers[0], 1) == 0) dir_scroll = 0x8000;
				}
			} else {
				// Wait one less frame before scrolling the directory again.
				dir_scroll++;
			}

			// c) Must we scroll the current file as a result of user input?
			if (entry_scroll != 0) {
				draw_hscroll(scrollers[sel_entry - first_entry + 1], entry_scroll);
				entry_scroll = 0;
			}

			// Draw.
			// a) The background.
			show_icon(DS2_GetSubScreen(), &ICON_SUBBG, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);

			// b) The selection background.
			show_icon(DS2_GetSubScreen(), &ICON_SUBSELA, SUBSELA_X, GUI_ROW1_Y + (sel_entry - first_entry) * GUI_ROW_SY + SUBSELA_OFFSET_Y);

			// c) The scrollers.
			for (i = 0; i < FILE_LIST_ROWS + 1; i++)
				draw_hscroll(scrollers[i], 0);

			// d) The icons.
			for (i = first_entry; i <= last_entry; i++)
			{
				struct gui_icon* icon;
				if (i == 0)
					icon = &ICON_DOTDIR;
				else if (entries[i].is_dir)
					icon = &ICON_DIRECTORY;
				else {
					char* ext = strrchr(entries[i].name, '.');
					if (ext != NULL) {
						if (strcasecmp(ext, ".zip") == 0)
							icon = &ICON_ZIPFILE;
						else
							icon = &ICON_UNKNOW;
					} else
						icon = &ICON_UNKNOW;
				}

				show_icon(DS2_GetSubScreen(), icon, FILE_SELECTOR_ICON_X, GUI_ROW1_Y + (i - first_entry) * GUI_ROW_SY + FILE_SELECTOR_ICON_Y);
			}

			DS2_UpdateScreen(DS_ENGINE_SUB);
			DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

			struct DS_InputState inputdata;
			gui_action_type gui_action = get_gui_input();
			DS2_GetInputState(&inputdata);

			// Get DS_BUTTON_RIGHT and DS_BUTTON_LEFT separately to allow scrolling
			// the selected file name faster.
			if (inputdata.buttons & DS_BUTTON_RIGHT)
				entry_scroll = -3;
			else if (inputdata.buttons & DS_BUTTON_LEFT)
				entry_scroll = 3;

			switch (gui_action) {
				case CURSOR_TOUCH:
				{
					DS2_AwaitNoButtons();
					// ___ 33        This screen has 6 possible rows. Touches
					// ___ 60        above or below these are ignored.
					// . . . (+27)
					// ___ 192
					if (inputdata.touch_y <= GUI_ROW1_Y || inputdata.touch_y > DS_SCREEN_HEIGHT)
						break;

					size_t row = (inputdata.touch_y - GUI_ROW1_Y) / GUI_ROW_SY;

					if (row >= last_entry - first_entry + 1)
						break;

					sel_entry = first_entry + row;
					/* fall through */
				}

				case CURSOR_SELECT:
					DS2_AwaitNoButtons();
					if (sel_entry == 0) {  /* the parent directory */
						char* slash = strrchr(cur_dir, '/');
						if (slash != NULL) {  /* there's a parent */
							*slash = '\0';
							continue_input = false;
						} else {  /* we're at the root */
							ret = -1;
							continue_dir = false;
						}
					} else if (entries[sel_entry].is_dir) {
						strcat(cur_dir, "/");
						strcat(cur_dir, entries[sel_entry].name);
						continue_input = false;
					} else {
						strcpy(dir, cur_dir);
						strcpy(result_name, entries[sel_entry].name);
						ret = 0;
						continue_dir = false;
					}
					break;

				case CURSOR_UP:
					if (sel_entry > 0)
						sel_entry--;
					break;

				case CURSOR_DOWN:
					sel_entry++;
					if (sel_entry >= count)
						sel_entry--;
					break;

				//scroll page down
				case CURSOR_RTRIGGER:
					sel_entry += FILE_LIST_ROWS;
					if (sel_entry >= count)
						sel_entry = count - 1;
					break;

				//scroll page up
				case CURSOR_LTRIGGER:
					if (sel_entry >= FILE_LIST_ROWS)
						sel_entry -= FILE_LIST_ROWS;
					else
						sel_entry = 0;
					break;

				case CURSOR_BACK:
				{
					DS2_AwaitNoButtons();
					char* slash = strrchr(cur_dir, '/');
					if (slash != NULL) {  /* there's a parent */
						*slash = '\0';
						continue_input = false;
					} else {  /* we're at the root */
						ret = -1;
						continue_dir = false;
					}
					break;
				}

				case CURSOR_EXIT:
					DS2_AwaitNoButtons();
					ret = -1;
					continue_dir = false;
					break;

				default:
					break;
			} // end switch
		} // end while

cleanupScrollers:
		for (i = 0; i < FILE_LIST_ROWS + 1; i++) {
			draw_hscroll_over(scrollers[i]);
			scrollers[i] = NULL;
		}

cleanup:
		if (cur_dir_handle != NULL)
			closedir(cur_dir_handle);

		if (entries != NULL) {
			for (; count > 0; count--)
				free(entries[count - 1].name);
			free(entries);
		}
	} // end while

	return ret;
}

/* --- THE MENU --- */

enum EntryKind {
	KIND_ACTION,
	KIND_OPTION,
	KIND_SUBMENU,
	KIND_DISPLAY,
	KIND_CUSTOM,
};

enum DataType {
	TYPE_STRING,
	TYPE_INT32,
	TYPE_UINT32,
	TYPE_INT64,
	TYPE_UINT64,
};

struct TouchBounds {
	uint32_t X1; /* The left bound. */
	uint32_t Y1; /* The top bound. */
	uint32_t X2; /* The right bound, exclusive. */
	uint32_t Y2; /* The bottom bound, exclusive. */
};

struct Entry;

struct Menu;

/*
 * ModifyFunction is the type of a function that acts on an input in the
 * menu. The function is assigned this input via the Entry struct's
 * various button functions, Init, End, etc.
 * Input:
 *   1: On entry into the function, points to a memory location containing
 *     a pointer to the active menu. On exit from the function, the menu may
 *     be modified to a new one, in which case the function has chosen to
 *     activate that new menu; the End of the old menu is called,
 *     then the Init function of the new menu is called.
 *
 *     The exception to this rule is the NULL menu. If NULL is chosen to be
 *     activated, then no Init function is called; additionally, the menu is
 *     exited.
 *   2: On entry into the function, points to a memory location containing the
 *     index among the active menu's Entries array corresponding to the active
 *     menu entry. On exit from the function, the menu entry index may be
 *     modified to a new one, in which case the function has chosen to
 *     activate that new menu entry.
 */
typedef void (*ModifyFunction) (struct Menu**, uint32_t*);

/*
 * EntryDisplayFunction is the type of a function that displays an element
 * (the name or the value, depending on which member receives a function of
 * this type) of a single menu entry.
 * Input:
 *   1: A pointer to the data for the menu entry whose part is being drawn.
 *   2: A pointer to the data for the active menu entry.
 *   3: The position, expressed as a line number starting at 0, of the entry
 *     part to be drawn.
 */
typedef void (*EntryDisplayFunction) (struct Entry*, struct Entry*, uint32_t);

/*
 * EntryCanFocusFunction is the type of a function that determines whether
 * a menu entry can receive the focus.
 * Input:
 *   1: The menu containing the entry that is being tested.
 *   2: The menu entry that is being tested.
 *   3: The index of the entry within its containing menu.
 */
typedef bool (*EntryCanFocusFunction) (struct Menu*, struct Entry*, uint32_t);

/*
 * EntryFunction is the type of a function that displays all data related
 * to a menu or that modifies the Target variable of the active menu entry.
 * Input:
 *   1: The menu containing the entries that are being drawn, or the entry
 *     whose Target variable is being modified.
 *   2: The active menu entry.
 */
typedef void (*EntryFunction) (struct Menu*, struct Entry*);

/*
 * EntryTouchBoundsFunction is the type of a function that determines the
 * bounds of a menu entry.
 * Input:
 *   1: A pointer to the data for the menu containing the entry whose touch
 *     bounds are being queried.
 *   2: A pointer to the data for the menu entry whose touch bounds are being
 *     queried.
 *   3: The position, expressed as a line number starting at 0, of the entry.
 * Output:
 *   4: The bounds of the entry's touch rectangle.
 */
typedef void (*EntryTouchBoundsFunction) (struct Menu*, struct Entry*, uint32_t, struct TouchBounds*);

/*
 * EntryTouchFunction is the type of a function that acts on touches in the
 * menu.
 * Input:
 *   1: On entry into the function, points to a memory location containing
 *     a pointer to the active menu. On exit from the function, the menu may
 *     be modified to a new one, in which case the function has chosen to
 *     activate that new menu; the End of the old menu is called,
 *     then the Init function of the new menu is called.
 *
 *     The exception to this rule is the NULL menu. If NULL is chosen to be
 *     activated, then no Init function is called; additionally, the menu is
 *     exited.
 *   2: On entry into the function, points to a memory location containing the
 *     index among the active menu's Entries array that has been touched. It
 *     is updated by the input dispatch function. On exit from the function,
 *     the menu entry index may be modified to a new one, in which case the
 *     function has chosen to activate that new menu entry.
 *   3: The X coordinate of the touch.
 *   4: The Y coordinate of the touch.
 */
typedef void (*EntryTouchFunction) (struct Menu**, uint32_t*, uint32_t X, uint32_t Y);

/*
 * InitFunction is the type of a function that runs when a menu is being
 * initialised.
 * Variables:
 *   1: A pointer to a variable holding the menu that is being initialised.
 *   On exit from the function, the menu may be modified to a new one, in
 *   which case the function has chosen to activate that new menu. This can
 *   be used when the initialisation has failed for some reason.
 */
typedef void (*InitFunction) (struct Menu**);

/*
 * MenuFunction is the type of a function that runs when a menu is being
 * finalised or drawn.
 * Input:
 *   1: The menu that is being finalised or drawn.
 */
typedef void (*MenuFunction) (struct Menu*);

/*
 * ActionFunction is the type of a function that runs after changes to the
 * value of an option (if the entry kind is KIND_OPTION), or to respond to
 * pressing the action button (if the entry kind is KIND_ACTION).
 */
typedef void (*ActionFunction) (void);

struct Entry {
	enum EntryKind Kind;
	const char** Name;
	enum DataType DisplayType;
	void* Target;           // With KIND_OPTION, must point to uint32_t.
	                        // With KIND_DISPLAY, must point to the data type
	                        // specified by DisplayType.
	                        // With KIND_SUBMENU, this is struct Menu*.
	// Choices and ChoiceCount are only used with KIND_OPTION.
	ModifyFunction Enter;
	EntryFunction Left;
	EntryFunction Right;
	EntryTouchBoundsFunction TouchBounds;
	EntryTouchFunction Touch;
	EntryDisplayFunction DisplayName;
	EntryDisplayFunction DisplayValue;
	EntryCanFocusFunction CanFocus;
	ActionFunction Action;
	void* UserData;
	uint32_t ChoiceCount;
	const char** Choices[];
};

struct Menu {
	struct Menu* Parent;
	const char** Title;
	MenuFunction DisplayBackground;
	MenuFunction DisplayTitle;
	EntryFunction DisplayData;
	ModifyFunction InputDispatch;
	ModifyFunction Up;
	ModifyFunction Down;
	ModifyFunction Leave;
	InitFunction Init;
	MenuFunction End;
	void* UserData;
	uint32_t ActiveEntryIndex;
	struct Entry* Entries[]; // Entries are ended by a NULL pointer value.
};

static bool DefaultCanFocus(struct Menu* ActiveMenu, struct Entry* ActiveEntry, uint32_t Position)
{
	if (ActiveEntry->Kind == KIND_DISPLAY)
		return false;
	return true;
}

static uint32_t FindNullEntry(struct Menu* ActiveMenu)
{
	uint32_t Result = 0;
	while (ActiveMenu->Entries[Result] != NULL)
		Result++;
	return Result;
}

static bool MoveUp(struct Menu* ActiveMenu, uint32_t* ActiveEntryIndex)
{
	if (*ActiveEntryIndex == 0) {  // Going over the top?
		// Go back to the bottom.
		uint32_t NullEntry = FindNullEntry(ActiveMenu);
		if (NullEntry == 0)
			return false;
		*ActiveEntryIndex = NullEntry - 1;
		return true;
	}
	(*ActiveEntryIndex)--;
	return true;
}

static bool MoveDown(struct Menu* ActiveMenu, uint32_t* ActiveEntryIndex)
{
	if (*ActiveEntryIndex == 0 && ActiveMenu->Entries[*ActiveEntryIndex] == NULL)
		return false;
	if (ActiveMenu->Entries[*ActiveEntryIndex] == NULL)  // Is the sentinel "active"?
		*ActiveEntryIndex = 0;  // Go back to the top.
	(*ActiveEntryIndex)++;
	if (ActiveMenu->Entries[*ActiveEntryIndex] == NULL)  // Going below the bottom?
		*ActiveEntryIndex = 0;  // Go back to the top.
	return true;
}

static void DefaultUp(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	if (MoveUp(*ActiveMenu, ActiveEntryIndex)) {
		// Keep moving up until a menu entry can be focused.
		uint32_t Sentinel = *ActiveEntryIndex;
		EntryCanFocusFunction CanFocus = (*ActiveMenu)->Entries[*ActiveEntryIndex]->CanFocus;
		if (CanFocus == NULL) CanFocus = DefaultCanFocus;
		while (!CanFocus(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveEntryIndex], *ActiveEntryIndex)) {
			MoveUp(*ActiveMenu, ActiveEntryIndex);
			if (*ActiveEntryIndex == Sentinel) {
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocus = (*ActiveMenu)->Entries[*ActiveEntryIndex]->CanFocus;
			if (CanFocus == NULL) CanFocus = DefaultCanFocus;
		}
	}
}

static void DefaultDown(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	if (MoveDown(*ActiveMenu, ActiveEntryIndex)) {
		// Keep moving up until a menu entry can be focused.
		uint32_t Sentinel = *ActiveEntryIndex;
		EntryCanFocusFunction CanFocus = (*ActiveMenu)->Entries[*ActiveEntryIndex]->CanFocus;
		if (CanFocus == NULL) CanFocus = DefaultCanFocus;
		while (!CanFocus(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveEntryIndex], *ActiveEntryIndex)) {
			MoveDown(*ActiveMenu, ActiveEntryIndex);
			if (*ActiveEntryIndex == Sentinel) {
				// If we went through all of them, we cannot focus anything.
				// Place the focus on the NULL entry.
				*ActiveEntryIndex = FindNullEntry(*ActiveMenu);
				return;
			}
			CanFocus = (*ActiveMenu)->Entries[*ActiveEntryIndex]->CanFocus;
			if (CanFocus == NULL) CanFocus = DefaultCanFocus;
		}
	}
}

static void DefaultRight(struct Menu* ActiveMenu, struct Entry* ActiveEntry)
{
	if (ActiveEntry->Kind == KIND_OPTION
	|| (ActiveEntry->Kind == KIND_CUSTOM && ActiveEntry->Right == &DefaultRight /* chose to use this function */
	 && ActiveEntry->Target != NULL)) {
		uint32_t* Target = (uint32_t*) ActiveEntry->Target;
		(*Target)++;
		if (*Target >= ActiveEntry->ChoiceCount)
			*Target = 0;

		ActionFunction Action = ActiveEntry->Action;
		if (Action != NULL)
			Action();
	}
}

static void DefaultLeft(struct Menu* ActiveMenu, struct Entry* ActiveEntry)
{
	if (ActiveEntry->Kind == KIND_OPTION
	|| (ActiveEntry->Kind == KIND_CUSTOM && ActiveEntry->Left == &DefaultLeft /* chose to use this function */
	 && ActiveEntry->Target != NULL)) {
		uint32_t* Target = (uint32_t*) ActiveEntry->Target;
		if (*Target == 0)
			*Target = ActiveEntry->ChoiceCount;
		(*Target)--;

		ActionFunction Action = ActiveEntry->Action;
		if (Action != NULL)
			Action();
	}
}

static void DefaultEnter(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	if ((*ActiveMenu)->Entries[*ActiveEntryIndex]->Kind == KIND_SUBMENU) {
		*ActiveMenu = (struct Menu*) (*ActiveMenu)->Entries[*ActiveEntryIndex]->Target;
	} else if ((*ActiveMenu)->Entries[*ActiveEntryIndex]->Kind == KIND_ACTION) {
		ActionFunction Action = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Action;
		if (Action != NULL) Action();
	}
}

static void DefaultLeave(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	*ActiveMenu = (*ActiveMenu)->Parent;
}

static void DefaultTouchBounds(struct Menu* ActiveMenu, struct Entry* ActiveEntry, uint32_t Position, struct TouchBounds* Bounds)
{
	*Bounds = (struct TouchBounds) {
		.X1 = 0, .Y1 = GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY,
		.X2 = 256, .Y2 = GUI_ROW1_Y + Position * GUI_ROW_SY
	};
}

static void DefaultTouch(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex, uint32_t X, uint32_t Y)
{
	if ((*ActiveMenu)->Entries[*ActiveEntryIndex]->Kind == KIND_SUBMENU) {
		ModifyFunction Enter = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Enter;
		if (Enter == NULL) Enter = DefaultEnter;
		Enter(ActiveMenu, ActiveEntryIndex);
	} else if ((*ActiveMenu)->Entries[*ActiveEntryIndex]->Kind == KIND_ACTION) {
		ActionFunction Action = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Action;
		if (Action != NULL)
			Action();
	} else if ((*ActiveMenu)->Entries[*ActiveEntryIndex]->Kind == KIND_OPTION) {
		EntryFunction Right = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Right;
		if (Right == NULL) Right = DefaultRight;
		Right(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveEntryIndex]);
	}
}

static void DefaultInputDispatch(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	gui_action_type gui_action = get_gui_input();
	
	switch (gui_action) {
	case CURSOR_SELECT:
		if ((*ActiveMenu)->Entries[*ActiveEntryIndex] != NULL) {
			ModifyFunction Enter = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Enter;
			if (Enter == NULL) Enter = DefaultEnter;
			Enter(ActiveMenu, ActiveEntryIndex);
			break;
		}
		// otherwise, no entry has the focus, so SELECT acts like BACK
		// (fall through)

	case CURSOR_BACK:
	{
		ModifyFunction Leave = (*ActiveMenu)->Leave;
		if (Leave == NULL) Leave = DefaultLeave;
		Leave(ActiveMenu, ActiveEntryIndex);
		break;
	}

	case CURSOR_UP:
	{
		ModifyFunction Up = (*ActiveMenu)->Up;
		if (Up == NULL) Up = DefaultUp;
		Up(ActiveMenu, ActiveEntryIndex);
		break;
	}

	case CURSOR_DOWN:
	{
		ModifyFunction Down = (*ActiveMenu)->Down;
		if (Down == NULL) Down = DefaultDown;
		Down(ActiveMenu, ActiveEntryIndex);
		break;
	}

	case CURSOR_LEFT:
		if ((*ActiveMenu)->Entries[*ActiveEntryIndex] != NULL) {
			EntryFunction Left = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Left;
			if (Left == NULL) Left = DefaultLeft;
			Left(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveEntryIndex]);
		}
		break;

	case CURSOR_RIGHT:
		if ((*ActiveMenu)->Entries[*ActiveEntryIndex] != NULL) {
			EntryFunction Right = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Right;
			if (Right == NULL) Right = DefaultRight;
			Right(*ActiveMenu, (*ActiveMenu)->Entries[*ActiveEntryIndex]);
		}
		break;

	case CURSOR_TOUCH:
	{
		struct DS_InputState inputdata;
		uint32_t EntryIndex = 0;
		struct Entry* Entry = (*ActiveMenu)->Entries[0];
		struct TouchBounds Bounds;

		DS2_GetInputState(&inputdata);

		for (; Entry != NULL; EntryIndex++, Entry = (*ActiveMenu)->Entries[EntryIndex]) {
			EntryCanFocusFunction CanFocus = (*ActiveMenu)->Entries[EntryIndex]->CanFocus;
			if (CanFocus == NULL) CanFocus = DefaultCanFocus;
			if (!CanFocus(*ActiveMenu, Entry, EntryIndex))
				continue;

			EntryTouchBoundsFunction TouchBounds = (*ActiveMenu)->Entries[EntryIndex]->TouchBounds;
			if (TouchBounds == NULL) TouchBounds = DefaultTouchBounds;
			TouchBounds(*ActiveMenu, Entry, EntryIndex, &Bounds);

			if (inputdata.touch_x >= Bounds.X1 && inputdata.touch_x < Bounds.X2
			 && inputdata.touch_y >= Bounds.Y1 && inputdata.touch_y < Bounds.Y2) {
				*ActiveEntryIndex = EntryIndex;

				EntryTouchFunction Touch = (*ActiveMenu)->Entries[EntryIndex]->Touch;
				if (Touch == NULL) Touch = DefaultTouch;
				Touch(ActiveMenu, ActiveEntryIndex, inputdata.touch_x, inputdata.touch_y);
				break;
			}
		}

		break;
	}

	default:
		break;
	}
}

static void DefaultDisplayName(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM;
	if (IsActive) {
		show_icon(DS2_GetSubScreen(), &ICON_SUBSELA, SUBSELA_X, GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY + SUBSELA_OFFSET_Y);
	}
	PRINT_STRING_BG(DS2_GetSubScreen(), *DrawnEntry->Name, TextColor, COLOR_TRANS, OPTION_TEXT_X, GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY + TEXT_OFFSET_Y);
}

static void DefaultDisplayValue(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	if (DrawnEntry->Kind == KIND_OPTION || DrawnEntry->Kind == KIND_DISPLAY) {
		char Temp[21];
		Temp[0] = '\0';
		const char* Value = Temp;
		bool Error = false;
		if (DrawnEntry->Kind == KIND_OPTION) {
			if (*(uint32_t*) DrawnEntry->Target < DrawnEntry->ChoiceCount)
				Value = *DrawnEntry->Choices[*(uint32_t*) DrawnEntry->Target];
			else {
				Value = "Out of bounds";
				Error = true;
			}
		} else if (DrawnEntry->Kind == KIND_DISPLAY) {
			switch (DrawnEntry->DisplayType) {
				case TYPE_STRING:
					Value = (const char*) DrawnEntry->Target;
					break;
				case TYPE_INT32:
					sprintf(Temp, "%" PRIi32, *(int32_t*) DrawnEntry->Target);
					break;
				case TYPE_UINT32:
					sprintf(Temp, "%" PRIu32, *(uint32_t*) DrawnEntry->Target);
					break;
				case TYPE_INT64:
					sprintf(Temp, "%" PRIi64, *(int64_t*) DrawnEntry->Target);
					break;
				case TYPE_UINT64:
					sprintf(Temp, "%" PRIu64, *(uint64_t*) DrawnEntry->Target);
					break;
				default:
					Value = "Unknown type";
					Error = true;
					break;
			}
		}
		bool IsActive = (DrawnEntry == ActiveEntry);
		uint16_t TextColor = Error ? BGR555(0, 0, 31) : (IsActive ? COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM);
		uint32_t TextWidth = BDF_WidthUTF8s(Value);
		PRINT_STRING_BG(DS2_GetSubScreen(), Value, TextColor, COLOR_TRANS, DS_SCREEN_WIDTH - OPTION_TEXT_X - TextWidth, GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY + TEXT_OFFSET_Y);
	}
}

static void DefaultDisplayBackground(struct Menu* ActiveMenu)
{
	show_icon(DS2_GetSubScreen(), &ICON_SUBBG, 0, 0);
}

static void DefaultDisplayData(struct Menu* ActiveMenu, struct Entry* ActiveEntry)
{
	uint32_t DrawnEntryIndex = 0;
	struct Entry* DrawnEntry = ActiveMenu->Entries[0];
	for (; DrawnEntry != NULL; DrawnEntryIndex++, DrawnEntry = ActiveMenu->Entries[DrawnEntryIndex]) {
		EntryDisplayFunction Function = DrawnEntry->DisplayName;
		if (Function == NULL) Function = &DefaultDisplayName;
		Function(DrawnEntry, ActiveEntry, DrawnEntryIndex);

		Function = DrawnEntry->DisplayValue;
		if (Function == NULL) Function = &DefaultDisplayValue;
		Function(DrawnEntry, ActiveEntry, DrawnEntryIndex);
	}
}

static void DefaultDisplayTitle(struct Menu* ActiveMenu)
{
	show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
	show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);

	draw_string_vcenter(DS2_GetSubScreen(), 0, 9, 256, COLOR_ACTIVE_ITEM, *ActiveMenu->Title);
}

// -- Shorthand for creating menu entries --

#define ENTRY_OPTION(_Name, _Target, _ChoiceCount) \
	.Kind = KIND_OPTION, .Name = _Name, .Target = _Target, .ChoiceCount = _ChoiceCount

#define ENTRY_DISPLAY(_Name, _Target, _DisplayType) \
	.Kind = KIND_DISPLAY, .Name = _Name, .Target = _Target, .DisplayType = _DisplayType

#define ENTRY_SUBMENU(_Name, _Target) \
	.Kind = KIND_SUBMENU, .Name = _Name, .Target = _Target

// -- Custom actions --

static void DisplayBack(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	if (DrawnEntry == ActiveEntry) {
		show_icon(DS2_GetSubScreen(), &ICON_BACK, BACK_BUTTON_X, BACK_BUTTON_Y);
	} else {
		show_icon(DS2_GetSubScreen(), &ICON_NBACK, BACK_BUTTON_X, BACK_BUTTON_Y);
	}
}

/* This function can be used to redirect any Entry's Touch function to its
 * Enter function. */
static void TouchEnter(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex, uint32_t X, uint32_t Y)
{
	ModifyFunction Enter = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Enter;
	if (Enter == NULL) Enter = DefaultEnter;
	Enter(ActiveMenu, ActiveEntryIndex);
}

static void TouchBoundsBack(struct Menu* ActiveMenu, struct Entry* ActiveEntry, uint32_t Position, struct TouchBounds* Bounds)
{
	*Bounds = (struct TouchBounds) {
		.X1 = BACK_BUTTON_TOUCH_X, .Y1 = 0,
		.X2 = DS_SCREEN_WIDTH, .Y2 = BACK_BUTTON_TOUCH_Y
	};
}

static struct Entry Back = {
	.Kind = KIND_CUSTOM,
	.DisplayName = DisplayBack,
	.Enter = DefaultLeave, .Touch = TouchEnter,
	.TouchBounds = TouchBoundsBack
};

/* --- MAIN MENU --- */

extern struct Menu MainMenu;

extern struct Menu AudioVideo;
extern struct Menu SavedStates;
extern struct Menu Tools;
extern struct Menu Options;
extern struct Menu NewGame;

static void DisplayBackgroundMainMenu(struct Menu* ActiveMenu)
{
	show_icon(DS2_GetSubScreen(), &ICON_MAINBG, 0, 0);
}

static void DisplayTitleMainMenu(struct Menu* ActiveMenu)
{
}

static void InputDispatchMainMenu(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	gui_action_type gui_action = get_gui_input();
	
	switch (gui_action) {
	case CURSOR_SELECT:
		if ((*ActiveMenu)->Entries[*ActiveEntryIndex] != NULL) {
			ModifyFunction Enter = (*ActiveMenu)->Entries[*ActiveEntryIndex]->Enter;
			if (Enter == NULL) Enter = DefaultEnter;
			Enter(ActiveMenu, ActiveEntryIndex);
			break;
		}
		// otherwise, no entry has the focus, so SELECT acts like BACK
		// (fall through)

	case CURSOR_BACK:
	{
		ModifyFunction Leave = (*ActiveMenu)->Leave;
		if (Leave == NULL) Leave = DefaultLeave;
		Leave(ActiveMenu, ActiveEntryIndex);
		break;
	}

	case CURSOR_UP:
	case CURSOR_DOWN:
		*ActiveEntryIndex ^= 2;
		break;

	case CURSOR_LEFT:
	case CURSOR_RIGHT:
		*ActiveEntryIndex ^= 1;
		break;

	case CURSOR_TOUCH:
	{
		struct DS_InputState inputdata;
		uint32_t EntryIndex;
		struct Entry* Entry;

		DS2_GetInputState(&inputdata);

		EntryIndex = inputdata.touch_x / 128 + (inputdata.touch_y / 96) * 2;

		Entry = (*ActiveMenu)->Entries[EntryIndex];

		EntryCanFocusFunction CanFocus = Entry->CanFocus;
		if (CanFocus == NULL) CanFocus = DefaultCanFocus;
		if (!CanFocus(*ActiveMenu, Entry, EntryIndex))
			break;

		*ActiveEntryIndex = EntryIndex;

		EntryTouchFunction Touch = Entry->Touch;
		if (Touch == NULL) Touch = DefaultTouch;
		Touch(ActiveMenu, ActiveEntryIndex, inputdata.touch_x, inputdata.touch_y);

		break;
	}

	default:
		break;
	}
}

static void DisplayCompress(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_MAIN : COLOR_INACTIVE_MAIN;
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_COMPRESS : &ICON_NCOMPRESS, 0, 0);
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_MSEL : &ICON_MNSEL, 24, 81);
	draw_string_vcenter(DS2_GetSubScreen(), 26, 81, 75, TextColor, *DrawnEntry->Name);
}

static void DisplayDecompress(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_MAIN : COLOR_INACTIVE_MAIN;
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_DECOMPRESS : &ICON_NDECOMPRESS, 128, 0);
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_MSEL : &ICON_MNSEL, 152, 81);
	draw_string_vcenter(DS2_GetSubScreen(), 154, 81, 75, TextColor, *DrawnEntry->Name);
}

static void DisplayOptions(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_MAIN : COLOR_INACTIVE_MAIN;
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_OPTIONS : &ICON_NOPTIONS, 0, 96);
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_MSEL : &ICON_MNSEL, 24, 177);
	draw_string_vcenter(DS2_GetSubScreen(), 26, 177, 75, TextColor, *DrawnEntry->Name);
}

static void ActionExit(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	// Please ensure that the Main Menu itself does not have entries of type
	// KIND_OPTION. The on-demand writing of settings to storage will not
	// occur after quit(), since it acts after the action function returns.
	DS2_HighClockSpeed();
	quit();
	*ActiveMenu = NULL;
}

static void DisplayExit(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = IsActive ? COLOR_ACTIVE_MAIN : COLOR_INACTIVE_MAIN;
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_EXIT : &ICON_NEXIT, 128, 96);
	show_icon(DS2_GetSubScreen(), IsActive ? &ICON_MSEL : &ICON_MNSEL, 152, 177);
	draw_string_vcenter(DS2_GetSubScreen(), 154, 177, 75, TextColor, *DrawnEntry->Name);
}

void ActionCompress(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	const char *file_ext[] = { NULL }; // Show all files
	char line_buffer[PATH_MAX], tmp_filename[PATH_MAX];

	if (load_file(file_ext, tmp_filename, g_default_rom_dir) != -1) {
		strcpy(line_buffer, g_default_rom_dir);
		strcat(line_buffer, "/");
		strcat(line_buffer, tmp_filename);

		DS2_FillScreen(DS_ENGINE_SUB, COLOR_BLACK);
		DS2_UpdateScreen(DS_ENGINE_SUB);

		DS2_SetScreenBacklights(DS_SCREEN_UPPER);

		DS2_HighClockSpeed();
		uint32_t level = application_config.CompressionLevel;
		if (level == 0)
			level = 1;
		else if (level > 9)
			level = 9;
		while (!GzipCompress(line_buffer, level)); // retry if needed

		DS2_LowClockSpeed();
		*ActiveMenu = NULL;
	}
}

void ActionDecompress(struct Menu** ActiveMenu, uint32_t* ActiveEntryIndex)
{
	const char *file_ext[] = { ".gz", ".zip", NULL };
	char line_buffer[PATH_MAX], tmp_filename[PATH_MAX];

	if (load_file(file_ext, tmp_filename, g_default_rom_dir) != -1) {
		strcpy(line_buffer, g_default_rom_dir);
		strcat(line_buffer, "/");
		strcat(line_buffer, tmp_filename);

		DS2_FillScreen(DS_ENGINE_SUB, COLOR_BLACK);
		DS2_UpdateScreen(DS_ENGINE_SUB);

		DS2_SetScreenBacklights(DS_SCREEN_UPPER);

		DS2_HighClockSpeed();
		if (strcasecmp(&line_buffer[strlen(line_buffer) - 3 /* .gz */], ".gz") == 0)
			while (!GzipUncompress(line_buffer)); // retry if needed
		else if (strcasecmp(&line_buffer[strlen(line_buffer) - 4 /* .zip */], ".zip") == 0)
			while (!ZipUncompress(line_buffer)); // retry if needed

		DS2_LowClockSpeed();
		*ActiveMenu = NULL;
	}
}

static struct Entry MainMenu_Compress = {
	.Kind = KIND_CUSTOM, .Name = &msg[MSG_MAIN_MENU_COMPRESS],
	.DisplayName = DisplayCompress,
	.Enter = ActionCompress, .Touch = TouchEnter
};

static struct Entry MainMenu_Decompress = {
	.Kind = KIND_CUSTOM, .Name = &msg[MSG_MAIN_MENU_DECOMPRESS],
	.DisplayName = DisplayDecompress,
	.Enter = ActionDecompress, .Touch = TouchEnter
};

static struct Entry MainMenu_Options = {
	ENTRY_SUBMENU(&msg[MSG_MAIN_MENU_OPTIONS], &Options),
	.DisplayName = DisplayOptions
};

static struct Entry MainMenu_Exit = {
	.Kind = KIND_CUSTOM, .Name = &msg[MSG_MAIN_MENU_EXIT],
	.Enter = ActionExit, .Touch = TouchEnter,
	.DisplayName = DisplayExit
};

struct Menu MainMenu = {
	.Parent = NULL, .Title = NULL,
	.DisplayBackground = DisplayBackgroundMainMenu, .DisplayTitle = DisplayTitleMainMenu,
	.InputDispatch = InputDispatchMainMenu,
	.Leave = ActionExit,
	.Entries = { &MainMenu_Compress, &MainMenu_Decompress, &MainMenu_Options, &MainMenu_Exit, NULL },
	.ActiveEntryIndex = 0  /* Start out at Compress */
};

/* --- Main Menu > OPTIONS --- */

static void DisplayLanguageValue(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	char Temp[21];
	Temp[0] = '\0';
	const char* Value = Temp;
	bool Error = false;

	if (*(uint32_t*) DrawnEntry->Target < DrawnEntry->ChoiceCount)
		Value = lang[*(uint32_t*) DrawnEntry->Target];
	else {
		Value = "Out of bounds";
		Error = true;
	}

	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = Error ? BGR555(0, 0, 31) : (IsActive ? COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM);
	uint32_t TextWidth = BDF_WidthUTF8s(Value);
	PRINT_STRING_BG(DS2_GetSubScreen(), Value, TextColor, COLOR_TRANS, DS_SCREEN_WIDTH - OPTION_TEXT_X - TextWidth, GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY + TEXT_OFFSET_Y);
}

static void DisplayCompressionLevelValue(struct Entry* DrawnEntry, struct Entry* ActiveEntry, uint32_t Position)
{
	char Temp[21];
	Temp[0] = '\0';
	const char* Value = Temp;
	bool Error = false;

	if (*(uint32_t*) DrawnEntry->Target < DrawnEntry->ChoiceCount)
		sprintf(Temp, "%" PRIu32, *(uint32_t*) DrawnEntry->Target);
	else {
		Value = "Out of bounds";
		Error = true;
	}

	bool IsActive = (DrawnEntry == ActiveEntry);
	uint16_t TextColor = Error ? BGR555(0, 0, 31) : (IsActive ? COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM);
	uint32_t TextWidth = BDF_WidthUTF8s(Value);
	PRINT_STRING_BG(DS2_GetSubScreen(), Value, TextColor, COLOR_TRANS, DS_SCREEN_WIDTH - OPTION_TEXT_X - TextWidth, GUI_ROW1_Y + (Position - 1) * GUI_ROW_SY + TEXT_OFFSET_Y);
}

void PostChangeLanguage()
{
	DS2_HighClockSpeed(); // crank it up

	load_language_msg(LANGUAGE_PACK, application_config.language);

	DS2_LowClockSpeed(); // and back down
}

void LoadDefaults()
{
	draw_message_box(DS2_GetSubScreen());
	draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_RESET]);

	if (draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_GENERAL_CONFIRM_WITH_A], msg[MSG_GENERAL_CANCEL_WITH_B])) {
		char file[PATH_MAX];

		DS2_AwaitNoButtons();
		draw_message_box(DS2_GetSubScreen());
		draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_PROGRESS_RESETTING]);
		DS2_UpdateScreen(DS_ENGINE_SUB);

		sprintf(file, "%s/%s", main_path, APPLICATION_CONFIG_FILENAME);
		remove(file);

		init_application_config();
		PostChangeLanguage();
	} else {
		DS2_AwaitNoButtons();
	}
}

void ShowVersion()
{
	char line_buffer[512];

	draw_message_box(DS2_GetSubScreen());
	sprintf(line_buffer, "%s\n%s %s", msg[MSG_APPLICATION_NAME], msg[MSG_WORD_APPLICATION_VERSION], DS2COMP_VERSION);
	draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, line_buffer);
	DS2_UpdateScreen(DS_ENGINE_SUB);

	DS2_AwaitNoButtons(); // invoked from the menu
	DS2_AwaitAnyButtons(); // wait until the user presses something
	DS2_AwaitNoButtons(); // don't give that button to the menu
}

static struct Entry Options_Language = {
	ENTRY_OPTION(&msg[FMT_OPTIONS_LANGUAGE], &application_config.language, LANG_END),
	.DisplayValue = DisplayLanguageValue,
	.Action = PostChangeLanguage
};

static struct Entry Options_CompressionLevel = {
	ENTRY_OPTION(&msg[FMT_OPTIONS_COMPRESSION_LEVEL], &application_config.CompressionLevel, 10),
	.DisplayValue = DisplayCompressionLevelValue
};

static struct Entry Options_Reset = {
	.Kind = KIND_CUSTOM, .Name = &msg[MSG_OPTIONS_RESET],
	.Enter = LoadDefaults, .Touch = TouchEnter
};

static struct Entry Options_Version = {
	.Kind = KIND_CUSTOM, .Name = &msg[MSG_OPTIONS_VERSION],
	.Enter = ShowVersion, .Touch = TouchEnter
};

struct Menu Options = {
	.Parent = &MainMenu, .Title = &msg[MSG_MAIN_MENU_OPTIONS],
	.Entries = { &Back, &Options_Language, &Options_CompressionLevel, &Options_Reset, &Options_Version, NULL },
	.ActiveEntryIndex = 1  /* Start out after Back */
};

void PreserveConfigs(APPLICATION_CONFIG* Config)
{
	memcpy(Config, &application_config, sizeof(APPLICATION_CONFIG));
}

void SaveConfigsIfNeeded(APPLICATION_CONFIG* Config)
{
	if (memcmp(Config, &application_config, sizeof(APPLICATION_CONFIG)) != 0) {
		save_application_config_file();
		memcpy(Config, &application_config, sizeof(APPLICATION_CONFIG));
	}
}

uint32_t menu(void)
{
	// Compared with current settings to determine if they need to be saved.
	APPLICATION_CONFIG PreviousConfig;
	struct Menu *ActiveMenu = &MainMenu, *PreviousMenu = ActiveMenu;

	DS2_FillScreen(DS_ENGINE_MAIN, COLOR_BLACK);
	DS2_FlipMainScreen();
	DS2_SetScreenBacklights(DS_SCREEN_LOWER);

	DS2_LowClockSpeed();
	// Allow the lid closing to go through so the user can close the lid and
	// make it sleep after (de)compressing
	DS2_AwaitNoButtonsIn(~DS_BUTTON_LID);

	PreserveConfigs(&PreviousConfig);

	if (MainMenu.Init != NULL) {
		MainMenu.Init(&ActiveMenu);
		while (PreviousMenu != ActiveMenu) {
			if (PreviousMenu != NULL && PreviousMenu->End != NULL)
				PreviousMenu->End(PreviousMenu);
			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->Init != NULL)
				ActiveMenu->Init(&ActiveMenu);
		}
	}

	while (ActiveMenu != NULL) {
		// Draw.
		MenuFunction DisplayBackground = ActiveMenu->DisplayBackground;
		if (DisplayBackground == NULL) DisplayBackground = DefaultDisplayBackground;
		DisplayBackground(ActiveMenu);

		MenuFunction DisplayTitle = ActiveMenu->DisplayTitle;
		if (DisplayTitle == NULL) DisplayTitle = DefaultDisplayTitle;
		DisplayTitle(ActiveMenu);

		EntryFunction DisplayData = ActiveMenu->DisplayData;
		if (DisplayData == NULL) DisplayData = DefaultDisplayData;
		DisplayData(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]);

		DS2_UpdateScreen(DS_ENGINE_SUB);
		DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

		// Get input.
		ModifyFunction InputDispatch = ActiveMenu->InputDispatch;
		if (InputDispatch == NULL) InputDispatch = DefaultInputDispatch;
		InputDispatch(&ActiveMenu, &ActiveMenu->ActiveEntryIndex);

		// Possibly finalise this menu and activate and initialise a new one.
		while (ActiveMenu != PreviousMenu) {
			if (PreviousMenu != NULL && PreviousMenu->End != NULL)
				PreviousMenu->End(PreviousMenu);

			SaveConfigsIfNeeded(&PreviousConfig);

			// Keep moving down until a menu entry can be focused, if
			// the first one can't be.
			if (ActiveMenu != NULL && ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex] != NULL) {
				uint32_t Sentinel = ActiveMenu->ActiveEntryIndex;
				EntryCanFocusFunction CanFocus = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocus;
				if (CanFocus == NULL) CanFocus = DefaultCanFocus;
				while (!CanFocus(ActiveMenu, ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex], ActiveMenu->ActiveEntryIndex)) {
					MoveDown(ActiveMenu, &ActiveMenu->ActiveEntryIndex);
					if (ActiveMenu->ActiveEntryIndex == Sentinel) {
						// If we went through all of them, we cannot focus anything.
						// Place the focus on the NULL entry.
						ActiveMenu->ActiveEntryIndex = FindNullEntry(ActiveMenu);
						break;
					}
					CanFocus = ActiveMenu->Entries[ActiveMenu->ActiveEntryIndex]->CanFocus;
					if (CanFocus == NULL) CanFocus = DefaultCanFocus;
				}
			}

			PreviousMenu = ActiveMenu;
			if (ActiveMenu != NULL && ActiveMenu->Init != NULL)
				ActiveMenu->Init(&ActiveMenu);
		}
	}

exit:
	SaveConfigsIfNeeded(&PreviousConfig);

	// Avoid leaving the menu with buttons pressed.
	DS2_AwaitNoButtons();

	// Clear the Sub Screen as its backlight is about to be turned off.
	DS2_FillScreen(DS_ENGINE_SUB, COLOR_BLACK);
	DS2_UpdateScreen(DS_ENGINE_SUB);

	DS2_SetScreenBacklights(DS_SCREEN_UPPER);

	DS2_HighClockSpeed();

	return 0;
}

/*--------------------------------------------------------
	Load language message
--------------------------------------------------------*/
int load_language_msg(const char *filename, uint32_t language)
{
	FILE *fp;
	char msg_path[PATH_MAX];
	char string[256];
	const char* start;
	const char* end;
	char *pt, *dst;
	uint32_t loop = 0, len;
	int ret;

	sprintf(msg_path, "%s/%s", main_path, filename);
	fp = fopen(msg_path, "rb");
	if (fp == NULL)
		return -1;

	switch (language) {
	case ENGLISH:
	default:
		start = "STARTENGLISH";
		end = "ENDENGLISH";
		break;
	case FRENCH:
		start = "STARTFRENCH";
		end = "ENDFRENCH";
		break;
	case SPANISH:
		start = "STARTSPANISH";
		end = "ENDSPANISH";
		break;
	case GERMAN:
		start = "STARTGERMAN";
		end = "ENDGERMAN";
		break;
	}
	size_t start_len = strlen(start), end_len = strlen(end);

	// find the start flag
	ret = 0;
	do {
		pt = fgets(string, sizeof(string), fp);
		if (pt == NULL) {
			ret = -2;
			goto load_language_msg_error;
		}
	} while (strncmp(pt, start, start_len) != 0);

	dst = msg_data;
	msg[0] = dst;

	while (loop != MSG_END) {
		while (1) {
			pt = fgets(string, sizeof(string), fp);
			if (pt != NULL) {
				if (pt[0] == '#' || pt[0] == '\r' || pt[0] == '\n')
					continue;
				else
					break;
			} else {
				ret = -3;
				goto load_language_msg_error;
			}
		}

		if (strncmp(pt, end, end_len) == 0)
			break;

		len = strlen(pt);

		// Replace key definitions (*letter) with Pictochat icons
		// while copying.
		size_t srcChar, dstLen = 0;
		for (srcChar = 0; srcChar < len; srcChar++)
		{
			if (pt[srcChar] == '*') {
				switch (pt[srcChar + 1]) {
				case 'A':
					memcpy(&dst[dstLen], HOTKEY_A_DISPLAY, sizeof(HOTKEY_A_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_A_DISPLAY) - 1;
					break;
				case 'B':
					memcpy(&dst[dstLen], HOTKEY_B_DISPLAY, sizeof(HOTKEY_B_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_B_DISPLAY) - 1;
					break;
				case 'X':
					memcpy(&dst[dstLen], HOTKEY_X_DISPLAY, sizeof(HOTKEY_X_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_X_DISPLAY) - 1;
					break;
				case 'Y':
					memcpy(&dst[dstLen], HOTKEY_Y_DISPLAY, sizeof(HOTKEY_Y_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_Y_DISPLAY) - 1;
					break;
				case 'L':
					memcpy(&dst[dstLen], HOTKEY_L_DISPLAY, sizeof(HOTKEY_L_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_L_DISPLAY) - 1;
					break;
				case 'R':
					memcpy(&dst[dstLen], HOTKEY_R_DISPLAY, sizeof(HOTKEY_R_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_R_DISPLAY) - 1;
					break;
				case 'S':
					memcpy(&dst[dstLen], HOTKEY_START_DISPLAY, sizeof(HOTKEY_START_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_START_DISPLAY) - 1;
					break;
				case 's':
					memcpy(&dst[dstLen], HOTKEY_SELECT_DISPLAY, sizeof(HOTKEY_SELECT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_SELECT_DISPLAY) - 1;
					break;
				case 'u':
					memcpy(&dst[dstLen], HOTKEY_UP_DISPLAY, sizeof(HOTKEY_UP_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_UP_DISPLAY) - 1;
					break;
				case 'd':
					memcpy(&dst[dstLen], HOTKEY_DOWN_DISPLAY, sizeof(HOTKEY_DOWN_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_DOWN_DISPLAY) - 1;
					break;
				case 'l':
					memcpy(&dst[dstLen], HOTKEY_LEFT_DISPLAY, sizeof(HOTKEY_LEFT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_LEFT_DISPLAY) - 1;
					break;
				case 'r':
					memcpy(&dst[dstLen], HOTKEY_RIGHT_DISPLAY, sizeof(HOTKEY_RIGHT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof(HOTKEY_RIGHT_DISPLAY) - 1;
					break;
				case '\0':
					dst[dstLen] = pt[srcChar];
					dstLen++;
					break;
				default:
					memcpy(&dst[dstLen], &pt[srcChar], 2);
					srcChar++;
					dstLen += 2;
					break;
				}
			} else {
				dst[dstLen] = pt[srcChar];
				dstLen++;
			}
		}

		dst += dstLen;
		// at a line return, when "\n" paded, this message not end
		if (*(dst - 1) == 0x0A) {
			pt = strrchr(pt, '\\');
			if ((pt != NULL) && (*(pt + 1) == 'n')) {
				if (*(dst - 2) == 0x0D) {
					*(dst - 4) = '\n';
					dst -= 3;
				} else {
					*(dst - 3) = '\n';
					dst -= 2;
				}
			} else /* the message ends */ {
				if (*(dst - 2) == 0x0D)
					dst -= 1;
				*(dst - 1) = '\0';
				msg[++loop] = dst;
			}
		}
	}

load_language_msg_error:
	fclose(fp);
	return ret;
}

/*--------------------------------------------------------
--------------------------------------------------------*/


/*--------------------------------------------------------
	Load font library
--------------------------------------------------------*/
uint32_t load_font()
{
    return (uint32_t)BDF_font_init();
}

/*--------------------------------------------------------
	Initialise application configuration
--------------------------------------------------------*/
void init_application_config(void)
{
	memset(&application_config, 0, sizeof(application_config));
	application_config.language = 0;     // Default language: English
	// Default trade-off between compression and speed: Speed
	// The processor in the DSTwo is just not the same as a 3.0 GHz
	// quad core, and the gains gotten from using "computer gzip"'s
	// default of 6 over the full speed of 1 are minimal.
	// The user can set his or her preference in Options anyway. [Neb]
	application_config.CompressionLevel = 1;
}

/*--------------------------------------------------------
	Load application configuration file
--------------------------------------------------------*/
int load_application_config_file(void)
{
	char tmp_path[PATH_MAX];
	FILE* fp;
	char *pt;

	sprintf(tmp_path, "%s/%s", main_path, APPLICATION_CONFIG_FILENAME);

	fp = fopen(tmp_path, "r");
	if(NULL != fp)
	{
		// check the file header
		pt= tmp_path;
		fread(pt, 1, APPLICATION_CONFIG_HEADER_SIZE, fp);
		pt[APPLICATION_CONFIG_HEADER_SIZE]= 0;
		if(!strcmp(pt, APPLICATION_CONFIG_HEADER))
		{
			memset(&application_config, 0, sizeof(application_config));
			fread(&application_config, 1, sizeof(application_config), fp);
			fclose(fp);
			return 0;
		}
		else
		{
			fclose(fp);
		}
	}

	// No configuration or in the wrong format. Set defaults.
	init_application_config();
	return -1;
}

/*--------------------------------------------------------
	Save application configuration file
--------------------------------------------------------*/
int save_application_config_file()
{
    char tmp_path[PATH_MAX];
	FILE* fp;

    sprintf(tmp_path, "%s/%s", main_path, APPLICATION_CONFIG_FILENAME);
	fp = fopen(tmp_path, "w");
	if(NULL != fp)
    {
		fwrite(APPLICATION_CONFIG_HEADER, 1, APPLICATION_CONFIG_HEADER_SIZE, fp);
		fwrite(&application_config, 1, sizeof(application_config), fp);
		fclose(fp);
        return 0;
    }

    return -1;
}

void quit(void)
{
	exit(EXIT_SUCCESS);
}

uint32_t file_length(FILE* file)
{
  uint32_t pos, size;
  pos= ftell(file);
  fseek(file, 0, SEEK_END);
  size= ftell(file);
  fseek(file, pos, SEEK_SET);

  return size;
}

void gui_init(uint32_t lang_id)
{
	int flag;

	DS2_HighClockSpeed(); // Crank it up. When the menu starts, -> low.

	// Find the "DS2COMP" system directory
	DIR *current_dir;

	strcpy(main_path, "fat:/DS2COMP");

	/* Various existence checks. */
	current_dir = opendir(main_path);
	if (current_dir)
		closedir(current_dir);
	else {
		strcpy(main_path, "fat:/_SYSTEM/PLUGINS/DS2COMP");
		current_dir = opendir(main_path);
		if (current_dir)
			closedir(current_dir);
		else {
			fprintf(stderr, "/DS2COMP: Directory missing\nPress any key to return to\nthe menu\n\n/DS2COMP: Dossier manquant\nAppuyer sur une touche pour\nretourner au menu");
			goto gui_init_err;
		}
	}

	show_logo();
	DS2_UpdateScreen(DS_ENGINE_SUB);

	load_application_config_file();
	lang_id = application_config.language;

	flag = icon_init(lang_id);
	if (flag != 0) {
		fprintf(stderr, "Some icons are missing\nLoad them onto your card\nPress any key to return to\nthe menu\n\nDes icones sont manquantes\nChargez-les sur votre carte\nAppuyer sur une touche pour\nretourner au menu");
		goto gui_init_err;
	}

	flag = color_init();
	if (flag != 0) {
		fprintf(stderr, "SYSTEM/GUI/uicolors.txt\nis missing\nPress any key to return to\nthe menu\n\nSYSTEM/GUI/uicolors.txt\nest manquant\nAppuyer sur une touche pour\nretourner au menu");
		goto gui_init_err;
	}


	flag = load_font();
	if (flag != 0) {
		fprintf(stderr, "Font library initialisation\nerror (%d)\nPress any key to return to\nthe menu\n\nErreur d'initalisation de la\npolice de caracteres (%d)\nAppuyer sur une touche pour\nretourner au menu", flag, flag);
		goto gui_init_err;
	}

	flag = load_language_msg(LANGUAGE_PACK, lang_id);
	if (flag != 0) {
		fprintf(stderr, "Language pack initialisation\nerror (%d)\nPress any key to return to\nthe menu\n\nErreur d'initalisation du\npack de langue (%d)\nAppuyer sur une touche pour\nretourner au menu", flag, flag);
		goto gui_init_err;
	}

	strcpy(g_default_rom_dir, "fat:");

	return;

gui_init_err:
	DS2_AwaitAnyButtons();
	exit(EXIT_FAILURE);
}
