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

const char *language_options[] = { (char *) &lang[0], (char *) &lang[1], (char *) &lang[2], (char *) &lang[3] };

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

#define MAKE_MENU(name, init_function, passive_function, key_function, end_function, \
	focus_option, screen_focus)												  \
  MENU_TYPE name##_menu =                                                     \
  {                                                                           \
    init_function,                                                            \
    passive_function,                                                         \
	key_function,															  \
	end_function,															  \
    name##_options,                                                           \
    sizeof(name##_options) / sizeof(MENU_OPTION_TYPE),                        \
	focus_option,															  \
	screen_focus															  \
  }                                                                           \

#define INIT_MENU(name, init_functions, passive_functions, key, end, focus, screen)\
	{																		  \
    name##_menu.init_function = init_functions,								  \
    name##_menu.passive_function = passive_functions,						  \
	name##_menu.key_function = key,											  \
	name##_menu.end_function = end,											  \
    name##_menu.options = name##_options,									  \
    name##_menu.num_options = sizeof(name##_options) / sizeof(MENU_OPTION_TYPE),\
	name##_menu.focus_option = focus,										  \
	name##_menu.screen_focus = screen;										  \
	}																		  \

#define ACTION_OPTION(action_function, passive_function, display_string,      \
 help_string, line_number)                                                    \
{                                                                             \
  action_function,                                                            \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  0,                                                                          \
  help_string,                                                                \
  line_number,                                                                \
  ACTION_TYPE                                                               \
}                                                                             \

#define SUBMENU_OPTION(sub_menu, display_string, help_string, line_number)    \
{                                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  sub_menu,                                                                   \
  display_string,                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  sizeof(sub_menu) / sizeof(MENU_OPTION_TYPE),                                \
  help_string,                                                                \
  line_number,                                                                \
  SUBMENU_TYPE                                                              \
}                                                                             \

#define SELECTION_OPTION(passive_function, display_string, options,           \
 option_ptr, num_options, help_string, line_number, type)                     \
{                                                                             \
  NULL,                                                                       \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  options,                                                                    \
  option_ptr,                                                                 \
  num_options,                                                                \
  help_string,                                                                \
  line_number,                                                                \
  type                                                                        \
}                                                                             \

#define ACTION_SELECTION_OPTION(action_function, passive_function,            \
 display_string, options, option_ptr, num_options, help_string, line_number,  \
 type)                                                                        \
{                                                                             \
  action_function,                                                            \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  options,                                                                    \
  option_ptr,                                                                 \
  num_options,                                                                \
  help_string,                                                                \
  line_number,                                                                \
  type | ACTION_TYPE                                                        \
}                                                                             \

#define STRING_SELECTION_OPTION(action_function, passive_function,            \
    display_string, options, option_ptr, num_options, help_string, action, line_number)\
{                                                                             \
  action_function,                                                            \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  options,                                                                    \
  option_ptr,                                                                 \
  num_options,                                                                \
  help_string,                                                                \
  line_number,                                                                \
  STRING_SELECTION_TYPE | action                                              \
}

#define NUMERIC_SELECTION_OPTION(passive_function, display_string,            \
 option_ptr, num_options, help_string, line_number)                           \
  SELECTION_OPTION(passive_function, display_string, NULL, option_ptr,        \
   num_options, help_string, line_number, NUMBER_SELECTION_TYPE)              \

#define STRING_SELECTION_HIDEN_OPTION(action_function, passive_function,      \
 display_string, options, option_ptr, num_options, help_string, line_number)  \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string,  options, option_ptr, num_options, help_string,            \
   line_number, (STRING_SELECTION_TYPE | HIDEN_TYPE))                         \

#define NUMERIC_SELECTION_ACTION_OPTION(action_function, passive_function,    \
 display_string, option_ptr, num_options, help_string, line_number)           \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string,  NULL, option_ptr, num_options, help_string,               \
   line_number, NUMBER_SELECTION_TYPE)                                        \

#define NUMERIC_SELECTION_HIDE_OPTION(action_function, passive_function,      \
    display_string, option_ptr, num_options, help_string, line_number)        \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string, NULL, option_ptr, num_options, help_string,                \
   line_number, NUMBER_SELECTION_TYPE)                                        \


