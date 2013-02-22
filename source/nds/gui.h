/* gui.h
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

#ifndef __GUI_H__
#define __GUI_H__

#include "ds2_types.h"
#include "fs_api.h"

#define UP_SCREEN_UPDATE_METHOD   2
#define DOWN_SCREEN_UPDATE_METHOD 2

// For general option text
#define OPTION_TEXT_X             10
#define OPTION_TEXT_SX            236

// For the file selector
#define FILE_SELECTOR_ICON_X      10
#define FILE_SELECTOR_NAME_X      32
#define FILE_SELECTOR_NAME_SX     214

#ifdef __cplusplus
extern "C" {
#endif

//
struct _APPLICATION_CONFIG
{
  u32 language;
  u32 CompressionLevel;
  u32 Reserved[126];
};

typedef enum
{
  CURSOR_NONE = 0,
  CURSOR_UP,
  CURSOR_DOWN,
  CURSOR_LEFT,
  CURSOR_RIGHT,
  CURSOR_SELECT,
  CURSOR_BACK,
  CURSOR_EXIT,
  CURSOR_RTRIGGER,
  CURSOR_LTRIGGER,
  CURSOR_KEY_SELECT,
  CURSOR_TOUCH
} gui_action_type;

extern char main_path[MAX_PATH];

/******************************************************************************
 ******************************************************************************/
extern char g_default_rom_dir[MAX_PATH];

typedef struct _APPLICATION_CONFIG		APPLICATION_CONFIG;

extern APPLICATION_CONFIG	application_config;

/******************************************************************************
 ******************************************************************************/
extern void gui_init(u32 lang_id);
extern u32 menu();
extern int load_language_msg(char *filename, u32 language);

extern void InitMessage (void);
extern void FiniMessage (void);
extern void InitProgress (char *Action, char *Filename, unsigned int TotalSize);
extern void UpdateProgress (unsigned int DoneSize);
extern void InitProgressMultiFile (char *Action, char *Filename, unsigned int TotalFiles);
extern void UpdateProgressChangeFile (unsigned int CurrentFile, unsigned int TotalSize);
extern void UpdateProgressMultiFile (unsigned int DoneSize);
extern unsigned int ReadInputDuringCompression ();

#ifdef __cplusplus
}
#endif

#endif //__GUI_H__
