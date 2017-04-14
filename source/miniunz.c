/* miniunz.c -- unzipper with progress reporting for the Supercard DSTwo
 *
 * (C) 2013 GBAtemp user Nebuleon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define DS2COMP_RETRY 55
#define DS2COMP_STOP  56

#include "zlib.h"
#include "unzip.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef STDC
#  include <string.h>
#  include <stdlib.h>
#else
   extern void exit  OF((int));
#endif

#ifdef USE_MMAP
#  include <sys/types.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

#if defined(MSDOS) || defined(OS2) || defined(WIN32)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#ifdef VMS
#  define unlink delete
#  define GZ_SUFFIX "-gz"
#endif
#ifdef RISCOS
#  define unlink remove
#  define GZ_SUFFIX "-gz"
#  define fileno(file) file->__file
#endif
#if defined(__MWERKS__) && __dest_os != __be_os && __dest_os != __win32_os
#  include <unix.h> /* for fileno */
#endif

#ifndef WIN32 /* unlink already in stdio.h for WIN32 */
  extern int unlink OF((const char *));
#endif

#ifndef GZ_SUFFIX
#  define GZ_SUFFIX ".gz"
#endif
#define SUFFIX_LEN (sizeof(GZ_SUFFIX)-1)

#ifdef MAXSEG_64K
#  define local static
   /* Needed for systems with limitation on stack size. */
#else
#  define local
#endif

#define DECOMPRESSION_BUFFER_SIZE 131072
#define MAX_NAME_LEN                1024

#include "gui.h"
#include "draw.h"
#include "message.h"

extern int error    OF((const char *message)); // to avoid duplicate definitions,
                                               // this one is in minigzip.c
int ZipUncompress   OF((const char  *file));

/* ===========================================================================
 * Uncompress the given .zip file and preserve it.
 * Returns 1 on success or if the user does not want to retry or has
 *   interrupted the process.
 * Returns 0 on failure if the user wants to retry.
 */