typedef enum
{
	NUMBER_SELECTION_TYPE = 0x01,
	STRING_SELECTION_TYPE = 0x02,
	SUBMENU_TYPE          = 0x04,
	ACTION_TYPE           = 0x08,
	HIDEN_TYPE            = 0x10,
	PASSIVE_TYPE          = 0x00,
} MENU_OPTION_TYPE_ENUM;

struct _MENU_OPTION_TYPE
{
	void (* action_function)();				//Active option to process input
	void (* passive_function)();			//Passive function to process this option
	struct _MENU_TYPE *sub_menu;			//Sub-menu of this option
	const char **display_string;			//Name and other things of this option
	void *options;							//output value of this option
	uint32_t *current_option;					//output values
	uint32_t num_options;						//Total output values
	char **help_string;						//Help string
	uint32_t line_number;						//Order id of this option in it menu
	MENU_OPTION_TYPE_ENUM option_type;		//Option types
};

struct _MENU_TYPE
{
	void (* init_function)();				//Function to initialize this menu
	void (* passive_function)();			//Function to draw this menu
	void (* key_function)();				//Function to process input
	void (* end_function)();				//End process of this menu
	struct _MENU_OPTION_TYPE *options;		//Options array
	uint32_t	num_options;						//Total options of this menu
	uint32_t	focus_option;						//Option which obtained focus
	uint32_t	screen_focus;						//screen positon of the focus option
};

typedef struct _MENU_OPTION_TYPE MENU_OPTION_TYPE;
typedef struct _MENU_TYPE MENU_TYPE;

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

