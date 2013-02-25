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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ds2_types.h"
#include "ds2_timer.h"
#include "ds2io.h"
#include "ds2_malloc.h"
#include "ds2_cpu.h"
#include "fs_api.h"
#include "key.h"
#include "gui.h"
#include "draw.h"
#include "message.h"
#include "bitmap.h"

#include "minigzip.h"

char	main_path[MAX_PATH];
char	gamepak_name[MAX_PATH];

// If adding a language, make sure you update the size of the array in
// message.h too.
char *lang[2] =
	{ 
		"English",					// 0
		"Français",					// 1
	};

char *language_options[] = { (char *) &lang[0], (char *) &lang[1] };

/******************************************************************************
*	Macro definition
 ******************************************************************************/
#define DS2COMP_VERSION "0.60"

#define LANGUAGE_PACK   "SYSTEM/language.msg"
#define APPLICATION_CONFIG_FILENAME "SYSTEM/ds2comp.cfg"

#define APPLICATION_CONFIG_HEADER  "D2CM1.0"
#define APPLICATION_CONFIG_HEADER_SIZE 7
APPLICATION_CONFIG application_config;

#define SUBMENU_ROW_NUM 6

// These are U+05C8 and subsequent codepoints encoded in UTF-8.
const char HOTKEY_A_DISPLAY[] = {0xD7, 0x88, 0x00};
const char HOTKEY_B_DISPLAY[] = {0xD7, 0x89, 0x00};
const char HOTKEY_X_DISPLAY[] = {0xD7, 0x8A, 0x00};
const char HOTKEY_Y_DISPLAY[] = {0xD7, 0x8B, 0x00};
const char HOTKEY_L_DISPLAY[] = {0xD7, 0x8C, 0x00};
const char HOTKEY_R_DISPLAY[] = {0xD7, 0x8D, 0x00};
const char HOTKEY_START_DISPLAY[] = {0xD7, 0x8E, 0x00};
const char HOTKEY_SELECT_DISPLAY[] = {0xD7, 0x8F, 0x00};
// These are U+2190 and subsequent codepoints encoded in UTF-8.
const char HOTKEY_LEFT_DISPLAY[] = {0xE2, 0x86, 0x90, 0x00};
const char HOTKEY_UP_DISPLAY[] = {0xE2, 0x86, 0x91, 0x00};
const char HOTKEY_RIGHT_DISPLAY[] = {0xE2, 0x86, 0x92, 0x00};
const char HOTKEY_DOWN_DISPLAY[] = {0xE2, 0x86, 0x93, 0x00};

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
	char **display_string;					//Name and other things of this option
	void *options;							//output value of this option
	u32 *current_option;					//output values
	u32 num_options;						//Total output values
	char **help_string;						//Help string
	u32 line_number;						//Order id of this option in it menu
	MENU_OPTION_TYPE_ENUM option_type;		//Option types
};

struct _MENU_TYPE
{
	void (* init_function)();				//Function to initialize this menu
	void (* passive_function)();			//Function to draw this menu
	void (* key_function)();				//Function to process input
	void (* end_function)();				//End process of this menu
	struct _MENU_OPTION_TYPE *options;		//Options array
	u32	num_options;						//Total options of this menu
	u32	focus_option;						//Option which obtained focus
	u32	screen_focus;						//screen positon of the focus option
};

typedef struct _MENU_OPTION_TYPE MENU_OPTION_TYPE;
typedef struct _MENU_TYPE MENU_TYPE;

/******************************************************************************
 ******************************************************************************/
char g_default_rom_dir[MAX_PATH];
/******************************************************************************
 ******************************************************************************/
static int sort_function(const void *dest_str_ptr, const void *src_str_ptr);
static void get_timestamp_string(char *buffer, u16 msg_id, u16 year, u16 mon, u16 day, u16 wday, u16 hour, u16 min, u16 sec, u32 msec);
static void get_time_string(char *buff, struct rtc *rtcp);
static void init_application_config(void);
static int load_application_config_file(void);
static int save_application_config_file(void);
static void quit(void);

/*--------------------------------------------------------
	Get GUI input
--------------------------------------------------------*/
gui_action_type get_gui_input(void)
{
	unsigned int key;
	gui_action_type	ret;

	key = getKey();

	if (key & KEY_LID)
	{
		ds2_setSupend();
		struct key_buf inputdata;
		do {
			ds2_getrawInput(&inputdata);
			mdelay(1);
		} while (inputdata.key & KEY_LID);
		ds2_wakeup();
		// In the menu, the upper screen's backlight can be off,
		// but it is on right away after resuming from suspend.
		mdelay(100); // needed to avoid ds2_setBacklight crashing
		ds2_setBacklight(1);
	}

	switch(key)
	{
		case KEY_UP:
			ret = CURSOR_UP;
			break;
		case KEY_DOWN:
			ret = CURSOR_DOWN;
			break;
		case KEY_LEFT:
			ret = CURSOR_LEFT;
			break;
		case KEY_RIGHT:
			ret = CURSOR_RIGHT;
			break;
		case KEY_L:
			ret = CURSOR_LTRIGGER;
			break;
		case KEY_R:
			ret = CURSOR_RTRIGGER;
			break;
		case KEY_A:
			ret = CURSOR_SELECT;
			break;
		case KEY_B:
			ret = CURSOR_BACK;
			break;
		case KEY_X:
			ret = CURSOR_EXIT;
			break;
		case KEY_TOUCH:
			ret = CURSOR_TOUCH;
			break;
		default:
			ret = CURSOR_NONE;
			break;
	}

	return ret;
}

/*--------------------------------------------------------
	Wait any key in [key_list] pressed
	if key_list == NULL, return at any key pressed
--------------------------------------------------------*/
unsigned int wait_Anykey_press(unsigned int key_list)
{
	unsigned int key;

	while(1)
	{
		key = getKey();
		if(key) {
			if(0 == key_list) break;
			else if(key & key_list) break;
		}
	}

	return key;
}

/*--------------------------------------------------------
	Wait all key in [key_list] released
	if key_list == NULL, return at all key released
--------------------------------------------------------*/
void wait_Allkey_release(unsigned int key_list)
{
	unsigned int key;
    struct key_buf inputdata;

	while(1)
	{
		ds2_getrawInput(&inputdata);
		key = inputdata.key;

		if(0 == key) break;
		else if(!key_list) continue;
		else if(0 == (key_list & key)) break;
	}
}

static char ProgressAction[64];
static char ProgressFilename[MAX_PATH + 1];
static unsigned int ProgressCurrentFile; // 1-based
static unsigned int ProgressTotalFiles;
static unsigned int ProgressTotalSize;
static unsigned int ProgressDoneSize;
static unsigned int LastProgressUpdateTime; // getSysTime() units: 42.667 us

void InitProgress (char *Action, char *Filename, unsigned int TotalSize)
{
    strcpy(ProgressAction, Action);
    strcpy(ProgressFilename, Filename);
    ProgressTotalSize = TotalSize;
    LastProgressUpdateTime = 0;

    UpdateProgress (0);
}

void InitProgressMultiFile (char *Action, char *Filename, unsigned int TotalFiles)
{
    strcpy(ProgressAction, Action);
    strcpy(ProgressFilename, Filename);
    ProgressTotalFiles = TotalFiles;
}

void UpdateProgressChangeFile (unsigned int CurrentFile, char *Filename, unsigned int TotalSize)
{
    ProgressCurrentFile = CurrentFile;
    strcpy(ProgressFilename, Filename);
    ProgressTotalSize = TotalSize;
    LastProgressUpdateTime = 0; // force an update when changing files
    // if this is too slow, move it to InitProgressMultiFile above

    UpdateProgressMultiFile (0);
}