int ZipUncompress(file)
    const char  *file;
{
    // Files are extracted relative to the path containing the .zip file.
    char Path[PATH_MAX + 1];
    strcpy(Path, file);
    char *pt = strrchr(Path, '/');
    if (pt == NULL)
        return 1;
    *pt = '\0';

    FILE  *out;
    gzFile in;
    int    err;

    in = unzOpen(file);
    if (in == NULL) {
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
    }

    unz_global_info global_info;
    if (unzGetGlobalInfo(in, &global_info) != UNZ_OK) {
        unzClose(in);
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
    }

    InitProgressMultiFile(msg[MSG_PROGRESS_DECOMPRESSING], file, global_info.number_entry);

    if (unzGoToFirstFile(in) != UNZ_OK) {
        unzClose(in);
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
    }
    unsigned int CurrentFile = 0;
    bool OverwriteAllFiles = false;
    bool LeaveAllFiles = false;
    bool AllFilesAsked = false;
    // For each file...
    for (;;) {
        // 1. Get the size and name of this file. Update progress accordingly.
        unz_file_info file_info;
        char Filename[PATH_MAX + 1];
        if (unzGetCurrentFileInfo(in, &file_info, Filename, sizeof (Filename), NULL, 0 /* not interested in the extra field */, NULL, 0 /* not interested in the global comment */) != UNZ_OK) {
            unzClose(in);
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
        }

        if (Filename[0] && Filename[strlen(Filename) - 1] == '/') {
            CurrentFile++;
            goto next_file;
        }
        else
            UpdateProgressChangeFile(++CurrentFile, Filename, file_info.uncompressed_size);

        char outfile[PATH_MAX + 1];
        strcpy(outfile, Path);
        strcat(outfile, "/");
        strcat(outfile, Filename); // buffer overflow possible
        // 2. Check whether the file exists.
        unsigned int FileExists = 0;
        FILE *outCheck = fopen(outfile, "rb");
        if (outCheck) {
            // The target file exists. Ask the user if he or she wishes
            // to overwrite it.
            FileExists = 1;
            fclose(outCheck);  // ... after closing it
        }
        // a) It does, but the user already said they want to leave files
        //    intact. Next!
        if (FileExists && LeaveAllFiles /* && AllFilesAsked */)
            goto next_file;
        // b) It does, and the user has not said anything about all files.
        if (FileExists && !OverwriteAllFiles /* && !AllFilesAsked */)
        {
            // This file already exists. Overwrite it?
            InitMessage();
            draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_OVERWRITE_EXISTING_FILE]);

            bool OverwriteCurrentFile = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_FILE_OVERWRITE_WITH_A], msg[MSG_FILE_LEAVE_WITH_B]);
            FiniMessage();

            if (!AllFilesAsked) {
                // Additionally, do you wish to {overwrite | leave} all files
                // for this .zip archive?
                InitMessage();
                if (OverwriteCurrentFile) {
                    draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_OVERWRITE_ALL_FILES]);
                    OverwriteAllFiles = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_GENERAL_YES_WITH_A], msg[MSG_GENERAL_NO_WITH_B]);
                } else {
                    draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_LEAVE_ALL_FILES]);
                    LeaveAllFiles = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_GENERAL_YES_WITH_A], msg[MSG_GENERAL_NO_WITH_B]);
                }
                FiniMessage();
                AllFilesAsked = 1;
            }

            if (OverwriteCurrentFile == 0 /* leave it */) {
                goto next_file;
            }
        }

        // 3. Make missing parent directories.
        // Assume the directory containing the .zip archive exists.
        unsigned int DirLen = 0;
        while (Filename[DirLen]) {
            if (Filename[DirLen] == '/') { // Found a new path component
                char IntermediatePath[PATH_MAX + 1];
                strcpy(IntermediatePath, Path);
                strcat(IntermediatePath, "/");
                Filename[DirLen] = '\0';
                strcat(IntermediatePath, Filename); // buffer overflow possible
                Filename[DirLen] = '/';
                DIR *IntermediateDir = opendir(IntermediatePath);
                if (IntermediateDir)
                    closedir(IntermediateDir);
                else
                    mkdir(IntermediatePath, 0755);
            }
            DirLen++;
        }

        // 4. Open the output file.
        out = fopen(outfile, "wb");
        if (out == NULL) {
            unzClose(in);
            return error(msg[MSG_ERROR_OUTPUT_FILE_OPEN]) != DS2COMP_RETRY;
        }

        if (unzOpenCurrentFile(in) != UNZ_OK) {
            unzClose(in);
            fclose(out);
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
        }

        local char buf[DECOMPRESSION_BUFFER_SIZE];
        int len;

        // 5. Unpack into the output file. Update progress accordingly.
        for (;;) {
            len = unzReadCurrentFile(in, buf, sizeof(buf));
            if (len < 0) {
                unzCloseCurrentFile(in);
                unzClose(in);
                fclose(out);
                remove(outfile); // PARTIAL FILE
                return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
            }
            if (len == 0) break;

            if (fwrite(buf, 1, len, out) != len) {
                unzCloseCurrentFile(in);
                unzClose(in);
                fclose(out);
                remove(outfile); // PARTIAL FILE
                return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]) != DS2COMP_RETRY;
            }

            if (ReadInputDuringCompression() & DS_BUTTON_B) {
                unzCloseCurrentFile(in);
                unzClose(in);
                fclose(out);
                remove(outfile); // PARTIAL FILE
                return 1;
            }

            UpdateProgressMultiFile(unztell(in));
        }

        fclose(out);
        if (unzCloseCurrentFile(in) != UNZ_OK) { // CRC32 mismatch
            unzClose(in);
            remove(outfile); // BAD FILE
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
        }

next_file: ;
        int result = unzGoToNextFile(in);
        if (result == UNZ_END_OF_LIST_OF_FILE) {
            unzCloseCurrentFile(in);
            unzClose(in);
            return 1;
        } else if (result != UNZ_OK) {
            unzCloseCurrentFile(in);
            unzClose(in);
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
        }
    }

    // The .zip file should already be closed above, but if control goes here,
    // then close it anyway.
    unzClose(in);
    return 1;
}