/*--------------------------------------------------------
	Main Menu
--------------------------------------------------------*/
uint32_t menu()
{
    gui_action_type gui_action;
    uint32_t i;
    uint32_t repeat;
    uint32_t return_value = 0;
    char tmp_filename[NAME_MAX];
    char line_buffer[512];

    MENU_TYPE *current_menu = NULL;
    MENU_OPTION_TYPE *current_option = NULL;
    MENU_OPTION_TYPE *display_option = NULL;
    
    uint32_t current_option_num;
//    uint32_t parent_option_num;
    uint32_t string_select;

    uint16_t *bg_screenp;
    uint32_t bg_screenp_color;

	auto void menu_exit();
	auto void choose_menu();
	auto void main_menu_passive();
	auto void main_menu_key();
	auto void menu_load_for_compression();
	auto void menu_load_for_decompression();
	auto void others_menu_init();
	auto void others_menu_end();
	auto void check_application_version();
	auto void language_set();

//Local function definition

	void menu_exit()
	{
		DS2_HighClockSpeed(); // Crank it up, leave quickly
		quit();
	}

	void menu_load_for_compression()
	{
		const char *file_ext[] = { NULL }; // Show all files

		if(load_file(file_ext, tmp_filename, g_default_rom_dir) != -1)
		{
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

			return_value = 1;
			repeat = 0;
		}
		else
		{
			choose_menu(current_menu);
		}
	}

	void menu_load_for_decompression()
	{
		const char *file_ext[] = { ".gz", ".zip", NULL };

		if(load_file(file_ext, tmp_filename, g_default_rom_dir) != -1)
		{
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

			return_value = 1;
			repeat = 0;
		}
		else
		{
			choose_menu(current_menu);
		}
	}

    void load_default_setting()
    {
        if(bg_screenp != NULL)
        {
            bg_screenp_color = COLOR_BG;
            memcpy(bg_screenp, DS2_GetSubScreen(), 256*192*2);
        }
        else
            bg_screenp_color = COLOR_BG;

        draw_message_box(DS2_GetSubScreen());
        draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_RESET]);

        if(draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_GENERAL_CONFIRM_WITH_A], msg[MSG_GENERAL_CANCEL_WITH_B]))
        {
		DS2_AwaitNoButtons();
            draw_message_box(DS2_GetSubScreen());
            draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_PROGRESS_RESETTING]);
            DS2_UpdateScreen(DS_ENGINE_SUB);
            DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

            sprintf(line_buffer, "%s/%s", main_path, APPLICATION_CONFIG_FILENAME);
            remove(line_buffer);

            init_application_config();

		DS2_FillScreen(DS_ENGINE_MAIN, COLOR_BLACK);
		DS2_FlipMainScreen();
        }
    }

    void check_application_version()
    {
        if(bg_screenp != NULL)
        {
            bg_screenp_color = COLOR_BG;
            memcpy(bg_screenp, DS2_GetSubScreen(), 256*192*2);
        }
        else
            bg_screenp_color = COLOR_BG;

        draw_message_box(DS2_GetSubScreen());
        sprintf(line_buffer, "%s\n%s %s", msg[MSG_APPLICATION_NAME], msg[MSG_WORD_APPLICATION_VERSION], DS2COMP_VERSION);
        draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, line_buffer);
        DS2_UpdateScreen(DS_ENGINE_SUB);
        DS2_AwaitScreenUpdate(DS_ENGINE_SUB);

		DS2_AwaitNoButtons(); // invoked from the menu
		DS2_AwaitAnyButtons(); // wait until the user presses something
		DS2_AwaitNoButtons(); // don't give that button to the menu
    }

    void language_set()
    {
        if(gui_action == CURSOR_LEFT || gui_action == CURSOR_RIGHT)
        {
            DS2_HighClockSpeed(); // crank it up
            if(bg_screenp != NULL)
            {
                bg_screenp_color = COLOR_BG;
                memcpy(bg_screenp, DS2_GetSubScreen(), 256*192*2);
            }
            else
                bg_screenp_color = COLOR_BG;

            load_language_msg(LANGUAGE_PACK, application_config.language);

            save_application_config_file();
            DS2_LowClockSpeed(); // and back down
        }
    }

    char *on_off_options[] = { (char*)&msg[MSG_GENERAL_OFF], (char*)&msg[MSG_GENERAL_ON] };

  /*--------------------------------------------------------
     Others
  --------------------------------------------------------*/
    uint32_t desert= 0;
	MENU_OPTION_TYPE others_options[] = 
	{
	/* 00 */ SUBMENU_OPTION(NULL, &msg[MSG_MAIN_MENU_OPTIONS], NULL, 0),

	/* 01 */ NUMERIC_SELECTION_OPTION(NULL, &msg[FMT_OPTIONS_COMPRESSION_LEVEL], &application_config.CompressionLevel, 10, NULL, 1),

	/* 02 */ STRING_SELECTION_OPTION(language_set, NULL, &msg[FMT_OPTIONS_LANGUAGE], language_options, 
        &application_config.language, sizeof(language_options) / sizeof(language_options[0]) /* number of possible languages */, NULL, ACTION_TYPE, 2),

	/* 03 */ ACTION_OPTION(load_default_setting, NULL, &msg[MSG_OPTIONS_RESET], NULL, 3),

	/* 04 */ ACTION_OPTION(check_application_version, NULL, &msg[MSG_OPTIONS_VERSION], NULL, 4),
	};

	MAKE_MENU(others, others_menu_init, NULL, NULL, others_menu_end, 1, 1);

  /*--------------------------------------------------------
     MAIN MENU
  --------------------------------------------------------*/
	MENU_OPTION_TYPE main_options[] =
	{

    /* 00 */ ACTION_OPTION(menu_load_for_compression, NULL, &msg[MSG_MAIN_MENU_COMPRESS], NULL, 0),

    /* 01 */ ACTION_OPTION(menu_load_for_decompression, NULL, &msg[MSG_MAIN_MENU_DECOMPRESS], NULL, 1),

    /* 02 */ SUBMENU_OPTION(&others_menu, &msg[MSG_MAIN_MENU_OPTIONS], NULL, 2),

    /* 03 */ ACTION_OPTION(menu_exit, NULL, &msg[MSG_MAIN_MENU_EXIT], NULL, 3),
	};

	MAKE_MENU(main, NULL, main_menu_passive, main_menu_key, NULL, 0 /* active item upon initialisation */, 0);

	void main_menu_passive()
	{
		show_icon(DS2_GetSubScreen(), &ICON_MAINBG, 0, 0);
		current_menu -> focus_option = current_option -> line_number;

		// Compress
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(DS2_GetSubScreen(), &ICON_COMPRESS, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_MSEL, 24, 81);
		}
		else {
			show_icon(DS2_GetSubScreen(), &ICON_NCOMPRESS, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_MNSEL, 24, 81);
		}
		draw_string_vcenter(DS2_GetSubScreen(), 26, 81, 75, COLOR_WHITE, line_buffer);

		// Decompress
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(DS2_GetSubScreen(), &ICON_DECOMPRESS, 128, 0);
			show_icon(DS2_GetSubScreen(), &ICON_MSEL, 152, 81);
		}
		else {
			show_icon(DS2_GetSubScreen(), &ICON_NDECOMPRESS, 128, 0);
			show_icon(DS2_GetSubScreen(), &ICON_MNSEL, 152, 81);
		}
		draw_string_vcenter(DS2_GetSubScreen(), 154, 81, 75, COLOR_WHITE, line_buffer);

		// Options
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(DS2_GetSubScreen(), &ICON_OPTIONS, 0, 96);
			show_icon(DS2_GetSubScreen(), &ICON_MSEL, 24, 177);
		}
		else {
			show_icon(DS2_GetSubScreen(), &ICON_NOPTIONS, 0, 96);
			show_icon(DS2_GetSubScreen(), &ICON_MNSEL, 24, 177);
		}
		draw_string_vcenter(DS2_GetSubScreen(), 26, 177, 75, COLOR_WHITE, line_buffer);

		// Exit
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(DS2_GetSubScreen(), &ICON_EXIT, 128, 96);
			show_icon(DS2_GetSubScreen(), &ICON_MSEL, 152, 177);
		}
		else {
			show_icon(DS2_GetSubScreen(), &ICON_NEXIT, 128, 96);
			show_icon(DS2_GetSubScreen(), &ICON_MNSEL, 152, 177);
		}
		draw_string_vcenter(DS2_GetSubScreen(), 154, 177, 75, COLOR_WHITE, line_buffer);
	}

    void main_menu_key()
    {
        switch(gui_action)
        {
            case CURSOR_DOWN:
				if(current_option_num < 2)	current_option_num += 2;
				else current_option_num -= 2;

                current_option = current_menu->options + current_option_num;
              break;

            case CURSOR_UP:
				if(current_option_num < 2)	current_option_num += 2;
				else current_option_num -= 2;

                current_option = current_menu->options + current_option_num;
              break;

            case CURSOR_RIGHT:
				if(current_option_num == 1)	current_option_num -= 1;
				else if(current_option_num == 3)	current_option_num -= 1;
				else current_option_num += 1;

                current_option = main_menu.options + current_option_num;
              break;

            case CURSOR_LEFT:
				if(current_option_num == 0)	current_option_num += 1;
				else if(current_option_num == 2)	current_option_num += 1;
				else current_option_num -= 1;

                current_option = main_menu.options + current_option_num;
              break;

            default:
              break;
        }// end swith
    }

	void tools_menu_init()
	{
	}

    void others_menu_init()
    {
    }

	void others_menu_end()
	{
		save_application_config_file();
	}

	void choose_menu(MENU_TYPE *new_menu)
	{
		if(new_menu == NULL)
			new_menu = &main_menu;

		if(NULL != current_menu) {
			if(current_menu->end_function)
				current_menu->end_function();
			current_menu->focus_option = current_menu->screen_focus = current_option_num;
		}

		current_menu = new_menu;
		current_option_num= current_menu -> focus_option;
		current_option = new_menu->options + current_option_num;
		if(current_menu->init_function)
			current_menu->init_function();
	}

