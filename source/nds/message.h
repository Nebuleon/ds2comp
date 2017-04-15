/* message.h
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

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

enum MSG {
	MSG_MAIN_MENU_COMPRESS,
	MSG_MAIN_MENU_DECOMPRESS,
	MSG_MAIN_MENU_OPTIONS,
	MSG_MAIN_MENU_EXIT,

	MSG_FILE_MENU_LOADING_LIST,
	MSG_FILE_MENU_SORTING_LIST,

	MSG_OPTIONS_COMPRESSION_LEVEL,
	MSG_OPTIONS_LANGUAGE,
	MSG_OPTIONS_CARD_CAPACITY,
	MSG_OPTIONS_RESET,
	MSG_OPTIONS_VERSION,

	MSG_GENERAL_OFF,
	MSG_GENERAL_ON,

	MSG_GENERAL_CONFIRM_WITH_A,
	MSG_GENERAL_CANCEL_WITH_B,
	MSG_GENERAL_YES_WITH_A,
	MSG_GENERAL_NO_WITH_B,

	MSG_CHANGE_LANGUAGE,
	MSG_CHANGE_LANGUAGE_WAITING,

	MSG_APPLICATION_NAME,
	MSG_WORD_APPLICATION_VERSION,

	MSG_DIALOG_RESET,
	MSG_PROGRESS_RESETTING,

	MSG_DIALOG_OVERWRITE_EXISTING_FILE,
	MSG_DIALOG_OVERWRITE_ALL_FILES,
	MSG_DIALOG_LEAVE_ALL_FILES,
	MSG_FILE_OVERWRITE_WITH_A,
	MSG_FILE_LEAVE_WITH_B,

	MSG_PROGRESS_COMPRESSING,
	MSG_PROGRESS_DECOMPRESSING,
	FMT_PROGRESS_KIBIBYTE_COUNT,
	FMT_PROGRESS_ARCHIVE_MEMBER_AND_KIBIBYTE_COUNT,
	MSG_PROGRESS_CANCEL_WITH_B,

	MSG_ERROR_INPUT_FILE_READ,
	MSG_ERROR_COMPRESSED_FILE_READ,
	MSG_ERROR_OUTPUT_FILE_OPEN,
	MSG_ERROR_OUTPUT_FILE_WRITE,

	MSG_ERROR_RETRY_WITH_A,
	MSG_ERROR_ABORT_WITH_B,

	MSG_END
};

enum LANGUAGE {
	ENGLISH,
	FRENCH,
	SPANISH,
	GERMAN,
	DUTCH,
	LANG_END  /* Must be last */
};

extern const char* lang[LANG_END]; // Allocated in gui.c, needs to match the languages ^

extern const char *msg[MSG_END + 1];
extern char msg_data[16 * 1024];

#endif //__MESSAGE_H__