#define PROGRESS_BAR_WIDTH (ICON_PROGRESS.x)

void UpdateProgress (unsigned int DoneSize)
{
    ProgressDoneSize = DoneSize;

    unsigned int Now = getSysTime();
    if (Now - LastProgressUpdateTime >= 5859 /* 250 milliseconds in 42.667 us units */
        || ProgressDoneSize == ProgressTotalSize /* force update if done */)
    {
        LastProgressUpdateTime = Now;
        // If you want to add skinning support for the upper screen, edit this.
        ds2_clearScreen(UP_SCREEN, RGB15(0, 0, 0));

        draw_string_vcenter(up_screen_addr, 1, 48, 254, RGB15(31, 31, 31), ProgressAction);

        draw_string_vcenter(up_screen_addr, 1, 64, 254, RGB15(31, 31, 31), ProgressFilename);

        char ByteCountLine[128];
        sprintf(ByteCountLine, msg[FMT_PROGRESS_KIBIBYTE_COUNT], ProgressDoneSize / 1024, ProgressTotalSize / 1024);
        draw_string_vcenter(up_screen_addr, 1, 114, 254, RGB15(31, 31, 31), ByteCountLine);

        draw_string_vcenter(up_screen_addr, 1, 130, 254, RGB15(31, 31, 31), msg[MSG_PROGRESS_CANCEL_WITH_B]);

        unsigned int PixelsDone = (unsigned int) (((unsigned long long) ProgressDoneSize * (unsigned long long) PROGRESS_BAR_WIDTH) / (unsigned long long) ProgressTotalSize);

        show_icon(up_screen_addr, &ICON_NPROGRESS, (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80);
        show_partial_icon_horizontal(up_screen_addr, &ICON_PROGRESS, (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80, PixelsDone);

        ds2_flipScreen(UP_SCREEN, UP_SCREEN_UPDATE_METHOD);
    }
}

void UpdateProgressMultiFile (unsigned int DoneSize)
{
    ProgressDoneSize = DoneSize;

    unsigned int Now = getSysTime();
    if (Now - LastProgressUpdateTime >= 5859 /* 250 milliseconds in 42.667 us units */
        || ((ProgressDoneSize == ProgressTotalSize)
         && (ProgressCurrentFile == ProgressTotalFiles)) /* force update if done */)
    {
        LastProgressUpdateTime = Now;
        // If you want to add skinning support for the upper screen, edit this.
        ds2_clearScreen(UP_SCREEN, RGB15(0, 0, 0));

        draw_string_vcenter(up_screen_addr, 1, 48, 254, RGB15(31, 31, 31), ProgressAction);

        draw_string_vcenter(up_screen_addr, 1, 64, 254, RGB15(31, 31, 31), ProgressFilename);

        char ByteCountLine[128];
        sprintf(ByteCountLine, msg[FMT_PROGRESS_ARCHIVE_MEMBER_AND_KIBIBYTE_COUNT], ProgressCurrentFile, ProgressTotalFiles, ProgressDoneSize / 1024, ProgressTotalSize / 1024);
        draw_string_vcenter(up_screen_addr, 1, 114, 254, RGB15(31, 31, 31), ByteCountLine);

        draw_string_vcenter(up_screen_addr, 1, 130, 254, RGB15(31, 31, 31), msg[MSG_PROGRESS_CANCEL_WITH_B]);

        unsigned int PixelsDone = (unsigned int) (((unsigned long long) ProgressDoneSize * (unsigned long long) PROGRESS_BAR_WIDTH) / (unsigned long long) ProgressTotalSize);

        show_icon(up_screen_addr, &ICON_NPROGRESS, (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80);
        show_partial_icon_horizontal(up_screen_addr, &ICON_PROGRESS, (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2, 80, PixelsDone);

        ds2_flipScreen(UP_SCREEN, UP_SCREEN_UPDATE_METHOD);
    }
}

void InitMessage (void)
{
    ds2_setCPUclocklevel(0);

    mdelay(100); // to prevent ds2_setBacklight from crashing
    ds2_setBacklight(3);

    draw_message(down_screen_addr, NULL, 28, 31, 227, 165, COLOR_BG);
}

void FiniMessage (void)
{
    mdelay(100); // to prevent ds2_setBacklight from crashing
    ds2_setBacklight(2);

    wait_Allkey_release(0);
    ds2_setCPUclocklevel(13);
}

unsigned int ReadInputDuringCompression ()
{
    struct key_buf inputdata;

	ds2_getrawInput(&inputdata);

	return inputdata.key & ~(KEY_LID);
}

void change_ext(char *src, char *buffer, char *extension)
{
	char *dot_position;

	strcpy(buffer, src);
	dot_position = strrchr(buffer, '.');

	if(dot_position)
		strcpy(dot_position, extension);
}

/*--------------------------------------------------------
	Sort function
--------------------------------------------------------*/
static int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
	char *dest_str = ((char *)dest_str_ptr);
	char *src_str = ((char *)src_str_ptr);

	// For files and directories, . and .. sort first.
	if(src_str[0] == '.' && dest_str[0] != '.')
		return 1;

	if(dest_str[0] == '.' && src_str[0] != '.')
		return -1;

	return strcasecmp(dest_str, src_str);
}

static int my_array_partion(void **array, int left, int right)
{
	// Choose a pivot, left <= pivot <= right
	unsigned int pivotIndex = left + (right - left) / 2;

	// Move pivot value to the end
	void *temp = array[pivotIndex];
	array[pivotIndex] = array[right];
	array[right] = temp;

	// Move values that sort before the pivot value to before the new
	// pivot's location
	unsigned int storeIndex = left, i;
	for (i = left; i <= right - 1; i++)
	{
		if (sort_function(array[i], array[right]) < 0)
		{
			temp = array[i];
			array[i] = array[storeIndex];
			array[storeIndex] = temp;
			storeIndex++;
		}
	}

	// Move the pivot value to its correct location
	temp = array[storeIndex];
	array[storeIndex] = array[right];
	array[right] = temp;

	return storeIndex;
}

static void my_qsort(void **array, int left, int right)
{
    if(left < right)
    {
        int mid= my_array_partion(array, left, right);
        my_qsort(array, left, mid-1);
        my_qsort(array, mid+1, right);
    }
}

static void strupr(char *str)
{
    while(*str)
    {
        if(*str <= 0x7A && *str >= 0x61) *str -= 0x20;
        str++;
    }
}

// ******************************************************************************
//	get file list
// ******************************************************************************
#define FILE_LIST_MAX  512
#define DIR_LIST_MAX   64
#define NAME_MEM_SIZE  (320*64)

struct  FILE_LIST_INFO
{
    char current_path[MAX_PATH];
    char **wildcards;
    unsigned int file_num;
    unsigned int dir_num;
	unsigned int mem_size;
	unsigned int file_size;
	unsigned int dir_size;
    char **file_list;
    char **dir_list;
    char *filename_mem;
};

/*
*	Function: internal function to manage FILE_LIST_INFO structure
*	filelist_infop: a pointer to a predefined FILE_LIST_INFO structure
*	flag: 0	initialize the structure
*			1 increase filename memory size
*			2 increase file name buffer size
*			4 increase directory name buffer size
*			-1 free all allocated memroy
*			other value are invalide
*	return: -1 on failure
*/
static int manage_filelist_info(struct FILE_LIST_INFO *filelist_infop, int flag)
{
	//Initialize
	if(0 == flag)
	{
		filelist_infop->file_list = (char**)malloc(FILE_LIST_MAX*4);
		if(NULL == filelist_infop->file_list)
			return -1;

		filelist_infop->dir_list = (char**)malloc(DIR_LIST_MAX*4);
		if(NULL == filelist_infop->dir_list)
		{
			free((void*)filelist_infop->file_list);
			return -1;
		}

		filelist_infop->filename_mem = (char*)malloc(NAME_MEM_SIZE);
		if(NULL == filelist_infop->filename_mem)
		{
			free((void*)filelist_infop->file_list);
			free((void*)filelist_infop->dir_list);
			return -1;
		}

		filelist_infop->mem_size = NAME_MEM_SIZE;
		filelist_infop->file_size = FILE_LIST_MAX;
		filelist_infop->dir_size = DIR_LIST_MAX;
		return 0;
	}
	//free all memroy
	if(-1 == flag)
	{
		free((void*)filelist_infop->file_list);
		free((void*)filelist_infop->dir_list);
		free((void*)filelist_infop->filename_mem);
		return 0;
	}

	int i;
	void *pt;

	//Increase all 
	if(flag & 0x1)
	{
		i = NAME_MEM_SIZE;

		do
		{
			pt = (void*)realloc(filelist_infop->filename_mem, filelist_infop->mem_size+i);
			if(NULL == pt) i /= 2;
		} while(i > 256);

		if(NULL == pt) return -1;

		filelist_infop->mem_size += i;
		filelist_infop->filename_mem = (char*)pt;
		i = (int)pt - (int)filelist_infop->file_list;

		int k;
		char **m;

		k = 0;
		m = filelist_infop->file_list;
		for(k= 0; k < filelist_infop->file_num; k++)
			m[k] += i;

		k = 0;
		m = filelist_infop->dir_list;
		for(k = 0; k < filelist_infop->dir_num; k++)
			m[k] += i;
	}
	//Increase file name buffer
	if(flag & 0x2)
	{
		i = filelist_infop->file_size + FILE_LIST_MAX;

		pt = (void*)realloc(filelist_infop->file_list, i*4);
		if(NULL == pt) return -1;

		filelist_infop->file_list = (char**)pt;
		filelist_infop->file_size = i;
	}
	//Increase directory name buffer
	if(flag & 0x4)
	{
		i = filelist_infop->dir_size + DIR_LIST_MAX;

		pt = (void*)realloc(filelist_infop->dir_list, i*4);
		if(NULL == pt) return -1;

		filelist_infop->dir_list = (char**)pt;
		filelist_infop->dir_size = i;
	}

	return 0;
}

static int load_file_list(struct FILE_LIST_INFO *filelist_infop)
{
    DIR* current_dir;
    char    current_dir_name[MAX_PATH];
    dirent *current_file;
	struct stat st;
    char *file_name;
	unsigned int len;
    unsigned int file_name_length;
    char* name_mem_base;
    char **file_list;
    char **dir_list;
	unsigned int mem_size;
	unsigned int file_size;
	unsigned int dir_size;
    unsigned int num_files;
    unsigned int num_dirs;
    char **wildcards;

    if(filelist_infop -> current_path == NULL)
        return -1;

    name_mem_base = &(filelist_infop -> filename_mem[0]);
    mem_size = filelist_infop -> mem_size;
	file_size = filelist_infop -> file_size;
	dir_size = filelist_infop -> dir_size;
    file_list = filelist_infop -> file_list;
    dir_list = filelist_infop -> dir_list;
    num_files = 0;
    num_dirs = 0;
    wildcards = filelist_infop -> wildcards;

    strcpy(current_dir_name, filelist_infop -> current_path);

	//	path formate should be: "fat:/" or "fat:/dir0" or "fat:", not "fat:/dir0/"
    current_dir = opendir(current_dir_name);
	//Open directory faiure
	if(current_dir == NULL) {
		return -1;
	}

	file_name_length = 0;
	//while((current_file = readdir(current_dir)) != NULL)
	while((current_file = readdir_ex(current_dir, &st)) != NULL)
    {
		//lstat(current_file->d_name, &st);
		file_name = current_file->d_name;

		len = strlen(file_name) +1;
		if((file_name_length+len) > mem_size)
		{
			//get more memory
			if(manage_filelist_info(filelist_infop, 1) == -1)
				break;

			name_mem_base = &(filelist_infop -> filename_mem[0]);
		    mem_size = filelist_infop -> mem_size;
		}
		//Increase file_list
		if(num_files >= file_size) {
			if(manage_filelist_info(filelist_infop, 2) == -1)
				break;
			file_size = filelist_infop -> file_size;
		}
		//Increase dir_list
		if(num_dirs >= dir_size) {
			if(manage_filelist_info(filelist_infop, 4) == -1)
				break;
			dir_size = filelist_infop -> dir_size;
		}

		//If dirctory
		if(S_ISDIR(st.st_mode))
		{
			if(file_name[0] != '.')
			{
				dir_list[num_dirs] = name_mem_base + file_name_length;
				strcpy(dir_list[num_dirs], file_name);
				num_dirs++;
				file_name_length += len;
			}
			//take ".." directory as file
			else if(file_name[1] == '.')
			{
				file_list[num_files] = name_mem_base + file_name_length;
				strcpy(file_list[num_files], file_name);
				num_files++;
				file_name_length += len;
			}
		}
		else
		{
			if (wildcards[0] == NULL) // Show every file
			{
				file_list[num_files] = name_mem_base + file_name_length;
				strcpy(file_list[num_files], file_name);
				num_files++;
				file_name_length += len;
			}
			else
			{
				char *ext_pos;
				unsigned int i;

				ext_pos = (char*)strrchr((const char*)file_name, '.');
				if(NULL != ext_pos)
				for(i = 0; wildcards[i] != NULL; i++)
				{
					if(!strcasecmp(ext_pos, wildcards[i]))
					{
						file_list[num_files] = name_mem_base + file_name_length;
						strcpy(file_list[num_files], file_name);
						num_files++;
						file_name_length += len;
						break;
					}
				}
			}
		}
    }

    closedir(current_dir);

	filelist_infop -> file_num = num_files;
	filelist_infop -> dir_num = num_dirs;
	//printf("%s: num_files %d; num_dirs %d\n", filelist_infop ->current_path, num_files, num_dirs);
#if 0
    my_qsort((void *)file_list, 0, num_files-1);
#else	//to support ".." directory, but take it as file
    my_qsort((void **)file_list, 1, num_files-1);
#endif
    my_qsort((void **)dir_list, 0, num_dirs-1);

    return 0;
}

/*--------------------------------------------------------
  读取文件
--------------------------------------------------------*/
#define FILE_LIST_ROWS 6
#define FILE_LIST_ROWS_CENTER   ((FILE_LIST_ROWS+1)/2-1)
#define FILE_LIST_POSITION 42		//Started displaying x position

s32 load_file(char **wildcards, char *result, char *default_dir_name)
{
	struct FILE_LIST_INFO filelist_info;
	u32 repeat;
	int return_value;
	gui_action_type gui_action;
	u32 selected_item_on_list;
	u32 selected_item_on_screen;
	u32 to_update_filelist;
	u32 total_items_num;
	int redraw;
    u32 path_scroll;
    u32 num_files;						//File numbers on the directory
    u32 num_dirs;                       //Directory numbers on the dirctory
    char **file_list;
    char **dir_list;
    int flag;

	//Have no path
	if(NULL == default_dir_name)
		return -1;

	//construct filelist_info struct
	if(-1 == manage_filelist_info(&filelist_info, 0))
		return -1;

	result[0] = '\0';
	strcpy(filelist_info.current_path, default_dir_name);

	filelist_info.wildcards = wildcards;
	filelist_info.file_num = 0;
	filelist_info.dir_num = 0;

	flag = load_file_list(&filelist_info);
	if(-1 == flag)
	{
		//deconstruct filelist_info struct
		manage_filelist_info(&filelist_info, -1);
		return -1;
	}

	num_files = filelist_info.file_num;
	num_dirs = filelist_info.dir_num;
	file_list = filelist_info.file_list;
	dir_list = filelist_info.dir_list;

	selected_item_on_list = 0;			//Selected item on list
	selected_item_on_screen = 0;		//Selected item on screen's position
	redraw = -1;						//redraw all
	total_items_num = num_files + num_dirs;
	to_update_filelist = 0;             //to load new file list
	path_scroll = 0x8000;				//path scroll method, first scroll left

	repeat= 1;
	return_value = -1;
	while(repeat)
	{
		struct key_buf inputdata;
		gui_action = get_gui_input();
		int mod;

		// Get KEY_RIGHT and KEY_LEFT separately to allow scrolling
		// the selected file name faster.
		ds2_getrawInput(&inputdata);
		if (inputdata.key & KEY_RIGHT)
			redraw = -3;
		else if (inputdata.key & KEY_LEFT)
			redraw = 3;

		switch(gui_action)
		{
			case CURSOR_TOUCH:
				ds2_getrawInput(&inputdata);
				// ___ 33        This screen has 6 possible rows. Touches
				// ___ 60        above or below these are ignored.
				// . . . (+27)
				// ___ 192
				if(inputdata.y <= 33 || inputdata.y > 192)
					break;
				else
					mod = (inputdata.y - 33) / 27;

				if(selected_item_on_list - selected_item_on_screen + mod >= total_items_num)
					break;

				selected_item_on_list = selected_item_on_list - selected_item_on_screen + mod;

				if(selected_item_on_list + 1 <= num_files)
				{
					//The ".." directory is the parent
					if(!strcasecmp(file_list[selected_item_on_list], ".."))
					{
						char *ext_pos;

						strcpy(filelist_info.current_path, default_dir_name);
						ext_pos = strrchr(filelist_info.current_path, '/');
						if(NULL != ext_pos)	//Not root directory
						{
							*ext_pos = '\0';
							to_update_filelist= 1;
						}
					}
					else
					{
						repeat = 0;
						return_value = 0;
						strcpy(default_dir_name, filelist_info.current_path);
						strcpy(result, file_list[selected_item_on_list]);
					}
				}
				//directory selected
				else if(num_dirs > 0)
				{
					unsigned int m;

					m = selected_item_on_list - num_files;
					strcpy(filelist_info.current_path, default_dir_name);
					strcat(filelist_info.current_path, "/");
					strcat(filelist_info.current_path, dir_list[m]);
					to_update_filelist= 1;
				}
				break;
			case CURSOR_UP:
				redraw = 1;
				if(selected_item_on_screen > 0)
				{
					//Not the first item on list
					selected_item_on_list -= 1;
					//Selected item on screen center
					if(FILE_LIST_ROWS_CENTER == selected_item_on_screen) {
						if(selected_item_on_screen > selected_item_on_list)
							selected_item_on_screen -= 1;
					}
					//Selected item on down half screen
					else if(FILE_LIST_ROWS_CENTER < selected_item_on_screen) {
						selected_item_on_screen -= 1;
					}
					//Selected item on up half screen
					else {
						if(selected_item_on_screen < selected_item_on_list)
							selected_item_on_screen += 1;
						else if(selected_item_on_screen > selected_item_on_list)
							selected_item_on_screen -= 1;
					}
				}
				else if(selected_item_on_screen > selected_item_on_list) {
					selected_item_on_screen -= 1;
				}
				else
					redraw = 0;

				break;

			case CURSOR_DOWN:
			{
				unsigned int m, n;
				m = total_items_num - (selected_item_on_list + 1);
				n = FILE_LIST_ROWS - (FILE_LIST_ROWS_CENTER + 1);
				//Selected item on screen center
				if(FILE_LIST_ROWS_CENTER == selected_item_on_screen) {
					if(m > 0 && m < n)
						selected_item_on_screen += 1;
					redraw = 1;
				}
				//Selected item on down half screen
				else if(FILE_LIST_ROWS_CENTER < selected_item_on_screen) {
					if(m > 0) {
						redraw = 1;
						if(m <= n) selected_item_on_screen += 1;
						else if(m > n) selected_item_on_screen -= 1;
						else redraw = 0;	//cancel
					}
				}
				//Selected item on up half screen
				else {
					if(m > 0) {
						selected_item_on_screen += 1;
						redraw = 1;
					}
				}

				//Not reach the last item
				if((selected_item_on_list + 1) < total_items_num)
					selected_item_on_list += 1;

				break;
			}
			//scroll page down
			case CURSOR_RTRIGGER:
			{
				unsigned int m, n;
				//first item on screen in item list's position
				m = selected_item_on_list - selected_item_on_screen;
				n = total_items_num - (m+1);
				//there are more than 1 page after the m item
				if(n >= FILE_LIST_ROWS) {
					m += FILE_LIST_ROWS -1;
					selected_item_on_list = m;

					m = total_items_num - (selected_item_on_list +1);
					if(m >= (FILE_LIST_ROWS_CENTER +1))
						selected_item_on_screen = FILE_LIST_ROWS_CENTER;
					else
						selected_item_on_screen = 0;

					selected_item_on_list += selected_item_on_screen;
					redraw = 1;
				}

				break;
			}
			//scroll page up
			case CURSOR_LTRIGGER:
			{
				unsigned int m;
				//first item on screen in item list's position
				m = selected_item_on_list - selected_item_on_screen;
				//there are more than 1 page befroe the m item
				if((m+1) >= FILE_LIST_ROWS)
				{
					m -= FILE_LIST_ROWS -1;
					selected_item_on_list = m;

					selected_item_on_screen = FILE_LIST_ROWS_CENTER;
					selected_item_on_list += selected_item_on_screen;
					redraw = 1;
				}

				break;
			}
			//scroll string (see above the switch)
			case CURSOR_RIGHT:
				/* fall through */
			//scroll string (see above the switch)
			case CURSOR_LEFT:
				break;

			case CURSOR_SELECT:
				//file selected
				if(selected_item_on_list + 1 <= num_files)
				{
					//The ".." directory
					if(!strcasecmp(file_list[selected_item_on_list], ".."))
					{
						char *ext_pos;

						strcpy(filelist_info.current_path, default_dir_name);
						ext_pos = strrchr(filelist_info.current_path, '/');
						if(NULL != ext_pos)	//Not root directory
						{
							*ext_pos = '\0';
							to_update_filelist= 1;
						}
					}
					else
					{
						repeat = 0;
						return_value = 0;
						strcpy(default_dir_name, filelist_info.current_path);
						strcpy(result, file_list[selected_item_on_list]);
					}
				}
				//directory selected
				else if(num_dirs > 0)
				{
					unsigned int m;

					m = selected_item_on_list - num_files;
					strcpy(filelist_info.current_path, default_dir_name);
					strcat(filelist_info.current_path, "/");
					strcat(filelist_info.current_path, dir_list[m]);
					to_update_filelist= 1;
				}
				break;

			case CURSOR_BACK:
			{
				char *ext_pos;

				strcpy(filelist_info.current_path, default_dir_name);
				ext_pos = strrchr(filelist_info.current_path, '/');
				if(NULL != ext_pos)	//Not root directory
				{
					*ext_pos = '\0';
					to_update_filelist= 1;
				}
				else { //is root directory
					return_value = -1;
					repeat = 0;
				}
				break;
			}

			case CURSOR_EXIT:
				return_value = -1;
				repeat = 0;
				break;

			default:
				break;
		} //end switch

		if(to_update_filelist)
		{
			flag = load_file_list(&filelist_info);
			if(-1 != flag) {
				strcpy(default_dir_name, filelist_info.current_path);
				num_files = filelist_info.file_num;
				num_dirs = filelist_info.dir_num;
				total_items_num = num_files + num_dirs;;

				selected_item_on_list = 0;			//Selected item on list
				selected_item_on_screen = 0;		//Selected item on screen's position

				redraw = -1;						//redraw all
			}
			to_update_filelist = 0;
		}

		//redraw, code reuse
		if(-1 == redraw || 1 == redraw)
		{
			unsigned int m, n, k;
			char *pt;
			unsigned short color;

			//draw background
			show_icon(down_screen_addr, &ICON_SUBBG, 0, 0);
			show_icon(down_screen_addr, &ICON_TITLE, 0, 0);
			show_icon(down_screen_addr, &ICON_TITLEICON, 12, 9);

			//release data struct to draw scrolling string
			//Path
			if(-1 == redraw) {
				draw_hscroll_over(0);
				draw_hscroll_init(down_screen_addr, 49, 10, 199, COLOR_TRANS, 
					COLOR_WHITE, default_dir_name);
				path_scroll = 0x8000;		//first scroll left
			}

			for(m = 0; m < MAX_SCROLL_STRING; m++)
				draw_hscroll_over(m+1);

			//file and directory list
			//the first displayed item
			m = selected_item_on_list - selected_item_on_screen;
			//nubers of item to displayed
			n = total_items_num - m;
			if(n > FILE_LIST_ROWS) n = FILE_LIST_ROWS;

			for(k= 0; k < n; k++, m++)
			{
				if(k == selected_item_on_screen) {
					color = COLOR_ACTIVE_ITEM;
					show_icon(down_screen_addr, &ICON_SUBSELA, 6, 35 + k*27);
				}
				else
					color = COLOR_INACTIVE_ITEM;

				//directorys
				if((m+1) > num_files) {
					show_icon(down_screen_addr, &ICON_DIRECTORY, FILE_SELECTOR_ICON_X, 37 + k*27);
					pt = dir_list[m - num_files];
				}
				//files
				else {
					pt= strrchr(file_list[m], '.');

					if(!strcasecmp(pt, ".zip") || !strcasecmp(pt, ".gz"))
						show_icon(down_screen_addr, &ICON_ZIPFILE, FILE_SELECTOR_ICON_X, 37 + k*27);
					else if(!strcasecmp(file_list[m], ".."))
						show_icon(down_screen_addr, &ICON_DOTDIR, FILE_SELECTOR_ICON_X, 37 + k*27);
					else //Not recoganized file
						show_icon(down_screen_addr, &ICON_UNKNOW, FILE_SELECTOR_ICON_X, 37 + k*27);

					pt = file_list[m];
				}

				draw_hscroll_init(down_screen_addr, FILE_SELECTOR_NAME_X, 40 + k*27, FILE_SELECTOR_NAME_SX, 
					COLOR_TRANS, color, pt);
			}

			redraw = 0;
		} //end if(0 != redraw)
		else if(0 != redraw) {
			unsigned int m, n;
			char *pt;

			m = selected_item_on_screen;
			show_icon(down_screen_addr, &ICON_SUBSELA, 6, 35 + m*27);

			n = selected_item_on_list;
			if((n+1) > num_files)
				show_icon(down_screen_addr, &ICON_DIRECTORY, 17, 37 + m*27);
			else {
				pt= strrchr(file_list[n], '.');

				if(!strcasecmp(pt, ".zip") || !strcasecmp(pt, ".gz"))
					show_icon(down_screen_addr, &ICON_ZIPFILE, FILE_SELECTOR_ICON_X, 37 + m*27);
				else if(!strcasecmp(file_list[m], ".."))
					show_icon(down_screen_addr, &ICON_DOTDIR, FILE_SELECTOR_ICON_X, 37 + m*27);
				else //Not recoganized file
					show_icon(down_screen_addr, &ICON_UNKNOW, FILE_SELECTOR_ICON_X, 37 + m*27);
			}

			draw_hscroll(m+1, redraw);
			redraw = 0;
		}

		//Path auto scroll
		unsigned int m = path_scroll & 0xFF;
		if(m < 20) //pause 0.5sec
			path_scroll += 1;
		else {
			show_icon(down_screen_addr, &ICON_TITLE, 0, 0);
			show_icon(down_screen_addr, &ICON_TITLEICON, 12, 9);

			if(path_scroll & 0x8000)	//scroll left
			{
				if(draw_hscroll(0, -1) <= 1) path_scroll = 0;	 //scroll right
			}
			else
			{
				if(draw_hscroll(0, 1) <= 1) path_scroll = 0x8000; //scroll left
			}
		}

		ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

		mdelay(20);			//about 50ms
	} //end while(repeat)

	unsigned int i;
	for(i= 0; i < (FILE_LIST_ROWS +1); i++)
		draw_hscroll_over(i);

	//deconstruct filelist_info struct
	manage_filelist_info(&filelist_info, -1);

	// ds2_clearScreen(DOWN_SCREEN, COLOR_BLACK);
	// ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

	return return_value;
}


/*
*	Function: search directory on directory_path
*	directory: directory name will be searched
*	directory_path: path, note that the buffer which hold directory_path should
*		be large enough for nested
*	return: 0= found, directory lay on directory_path
*/
int search_dir(char *directory, char* directory_path)
{
    DIR *current_dir;
    dirent *current_file;
	struct stat st;
    int directory_path_len;

    current_dir= opendir(directory_path);
    if(current_dir == NULL)
        return -1;

	directory_path_len = strlen(directory_path);

	//while((current_file = readdir(current_dir)) != NULL)
	while((current_file = readdir_ex(current_dir, &st)) != NULL)
	{
		//Is directory 
		if(S_ISDIR(st.st_mode))
		{
			if(strcmp(".", current_file->d_name) || strcmp("..", current_file->d_name))
				continue;

			strcpy(directory_path+directory_path_len, current_file->d_name);

			if(!strcasecmp(current_file->d_name, directory))
			{//dirctory find
				closedir(current_dir);
				return 0;
			}

			if(search_dir(directory, directory_path) == 0)
			{//dirctory find
				closedir(current_dir);
				return 0;
			}

			directory_path[directory_path_len] = '\0';
		}
    }

    closedir(current_dir);
    return -1;
}

void savefast_int(void)
{

}

#if 1
void dump_mem(unsigned char* addr, unsigned int len)
{
	unsigned int i;

	for(i= 0; i < len; i++)
	{
		cprintf("%02x ", addr[i]);
		if((i+1)%16 == 0) cprintf("\n");
	}
}
#endif

/*--------------------------------------------------------
	Main Menu
--------------------------------------------------------*/
u32 menu()
{
    gui_action_type gui_action;
    u32 i;
    u32 repeat;
    u32 return_value = 0;
    char tmp_filename[MAX_FILE];
    char line_buffer[512];

    MENU_TYPE *current_menu = NULL;
    MENU_OPTION_TYPE *current_option = NULL;
    MENU_OPTION_TYPE *display_option = NULL;
    
    u32 current_option_num;
//    u32 parent_option_num;
    u32 string_select;

    u16 *bg_screenp;
    u32 bg_screenp_color;

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
	auto void show_card_space();

//Local function definition

	void menu_exit()
	{
		ds2_setCPUclocklevel(13); // Crank it up, leave quickly
		quit();
	}

	void menu_load_for_compression()
	{
		char *file_ext[] = { NULL }; // Show all files

		if(load_file(file_ext, tmp_filename, g_default_rom_dir) != -1)
		{
			strcpy(line_buffer, g_default_rom_dir);
			strcat(line_buffer, "/");
			strcat(line_buffer, tmp_filename);

			ds2_clearScreen(DOWN_SCREEN, RGB15(0, 0, 0));
			ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

			mdelay(100); // to prevent ds2_setBacklight from crashing
			ds2_setBacklight(2);

			ds2_setCPUclocklevel(13);
			u32 level = application_config.CompressionLevel;
			if (level == 0)
				level = 1;
			else if (level > 9)
				level = 9;
			while (!GzipCompress (line_buffer, level)); // retry if needed
			ds2_setCPUclocklevel(0);

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
		char *file_ext[] = { ".gz", ".zip", NULL };

		if(load_file(file_ext, tmp_filename, g_default_rom_dir) != -1)
		{
			strcpy(line_buffer, g_default_rom_dir);
			strcat(line_buffer, "/");
			strcat(line_buffer, tmp_filename);

			ds2_clearScreen(DOWN_SCREEN, RGB15(0, 0, 0));
			ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

			mdelay(100); // to prevent ds2_setBacklight from crashing
			ds2_setBacklight(2);

			ds2_setCPUclocklevel(13);
			if (strcasecmp(&line_buffer[strlen(line_buffer) - 3 /* .gz */], ".gz") == 0)
				while (!GzipDecompress (line_buffer)); // retry if needed
			else if (strcasecmp(&line_buffer[strlen(line_buffer) - 4 /* .zip */], ".zip") == 0)
				while (!ZipUncompress (line_buffer)); // retry if needed
			ds2_setCPUclocklevel(0);

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
            bg_screenp_color = COLOR16(43, 11, 11);
            memcpy(bg_screenp, down_screen_addr, 256*192*2);
        }
        else
            bg_screenp_color = COLOR_BG;

        draw_message(down_screen_addr, bg_screenp, 28, 31, 227, 165, bg_screenp_color);
        draw_string_vcenter(down_screen_addr, 36, 70, 190, COLOR_MSSG, msg[MSG_DIALOG_RESET]);

        if(draw_yesno_dialog(DOWN_SCREEN, 115, msg[MSG_GENERAL_CONFIRM_WITH_A], msg[MSG_GENERAL_CANCEL_WITH_B]))
        {
		wait_Allkey_release(0);
            draw_message(down_screen_addr, bg_screenp, 28, 31, 227, 165, bg_screenp_color);
            draw_string_vcenter(down_screen_addr, 36, 80, 190, COLOR_MSSG, msg[MSG_PROGRESS_RESETTING]);
            ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

            sprintf(line_buffer, "%s/%s", main_path, APPLICATION_CONFIG_FILENAME);
            remove(line_buffer);

            init_application_config();

		ds2_clearScreen(UP_SCREEN, 0);
		ds2_flipScreen(UP_SCREEN, 1);

		// mdelay(500); // Delete this delay
        }
    }

    void check_application_version()
    {
        if(bg_screenp != NULL)
        {
            bg_screenp_color = COLOR16(43, 11, 11);
            memcpy(bg_screenp, down_screen_addr, 256*192*2);
        }
        else
            bg_screenp_color = COLOR_BG;

        draw_message(down_screen_addr, bg_screenp, 28, 31, 227, 165, bg_screenp_color);
        draw_string_vcenter(down_screen_addr, 36, 80, 190, COLOR_MSSG, msg[MSG_APPLICATION_NAME]);
        sprintf(line_buffer, "%s %s", msg[MSG_WORD_APPLICATION_VERSION], DS2COMP_VERSION);
        draw_string_vcenter(down_screen_addr, 36, 95, 190, COLOR_MSSG, line_buffer);
        ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

		wait_Anykey_press(0);
    }

    void language_set()
    {
        if(gui_action == CURSOR_LEFT || gui_action == CURSOR_RIGHT)
        {
            ds2_setCPUclocklevel(13); // crank it up
            if(bg_screenp != NULL)
            {
                bg_screenp_color = COLOR16(43, 11, 11);
                memcpy(bg_screenp, down_screen_addr, 256*192*2);
            }
            else
                bg_screenp_color = COLOR_BG;

            draw_message(down_screen_addr, bg_screenp, 28, 31, 227, 165, bg_screenp_color);
            draw_string_vcenter(down_screen_addr, 36, 75, 190, COLOR_MSSG, msg[MSG_CHANGE_LANGUAGE]);
            draw_string_vcenter(down_screen_addr, 36, 95, 190, COLOR_MSSG, msg[MSG_CHANGE_LANGUAGE_WAITING]);
            ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

            load_language_msg(LANGUAGE_PACK, application_config.language);

            save_application_config_file();
            ds2_setCPUclocklevel(0); // and back down
            wait_Allkey_release(0);
        }
    }

	unsigned int freespace;
    void show_card_space ()
    {
        u32 line_num;
        u32 num_byte;

        strcpy(line_buffer, *(display_option->display_string));
        line_num= display_option-> line_number;
        PRINT_STRING_BG(down_screen_addr, line_buffer, COLOR_INACTIVE_ITEM, COLOR_TRANS, 27,
            40 + (display_option->line_number)*27);

		num_byte = freespace;

        if(num_byte <= 9999*2)
        { /* < 9999KB */
            sprintf(line_buffer, "%d", num_byte/2);
            if(num_byte & 1)
                strcat(line_buffer, ".5 KB");
            else
                strcat(line_buffer, ".0 KB");
        }
        else if(num_byte <= 9999*1024*2)
        { /* < 9999MB */
            num_byte /= 1024;
            sprintf(line_buffer, "%d", num_byte/2);
            if(num_byte & 1)
                strcat(line_buffer, ".5 MB");
            else
                strcat(line_buffer, ".0 MB");
        }
        else
        {
            num_byte /= 1024*1024;
            sprintf(line_buffer, "%d", num_byte/2);
            if(num_byte & 1)
                strcat(line_buffer, ".5 GB");
            else
                strcat(line_buffer, ".0 GB");
        }

        PRINT_STRING_BG(down_screen_addr, line_buffer, COLOR_INACTIVE_ITEM, COLOR_TRANS, 147,
            40 + (display_option->line_number)*27);
    }

    char *on_off_options[] = { (char*)&msg[MSG_GENERAL_OFF], (char*)&msg[MSG_GENERAL_ON] };

  /*--------------------------------------------------------
     Others
  --------------------------------------------------------*/
    u32 desert= 0;
	MENU_OPTION_TYPE others_options[] = 
	{
	/* 00 */ SUBMENU_OPTION(NULL, &msg[MSG_MAIN_MENU_OPTIONS], NULL, 0),

	/* 01 */ NUMERIC_SELECTION_OPTION(NULL, &msg[FMT_OPTIONS_COMPRESSION_LEVEL], &application_config.CompressionLevel, 10, NULL, 1),

	/* 02 */ STRING_SELECTION_OPTION(language_set, NULL, &msg[FMT_OPTIONS_LANGUAGE], language_options, 
        &application_config.language, sizeof(language_options) / sizeof(language_options[0]) /* number of possible languages */, NULL, ACTION_TYPE, 2),

	/* 03 */ STRING_SELECTION_OPTION(NULL, show_card_space, &msg[MSG_OPTIONS_CARD_CAPACITY], NULL, 
        &desert, 2, NULL, PASSIVE_TYPE | HIDEN_TYPE, 3),

	/* 04 */ ACTION_OPTION(load_default_setting, NULL, &msg[MSG_OPTIONS_RESET], NULL, 4),

	/* 05 */ ACTION_OPTION(check_application_version, NULL, &msg[MSG_OPTIONS_VERSION], NULL, 5),
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
		show_icon(down_screen_addr, &ICON_MAINBG, 0, 0);
		current_menu -> focus_option = current_option -> line_number;

		// Compress
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(down_screen_addr, &ICON_COMPRESS, 0, 0);
			show_icon(down_screen_addr, &ICON_MSEL, 24, 81);
		}
		else {
			show_icon(down_screen_addr, &ICON_NCOMPRESS, 0, 0);
			show_icon(down_screen_addr, &ICON_MNSEL, 24, 81);
		}
		draw_string_vcenter(down_screen_addr, 26, 81, 75, COLOR_WHITE, line_buffer);

		// Decompress
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(down_screen_addr, &ICON_DECOMPRESS, 128, 0);
			show_icon(down_screen_addr, &ICON_MSEL, 152, 81);
		}
		else {
			show_icon(down_screen_addr, &ICON_NDECOMPRESS, 128, 0);
			show_icon(down_screen_addr, &ICON_MNSEL, 152, 81);
		}
		draw_string_vcenter(down_screen_addr, 154, 81, 75, COLOR_WHITE, line_buffer);

		// Options
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(down_screen_addr, &ICON_OPTIONS, 0, 96);
			show_icon(down_screen_addr, &ICON_MSEL, 24, 177);
		}
		else {
			show_icon(down_screen_addr, &ICON_NOPTIONS, 0, 96);
			show_icon(down_screen_addr, &ICON_MNSEL, 24, 177);
		}
		draw_string_vcenter(down_screen_addr, 26, 177, 75, COLOR_WHITE, line_buffer);

		// Exit
		strcpy(line_buffer, *(display_option->display_string));
		if(display_option++ == current_option) {
			show_icon(down_screen_addr, &ICON_EXIT, 128, 96);
			show_icon(down_screen_addr, &ICON_MSEL, 152, 177);
		}
		else {
			show_icon(down_screen_addr, &ICON_NEXIT, 128, 96);
			show_icon(down_screen_addr, &ICON_MNSEL, 152, 177);
		}
		draw_string_vcenter(down_screen_addr, 154, 177, 75, COLOR_WHITE, line_buffer);
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
		unsigned int total, used;

		//get card space info
		freespace = 0;
		fat_getDiskSpaceInfo("fat:", &total, &used, &freespace);
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
		}

		current_menu = new_menu;
		current_option_num= current_menu -> focus_option;
		current_option = new_menu->options + current_option_num;
		if(current_menu->init_function)
			current_menu->init_function();
	}

//----------------------------------------------------------------------------//
//	Menu Start
	ds2_setCPUclocklevel(0);
	mdelay(100); // to prevent ds2_setBacklight() from crashing
	ds2_setBacklight(1);
	
	wait_Allkey_release(0);
	bg_screenp= (u16*)malloc(256*192*2);

	repeat = 1;

	ds2_clearScreen(UP_SCREEN, RGB15(0, 0, 0));
	ds2_flipScreen(UP_SCREEN, UP_SCREEN_UPDATE_METHOD);

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
			u32 line_num, screen_focus, focus_option;

			//draw background
			show_icon(down_screen_addr, &ICON_SUBBG, 0, 0);
			show_icon(down_screen_addr, &ICON_TITLE, 0, 0);
			show_icon(down_screen_addr, &ICON_TITLEICON, 12, 9);

			strcpy(line_buffer, *(display_option->display_string));
			draw_string_vcenter(down_screen_addr, 0, 9, 256, COLOR_ACTIVE_ITEM, line_buffer);

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
				show_icon(down_screen_addr, &ICON_BACK, 229, 10);
			else
				show_icon(down_screen_addr, &ICON_NBACK, 229, 10);

			for(i= 0; i < line_num; i++, display_option++)
    	    {
    	        unsigned short color;

				if(display_option == current_option)
					show_icon(down_screen_addr, &ICON_SUBSELA, 6, 35 + i*27);

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
						*((u32*)(((u32 *)display_option->options)[*(display_option->current_option)])));
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
	
					PRINT_STRING_BG(down_screen_addr, line_buffer, color, COLOR_TRANS, OPTION_TEXT_X, 40 + i*27);
				}
    	    }
    	}

		struct key_buf inputdata;
		gui_action = get_gui_input();

		switch(gui_action)
		{
			case CURSOR_TOUCH:
				ds2_getrawInput(&inputdata);
				/* Back button at the top of every menu but the main one */
				if(current_menu != &main_menu && inputdata.x > 231 && inputdata.y <= 25)
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

					current_option_num = (inputdata.y / 96) * 2 + (inputdata.x / 128);
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
					if (inputdata.y <= 33 || inputdata.y > 192)
						break;
					// ___ 33        This screen has 6 possible rows. Touches
					// ___ 60        above or below these are ignored.
					// . . . (+27)   The row between 33 and 60 is [1], though!
					// ___ 192
					u32 next_option_num = (inputdata.y - 33) / 27 + 1;
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
						u32 current_option_val = *(current_option->current_option);

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
						u32 current_option_val = *(current_option->current_option);

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
						u32 current_option_val = *(current_option->current_option);

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

		ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);
	} // end while

	if (current_menu && current_menu->end_function)
		current_menu->end_function();

	if(bg_screenp != NULL) free((void*)bg_screenp);
	
	ds2_clearScreen(DOWN_SCREEN, 0);
	ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);
	wait_Allkey_release(0);

	mdelay(100); // to prevent ds2_setBacklight() from crashing
	ds2_setBacklight(2);

	ds2_setCPUclocklevel(13);

	return return_value;
}

