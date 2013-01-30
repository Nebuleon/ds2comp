/* minigzip.c -- simulate gzip using the zlib compression library
 * Copyright (C) 1995-1998 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/*
 * minigzip is a minimal implementation of the gzip utility. This is
 * only an example of using zlib and isn't meant to replace the
 * full-featured gzip. No attempt is made to deal with file systems
 * limiting names to 14 or 8+3 characters, etc... Error checking is
 * very limited. So use minigzip only for testing; use gzip for the
 * real thing. On MSDOS, use only on file names without extension
 * or in pipe mode.
 */

/* @(#) $Id: minigzip.c,v 1.1.1.1 1999/04/23 02:07:09 wsanchez Exp $ */

#define DS2COMP_RETRY 55

#include "zlib.h"
#include "zutil.h"

#ifdef STDC
#  include <string.h>
// #  include <stdlib.h>
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

#define BUFLEN      65536
#define MAX_NAME_LEN 1024

#include "gui.h"
#include "draw.h"
#include "message.h"

int  error            OF((char *message));
int  gz_compress      OF((FILE   *in, gzFile out));
int  gz_uncompress    OF((gzFile in, FILE   *out));
int  GzipCompress     OF((char  *file, unsigned int level));
int  GzipUncompress   OF((char  *file));

/* ===========================================================================
 * Display error message, asking if the user wishes to retry.
 * Return DS2COMP_RETRY if he or she wants to retry, and Z_ERRNO if not.
 */
int error(message)
    char *message;
{
    InitMessage ();
    draw_string_vcenter(down_screen_addr, 36, 70, 190, COLOR_MSSG, message);

    u32 result = draw_yesno_dialog(DOWN_SCREEN, 115, msg[MSG_ERROR_RETRY_WITH_A], msg[MSG_ERROR_ABORT_WITH_B]);
    FiniMessage ();

    return (result == 1) ? DS2COMP_RETRY : Z_ERRNO;
}

/* ===========================================================================
 * Compress input to output then close both files.
 * Return Z_OK on success, Z_ERRNO or DS2COMP_RETRY otherwise.
 */

int gz_compress(in, out)
    FILE   *in;
    gzFile out;
{
    local char buf[BUFLEN];
    int len;
    int err;

    for (;;) {
        len = fread(buf, 1, sizeof(buf), in);
        if (len < 0) {
            fclose(in);
            gzclose(out);
            return error(msg[MSG_ERROR_INPUT_FILE_READ]);
        }
        if (len == 0) break;

        if (gzwrite(out, buf, len) != len) {
            gzerror(out, &err);
            fclose(in);
            gzclose(out);
            return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]);
        }
    }
    fclose(in);
    if (gzclose(out) != Z_OK) {
        return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]);
    }

    return Z_OK;
}

/* ===========================================================================
 * Uncompress input to output then close both files.
 * Return Z_OK on success, Z_ERRNO or DS2COMP_RETRY otherwise.
 */
int gz_uncompress(in, out)
    gzFile in;
    FILE   *out;
{
    local char buf[BUFLEN];
    int len;
    int err;

    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) {
            gzclose(in);
            fclose(out);
            // return error (gzerror(in, &err));
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]);
        }
        if (len == 0) break;

        if (fwrite(buf, 1, len, out) != len) {
            gzclose(in);
            fclose(out);
            return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]);
	}
    }
    if (fclose(out)) {
        gzclose(in);
        return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]);
    }

    if (gzclose(in) != Z_OK) {
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]);
    }

    return Z_OK;
}


/* ===========================================================================
 * Compress the given file: create a corresponding .gz file and remove the
 * original.
 * Returns 1 on success or if the user does not want to retry.
 * Returns 0 on failure if the user wants to retry.
 */
int GzipCompress(file, level)
    char  *file;
    unsigned int level;
{
    local char outfile[MAX_NAME_LEN];
    local char mode[4];

    strcpy(mode, "wb ");
    if (level > 9)
        level = 9;
    mode[2] = (char) ('0' + level);

    FILE  *in;
    gzFile out;
    int    err;

    strcpy(outfile, file);
    strcat(outfile, GZ_SUFFIX);

    out = gzopen(outfile, mode);
    if (out == NULL) {
        gzerror(out, &err);
        // Even if err == Z_OK, a NULL gzFile is unusable.
        return error(msg[MSG_ERROR_OUTPUT_FILE_OPEN]) != DS2COMP_RETRY;
    }
    in = fopen(file, "rb");
    if (in == NULL) {
        gzclose(out);
        return error(msg[MSG_ERROR_INPUT_FILE_READ]) != DS2COMP_RETRY;
    }

    int result = gz_compress(in, out);
    if (result == Z_OK) {
        fat_remove(file); // compression succeeded, delete the original file
        return 1;
    } else {
        fat_remove(outfile); // compression failed, delete the output file (PARTIAL)
        return result != DS2COMP_RETRY;
    }
}


/* ===========================================================================
 * Uncompress the given file and remove the original.
 * Returns 1 on success or if the user does not want to retry.
 * Returns 0 on failure if the user wants to retry.
 */
int GzipDecompress(file)
    char  *file;
{
    local char buf[MAX_NAME_LEN];
    char *infile, *outfile;
    FILE  *out;
    gzFile in;
    int    err;
    int len = strlen(file);

    strcpy(buf, file);

    if (len > SUFFIX_LEN && strcmp(file+len-SUFFIX_LEN, GZ_SUFFIX) == 0) {
        infile = file;
        outfile = buf;
        outfile[len-SUFFIX_LEN] = '\0';
    } else {
        outfile = file;
        infile = buf;
        strcat(infile, GZ_SUFFIX);
    }
    in = gzopen(infile, "rb");
    if (in == NULL) {
        gzerror(in, &err);
        // Even if err == Z_OK, a NULL gzFile is unusable.
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
    }
    out = fopen(outfile, "wb");
    if (out == NULL) {
        gzclose(in);
        return error(msg[MSG_ERROR_OUTPUT_FILE_OPEN]) != DS2COMP_RETRY;
    }

    int result = gz_uncompress(in, out);
    if (result == Z_OK) {
        fat_remove(infile); // compression succeeded, delete the original file
        return 1;
    } else {
        fat_remove(outfile); // compression failed, delete the output file (PARTIAL)
        return result != DS2COMP_RETRY;
    }
}