//----------------------------------------------------------------------------//
//	Menu Start
	DS2_FillScreen(DS_ENGINE_MAIN, COLOR_BLACK);
	DS2_FlipMainScreen();

	DS2_LowClockSpeed();
	DS2_SetScreenBacklights(DS_SCREEN_LOWER);
	
	DS2_AwaitNoButtonsIn(~DS_BUTTON_LID); // Allow the lid closing to go through
	// so the user can close the lid and make it sleep after compressing
	bg_screenp= (uint16_t*)malloc(256*192*2);

	repeat = 1;

	choose_menu(&main_menu);

//	Menu loop
	
	while(repeat)
	{
		display_option = current_menu->options;
		string_select= 0;

		if(current_menu -> passive_function)
			current_menu -> passive_function();
		else
		{
			uint32_t line_num, screen_focus, focus_option;

			//draw background
			show_icon(DS2_GetSubScreen(), &ICON_SUBBG, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_TITLE, 0, 0);
			show_icon(DS2_GetSubScreen(), &ICON_TITLEICON, TITLE_ICON_X, TITLE_ICON_Y);

			strcpy(line_buffer, *(display_option->display_string));
			draw_string_vcenter(DS2_GetSubScreen(), 0, 9, 256, COLOR_ACTIVE_ITEM, line_buffer);

			line_num = current_option_num;
			screen_focus = current_menu -> screen_focus;
			focus_option = current_menu -> focus_option;

			if(focus_option < line_num)	//focus next option
			{
				focus_option = line_num - focus_option;
				screen_focus += focus_option;
				if(screen_focus > SUBMENU_ROW_NUM)	//Reach max row numbers can display
					screen_focus = SUBMENU_ROW_NUM;

				current_menu -> screen_focus = screen_focus;
				focus_option = line_num;
			}
			else if(focus_option > line_num)	//focus last option
			{
				focus_option = focus_option - line_num;
				if(screen_focus > focus_option)
					screen_focus -= focus_option;
				else
					screen_focus = 0;

				if(screen_focus == 0 && line_num > 0)
					screen_focus = 1;

				current_menu -> screen_focus = screen_focus;
				focus_option = line_num;
			}
			current_menu -> focus_option = focus_option;
		
			i = focus_option - screen_focus;
			display_option += i +1;

			line_num = current_menu->num_options-1;
			if(line_num > SUBMENU_ROW_NUM)
				line_num = SUBMENU_ROW_NUM;

			if(focus_option == 0)
				show_icon(DS2_GetSubScreen(), &ICON_BACK, BACK_BUTTON_X, BACK_BUTTON_Y);
			else
				show_icon(DS2_GetSubScreen(), &ICON_NBACK, BACK_BUTTON_X, BACK_BUTTON_Y);

			for(i= 0; i < line_num; i++, display_option++)
    	    {
    	        unsigned short color;

				if(display_option == current_option)
					show_icon(DS2_GetSubScreen(), &ICON_SUBSELA, SUBSELA_X, GUI_ROW1_Y + i * GUI_ROW_SY + SUBSELA_OFFSET_Y);

				if(display_option->passive_function)
				{
					display_option->line_number = i;
					display_option->passive_function();
				}
				else if(display_option->option_type & NUMBER_SELECTION_TYPE)
				{
					sprintf(line_buffer, *(display_option->display_string),
						*(display_option->current_option));
				}
				else if(display_option->option_type & STRING_SELECTION_TYPE)
				{
					sprintf(line_buffer, *(display_option->display_string),
						*((uint32_t*)(((uint32_t *)display_option->options)[*(display_option->current_option)])));
				}
				else
				{
					strcpy(line_buffer, *(display_option->display_string));
				}

				if(display_option->passive_function == NULL)
				{
					if(display_option == current_option)
						color= COLOR_ACTIVE_ITEM;
					else
						color= COLOR_INACTIVE_ITEM;
	
					PRINT_STRING_BG(DS2_GetSubScreen(), line_buffer, color, COLOR_TRANS, OPTION_TEXT_X, GUI_ROW1_Y + i * GUI_ROW_SY + TEXT_OFFSET_Y);
				}
    	    }
    	}

		struct DS_InputState inputdata;
		gui_action = get_gui_input();

		switch(gui_action)
		{
			case CURSOR_TOUCH:
				DS2_GetInputState(&inputdata);
				/* Back button at the top of every menu but the main one */
				if(current_menu != &main_menu && inputdata.touch_x >= BACK_BUTTON_X && inputdata.touch_y < BACK_BUTTON_Y + ICON_BACK.y)
				{
					choose_menu(current_menu->options->sub_menu);
					break;
				}
				/* Main menu */
				if(current_menu == &main_menu)
				{
					// 0     128    256
					//  _____ _____  0
					// |0CMP_|1DEC_| 96
					// |2OPT_|3EXI_| 192

					current_option_num = (inputdata.touch_y / 96) * 2 + (inputdata.touch_x / 128);
					current_option = current_menu->options + current_option_num;
					
					if(current_option -> option_type & HIDEN_TYPE)
						break;
					else if(current_option->option_type & ACTION_TYPE)
						current_option->action_function();
					else if(current_option->option_type & SUBMENU_TYPE)
						choose_menu(current_option->sub_menu);
				}
				/* This is the majority case, covering all menus except file loading */
				else if(current_menu != (main_menu.options + 0)->sub_menu
				&& current_menu != (main_menu.options + 1)->sub_menu)
				{
					if (inputdata.touch_y <= GUI_ROW1_Y || inputdata.touch_y > GUI_ROW1_Y + GUI_ROW_SY * SUBMENU_ROW_NUM)
						break;
					// ___ 33        This screen has 6 possible rows. Touches
					// ___ 60        above or below these are ignored.
					// . . . (+27)   The row between 33 and 60 is [1], though!
					// ___ 192
					uint32_t next_option_num = (inputdata.touch_y - GUI_ROW1_Y) / GUI_ROW_SY + 1;
					struct _MENU_OPTION_TYPE *next_option = current_menu->options + next_option_num;

					if (next_option_num >= current_menu->num_options)
						break;

					if(!next_option)
						break;

					if(next_option -> option_type & HIDEN_TYPE)
						break;

					current_option_num = next_option_num;
					current_option = current_menu->options + current_option_num;

					if(current_menu->key_function)
					{
						gui_action = CURSOR_RIGHT;
						current_menu->key_function();
					}
					else if(current_option->option_type & (NUMBER_SELECTION_TYPE | STRING_SELECTION_TYPE))
					{
						gui_action = CURSOR_RIGHT;
						uint32_t current_option_val = *(current_option->current_option);

						if(current_option_val <  current_option->num_options -1)
							current_option_val++;
						else
							current_option_val= 0;
						*(current_option->current_option) = current_option_val;

						if(current_option->action_function)
							current_option->action_function();
					}
					else if(current_option->option_type & ACTION_TYPE)
						current_option->action_function();
					else if(current_option->option_type & SUBMENU_TYPE)
						choose_menu(current_option->sub_menu);
				}
				break;
			case CURSOR_DOWN:
				if(current_menu->key_function)
					current_menu->key_function();
				else
				{
					current_option_num = (current_option_num + 1) % current_menu->num_options;
					current_option = current_menu->options + current_option_num;

					while(current_option -> option_type & HIDEN_TYPE)
					{
						current_option_num = (current_option_num + 1) % current_menu->num_options;
						current_option = current_menu->options + current_option_num;
					}
				}
				break;

			case CURSOR_UP:
				if(current_menu->key_function)
					current_menu->key_function();
				else
				{
					if(current_option_num)
						current_option_num--;
					else
						current_option_num = current_menu->num_options - 1;
					current_option = current_menu->options + current_option_num;

					while(current_option -> option_type & HIDEN_TYPE)
					{
						if(current_option_num)
							current_option_num--;
						else
							current_option_num = current_menu->num_options - 1;
						current_option = current_menu->options + current_option_num;
					}
				}
				break;

			case CURSOR_RIGHT:
				if(current_menu->key_function)
					current_menu->key_function();
				else
				{
					if(current_option->option_type & (NUMBER_SELECTION_TYPE | STRING_SELECTION_TYPE))
					{
						uint32_t current_option_val = *(current_option->current_option);

						if(current_option_val <  current_option->num_options -1)
							current_option_val++;
						else
							current_option_val= 0;
						*(current_option->current_option) = current_option_val;

						if(current_option->action_function)
							current_option->action_function();
					}
				}
				break;

			case CURSOR_LEFT:
				if(current_menu->key_function)
					current_menu->key_function();
				else
				{
					if(current_option->option_type & (NUMBER_SELECTION_TYPE | STRING_SELECTION_TYPE))
					{
						uint32_t current_option_val = *(current_option->current_option);

						if(current_option_val)
							current_option_val--;
						else
							current_option_val = current_option->num_options - 1;
						*(current_option->current_option) = current_option_val;

						if(current_option->action_function)
							current_option->action_function();
					}
				}
				break;

			case CURSOR_EXIT:
				break;

			case CURSOR_SELECT:
				if(current_option->option_type & ACTION_TYPE)
					current_option->action_function();
				else if(current_option->option_type & SUBMENU_TYPE)
					choose_menu(current_option->sub_menu);
				break;

			case CURSOR_BACK: 
				if(current_menu != &main_menu)
					choose_menu(current_menu->options->sub_menu);
				break;

			default:
				break;
		} // end swith

		DS2_UpdateScreen(DS_ENGINE_SUB);
		DS2_AwaitScreenUpdate(DS_ENGINE_SUB);
	} // end while

	if (current_menu && current_menu->end_function)
		current_menu->end_function();

	if(bg_screenp != NULL) free((void*)bg_screenp);

	DS2_FillScreen(DS_ENGINE_SUB, COLOR_BLACK);
		DS2_UpdateScreen(DS_ENGINE_SUB);
	DS2_AwaitNoButtons();

	DS2_SetScreenBacklights(DS_SCREEN_UPPER);

	DS2_HighClockSpeed();

	return return_value;
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