/*--------------------------------------------------------
	Load language message
--------------------------------------------------------*/
int load_language_msg(char *filename, u32 language)
{
	FILE *fp;
	char msg_path[MAX_PATH];
	char string[256];
	char start[32];
	char end[32];
	char *pt, *dst;
	u32 loop, offset, len;
	int ret;

	sprintf(msg_path, "%s/%s", main_path, filename);
	fp = fopen(msg_path, "rb");
	if(fp == NULL)
	return -1;

	switch(language)
	{
	case ENGLISH:
	default:
		strcpy(start, "STARTENGLISH");
		strcpy(end, "ENDENGLISH");
		break;
	case FRENCH:
		strcpy(start, "STARTFRENCH");
		strcpy(end, "ENDFRENCH");
		break;
	}
	u32 cmplen = strlen(start);

	//find the start flag
	ret= 0;
	while(1)
	{
		pt= fgets(string, 256, fp);
		if(pt == NULL)
		{
			ret= -2;
			goto load_language_msg_error;
		}

		if(!strncmp(pt, start, cmplen))
			break;
	}

	loop= 0;
	offset= 0;
	dst= msg_data;
	msg[0]= dst;

	while(loop != MSG_END)
	{
		while(1)
		{
			pt = fgets(string, 256, fp);
			if(pt[0] == '#' || pt[0] == '\r' || pt[0] == '\n')
				continue;
			if(pt != NULL)
				break;
			else
			{
				ret = -3;
				goto load_language_msg_error;
			}
		}

		if(!strncmp(pt, end, cmplen-2))
			break;


		len= strlen(pt);
		// memcpy(dst, pt, len);

		// Replace key definitions (*letter) with Pictochat icons
		// while copying.
		unsigned int srcChar, dstLen = 0;
		for (srcChar = 0; srcChar < len; srcChar++)
		{
			if (pt[srcChar] == '*')
			{
				switch (pt[srcChar + 1])
				{
				case 'A':
					memcpy(&dst[dstLen], HOTKEY_A_DISPLAY, sizeof (HOTKEY_A_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_A_DISPLAY) - 1;
					break;
				case 'B':
					memcpy(&dst[dstLen], HOTKEY_B_DISPLAY, sizeof (HOTKEY_B_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_B_DISPLAY) - 1;
					break;
				case 'X':
					memcpy(&dst[dstLen], HOTKEY_X_DISPLAY, sizeof (HOTKEY_X_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_X_DISPLAY) - 1;
					break;
				case 'Y':
					memcpy(&dst[dstLen], HOTKEY_Y_DISPLAY, sizeof (HOTKEY_Y_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_Y_DISPLAY) - 1;
					break;
				case 'L':
					memcpy(&dst[dstLen], HOTKEY_L_DISPLAY, sizeof (HOTKEY_L_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_L_DISPLAY) - 1;
					break;
				case 'R':
					memcpy(&dst[dstLen], HOTKEY_R_DISPLAY, sizeof (HOTKEY_R_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_R_DISPLAY) - 1;
					break;
				case 'S':
					memcpy(&dst[dstLen], HOTKEY_START_DISPLAY, sizeof (HOTKEY_START_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_START_DISPLAY) - 1;
					break;
				case 's':
					memcpy(&dst[dstLen], HOTKEY_SELECT_DISPLAY, sizeof (HOTKEY_SELECT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_SELECT_DISPLAY) - 1;
					break;
				case 'u':
					memcpy(&dst[dstLen], HOTKEY_UP_DISPLAY, sizeof (HOTKEY_UP_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_UP_DISPLAY) - 1;
					break;
				case 'd':
					memcpy(&dst[dstLen], HOTKEY_DOWN_DISPLAY, sizeof (HOTKEY_DOWN_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_DOWN_DISPLAY) - 1;
					break;
				case 'l':
					memcpy(&dst[dstLen], HOTKEY_LEFT_DISPLAY, sizeof (HOTKEY_LEFT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_LEFT_DISPLAY) - 1;
					break;
				case 'r':
					memcpy(&dst[dstLen], HOTKEY_RIGHT_DISPLAY, sizeof (HOTKEY_RIGHT_DISPLAY) - 1);
					srcChar++;
					dstLen += sizeof (HOTKEY_RIGHT_DISPLAY) - 1;
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
			}
			else
			{
				dst[dstLen] = pt[srcChar];
				dstLen++;
			}
		}

		dst += dstLen;
		//at a line return, when "\n" paded, this message not end
		if(*(dst-1) == 0x0A)
		{
			pt = strrchr(pt, '\\');
			if((pt != NULL) && (*(pt+1) == 'n'))
			{
				if(*(dst-2) == 0x0D)
				{
					*(dst-4)= '\n';
					dst -= 3;
				}
				else
				{
					*(dst-3)= '\n';
					dst -= 2;
				}
			}
			else//a message end
			{
				if(*(dst-2) == 0x0D)
					dst -= 1;
				*(dst-1) = '\0';
				msg[++loop] = dst;
			}
		}
	}

	#if 0
	loop= 0;
	printf("------\n");
	while(loop != MSG_END)
	printf("%d: %s\n",loop, msg[loop++]);
	#endif

load_language_msg_error:
	fclose(fp);
	return ret;
}

/*--------------------------------------------------------
--------------------------------------------------------*/


/*--------------------------------------------------------
	Load font library
--------------------------------------------------------*/
u32 load_font()
{
    return (u32)BDF_font_init();
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
	char tmp_path[MAX_PATH];
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
    char tmp_path[MAX_PATH];
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
/*
  u32 reg_ra;

  __asm__ __volatile__("or %0, $0, $ra"
                        : "=r" (reg_ra)
                        :);
  
  dbg_printf("return address= %08x\n", reg_ra);
*/

#ifdef USE_DEBUG
	fclose(g_dbg_file);
#endif

	mdelay(100); // to prevent ds2_setBacklight from crashing
	ds2_setBacklight(3); // return to the OS with both backlights on!

	ds2_plug_exit();
	while(1);
}

u32 file_length(FILE* file)
{
  u32 pos, size;
  pos= ftell(file);
  fseek(file, 0, SEEK_END);
  size= ftell(file);
  fseek(file, pos, SEEK_SET);

  return size;
}

/*
*	GUI Initialize
*/
void gui_init(u32 lang_id)
{
	int flag;

	ds2_setCPUclocklevel(13); // Crank it up. When the menu starts, -> 0.

    //Find the "DS2COMP" system directory
    DIR *current_dir;

    strcpy(main_path, "fat:/DS2COMP");
    current_dir = opendir(main_path);
    if(current_dir)
        closedir(current_dir);
    else
    {
        strcpy(main_path, "fat:/_SYSTEM/PLUGINS/DS2COMP");
        current_dir = opendir(main_path);
        if(current_dir)
            closedir(current_dir);
        else
        {
            strcpy(main_path, "fat:");
            if(search_dir("DS2COMP", main_path) == 0)
            {
                printf("Found DS2COMP directory\r\nDossier DS2COMP trouve\r\n\r\n%s\r\n", main_path);
            }
            else
            {
				err_msg(DOWN_SCREEN, "/DS2COMP: Directory missing\r\nPress any key to return to\r\nthe menu\r\n\r\n/DS2COMP: Dossier manquant\r\nAppuyer sur une touche pour\r\nretourner au menu");
                goto gui_init_err;
            }
        }
    }

	show_log(down_screen_addr);
	ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);

	flag = icon_init(lang_id);
	if(0 != flag)
	{
		err_msg(DOWN_SCREEN, "Some icons are missing\r\nLoad them onto your card\r\nPress any key to return to\r\nthe menu\r\n\r\nDes icones sont manquantes\r\nChargez-les sur votre carte\r\nAppuyer sur une touche pour\r\nretourner au menu");
		goto gui_init_err;
	}


	flag = load_font();
	if(0 != flag)
	{
		char message[512];
		sprintf(message, "Font library initialisation\r\nerror (%d)\r\nPress any key to return to\r\nthe menu\r\n\r\nErreur d'initalisation de la\r\npolice de caracteres (%d)\r\nAppuyer sur une touche pour\r\nretourner au menu", flag, flag);
		err_msg(DOWN_SCREEN, message);
		goto gui_init_err;
	}

	load_application_config_file();
	lang_id = application_config.language;

	flag = load_language_msg(LANGUAGE_PACK, lang_id);
	if(0 != flag)
	{
		char message[512];
		sprintf(message, "Language pack initialisation\r\nerror (%d)\r\nPress any key to return to\r\nthe menu\r\n\r\nErreur d'initalisation du\r\npack de langue (%d)\r\nAppuyer sur une touche pour\r\nretourner au menu", flag, flag);
		err_msg(DOWN_SCREEN, message);
		goto gui_init_err;
	}

	strcpy(g_default_rom_dir, "fat:");

	return;

gui_init_err:
	ds2_flipScreen(DOWN_SCREEN, DOWN_SCREEN_UPDATE_METHOD);
	wait_Anykey_press(0);
	quit();
}
