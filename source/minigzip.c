/* minigzip.c -- simulate gzip using the zlib compression library
 * Copyright (C) 1995-2006, 2010, 2011, 2016 Jean-loup Gailly
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

/* @(#) $Id$ */

#include "zlib.h"
#include <stdio.h>

#define DS2COMP_RETRY 55
#define DS2COMP_STOP  56

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

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  ifdef UNDER_CE
#    include <stdlib.h>
#  endif
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#  define snprintf _snprintf
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

#if !defined(Z_HAVE_UNISTD_H) && !defined(_LARGEFILE64_SOURCE)
#ifndef WIN32 /* unlink already in stdio.h for WIN32 */
  extern int unlink OF((const char *));
#endif
#endif

#if defined(UNDER_CE)
#  include <windows.h>
#  define perror(s) pwinerror(s)

/* Map the Windows error number in ERROR to a locale-dependent error
   message string and return a pointer to it.  Typically, the values
   for ERROR come from GetLastError.
   The string pointed to shall not be modified by the application,
   but may be overwritten by a subsequent call to strwinerror
   The strwinerror function does not change the current setting
   of GetLastError.  */

static char *strwinerror (error)
     DWORD error;
{
    static char buf[1024];

    wchar_t *msgbuf;
    DWORD lasterr = GetLastError();
    DWORD chars = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        error,
        0, /* Default language */
        (LPVOID)&msgbuf,
        0,
        NULL);
    if (chars != 0) {
        /* If there is an \r\n appended, zap it.  */
        if (chars >= 2
            && msgbuf[chars - 2] == '\r' && msgbuf[chars - 1] == '\n') {
            chars -= 2;
            msgbuf[chars] = 0;
        }

        if (chars > sizeof (buf) - 1) {
            chars = sizeof (buf) - 1;
            msgbuf[chars] = 0;
        }

        wcstombs(buf, msgbuf, chars + 1);
        LocalFree(msgbuf);
    }
    else {
        sprintf(buf, "unknown win32 error (%ld)", error);
    }

    SetLastError(lasterr);
    return buf;
}

static void pwinerror (s)
    const char *s;
{
    if (s && *s)
        fprintf(stderr, "%s: %s\n", s, strwinerror(GetLastError ()));
    else
        fprintf(stderr, "%s\n", strwinerror(GetLastError ()));
}

#endif /* UNDER_CE */

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

#ifdef Z_SOLO
/* for Z_SOLO, create simplified gz* functions using deflate and inflate */

#if defined(Z_HAVE_UNISTD_H) || defined(Z_LARGE)
#  include <unistd.h>       /* for unlink() */
#endif

void *myalloc OF((void *, unsigned, unsigned));
void myfree OF((void *, void *));

void *myalloc(q, n, m)
    void *q;
    unsigned n, m;
{
    (void)q;
    return calloc(n, m);
}

void myfree(q, p)
    void *q, *p;
{
    (void)q;
    free(p);
}

typedef struct gzFile_s {
    FILE *file;
    int write;
    int err;
    char *msg;
    z_stream strm;
} *gzFile;

gzFile gzopen OF((const char *, const char *));
gzFile gzdopen OF((int, const char *));
gzFile gz_open OF((const char *, int, const char *));

gzFile gzopen(path, mode)
const char *path;
const char *mode;
{
    return gz_open(path, -1, mode);
}

gzFile gzdopen(fd, mode)
int fd;
const char *mode;
{
    return gz_open(NULL, fd, mode);
}

gzFile gz_open(path, fd, mode)
    const char *path;
    int fd;
    const char *mode;
{
    gzFile gz;
    int ret;

    gz = malloc(sizeof(struct gzFile_s));
    if (gz == NULL)
        return NULL;
    gz->write = strchr(mode, 'w') != NULL;
    gz->strm.zalloc = myalloc;
    gz->strm.zfree = myfree;
    gz->strm.opaque = Z_NULL;
    if (gz->write)
        ret = deflateInit2(&(gz->strm), -1, 8, 15 + 16, 8, 0);
    else {
        gz->strm.next_in = 0;
        gz->strm.avail_in = Z_NULL;
        ret = inflateInit2(&(gz->strm), 15 + 16);
    }
    if (ret != Z_OK) {
        free(gz);
        return NULL;
    }
    gz->file = path == NULL ? fdopen(fd, gz->write ? "wb" : "rb") :
                              fopen(path, gz->write ? "wb" : "rb");
    if (gz->file == NULL) {
        gz->write ? deflateEnd(&(gz->strm)) : inflateEnd(&(gz->strm));
        free(gz);
        return NULL;
    }
    gz->err = 0;
    gz->msg = "";
    return gz;
}

int gzwrite OF((gzFile, const void *, unsigned));

int gzwrite(gz, buf, len)
    gzFile gz;
    const void *buf;
    unsigned len;
{
    z_stream *strm;
    unsigned char out[BUFLEN];

    if (gz == NULL || !gz->write)
        return 0;
    strm = &(gz->strm);
    strm->next_in = (void *)buf;
    strm->avail_in = len;
    do {
        strm->next_out = out;
        strm->avail_out = BUFLEN;
        (void)deflate(strm, Z_NO_FLUSH);
        fwrite(out, 1, BUFLEN - strm->avail_out, gz->file);
    } while (strm->avail_out == 0);
    return len;
}

int gzread OF((gzFile, void *, unsigned));

int gzread(gz, buf, len)
    gzFile gz;
    void *buf;
    unsigned len;
{
    int ret;
    unsigned got;
    unsigned char in[1];
    z_stream *strm;

    if (gz == NULL || gz->write)
        return 0;
    if (gz->err)
        return 0;
    strm = &(gz->strm);
    strm->next_out = (void *)buf;
    strm->avail_out = len;
    do {
        got = fread(in, 1, 1, gz->file);
        if (got == 0)
            break;
        strm->next_in = in;
        strm->avail_in = 1;
        ret = inflate(strm, Z_NO_FLUSH);
        if (ret == Z_DATA_ERROR) {
            gz->err = Z_DATA_ERROR;
            gz->msg = strm->msg;
            return 0;
        }
        if (ret == Z_STREAM_END)
            inflateReset(strm);
    } while (strm->avail_out);
    return len - strm->avail_out;
}

int gzclose OF((gzFile));

int gzclose(gz)
    gzFile gz;
{
    z_stream *strm;
    unsigned char out[BUFLEN];

    if (gz == NULL)
        return Z_STREAM_ERROR;
    strm = &(gz->strm);
    if (gz->write) {
        strm->next_in = Z_NULL;
        strm->avail_in = 0;
        do {
            strm->next_out = out;
            strm->avail_out = BUFLEN;
            (void)deflate(strm, Z_FINISH);
            fwrite(out, 1, BUFLEN - strm->avail_out, gz->file);
        } while (strm->avail_out == 0);
        deflateEnd(strm);
    }
    else
        inflateEnd(strm);
    fclose(gz->file);
    free(gz);
    return Z_OK;
}

const char *gzerror OF((gzFile, int *));

const char *gzerror(gz, err)
    gzFile gz;
    int *err;
{
    *err = gz->err;
    return gz->msg;
}

#endif

#define COMPRESSION_BUFFER_SIZE    16384
#define DECOMPRESSION_BUFFER_SIZE 131072
#define MAX_NAME_LEN                1024

#include "gui.h"
#include "draw.h"
#include "message.h"

int  error            OF((const char *message));
int  gz_compress      OF((FILE   *in, gzFile out));
int  gz_uncompress    OF((gzFile in, FILE   *out));
int  GzipCompress     OF((const char  *file, unsigned int level));
int  GzipUncompress   OF((const char  *file));

/* ===========================================================================
 * Display error message, asking if the user wishes to retry.
 * Return DS2COMP_RETRY if he or she wants to retry, and Z_ERRNO if not.
 */
int error(message)
    const char *message;
{
    InitMessage();
    draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, message);

    bool result = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_ERROR_RETRY_WITH_A], msg[MSG_ERROR_ABORT_WITH_B]);
    FiniMessage();

    return result ? DS2COMP_RETRY : Z_ERRNO;
}

/* ===========================================================================
 * Compress input to output then close both files.
 * Return Z_OK on success, Z_ERRNO or DS2COMP_RETRY otherwise.
 * May return DS2COMP_STOP if the user interrupted the process.
 */

int gz_compress(in, out)
    FILE   *in;
    gzFile out;
{
    local char buf[COMPRESSION_BUFFER_SIZE];
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

        if (ReadInputDuringCompression() & DS_BUTTON_B) {
            fclose(in);
            gzclose(out);
            return DS2COMP_STOP;
        }

        UpdateProgress(ftell(in));
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
 * May return DS2COMP_STOP if the user interrupted the process.
 */
int gz_uncompress(in, out)
    gzFile in;
    FILE   *out;
{
    local char buf[DECOMPRESSION_BUFFER_SIZE];
    int len;
    int err;

    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) {
            gzerror(in, &err);
            gzclose(in);
            fclose(out);
            return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]);
        }
        if (len == 0) break;

        if (fwrite(buf, 1, len, out) != len) {
            gzclose(in);
            fclose(out);
            return error(msg[MSG_ERROR_OUTPUT_FILE_WRITE]);
        }

        if (ReadInputDuringCompression() & DS_BUTTON_B) {
            gzclose(in);
            fclose(out);
            return DS2COMP_STOP;
        }

        UpdateProgress(gzoffset(in));
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
 * Returns 1 on success or if the user does not want to retry or has
 *   interrupted the process.
 * Returns 0 on failure if the user wants to retry.
 */
int GzipCompress(file, level)
    const char  *file;
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

    {
        FILE *outCheck = fopen(outfile, "rb");
        if (outCheck) {
            // The .gz file exists. Ask the user if he or she wishes to
            // overwrite it.
            fclose(outCheck);  // ... after closing it
            InitMessage();
            draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_OVERWRITE_EXISTING_FILE]);

            bool result = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_FILE_OVERWRITE_WITH_A], msg[MSG_FILE_LEAVE_WITH_B]);
            FiniMessage();
            if (!result /* leave it */)
                return 1; // user aborted
        }
    }

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

    // Get the length of the source file for progress indication
    fseek(in, 0, SEEK_END);
    InitProgress(msg[MSG_PROGRESS_COMPRESSING], file, ftell(in));
    fseek(in, 0, SEEK_SET);

    int result = gz_compress(in, out);
    if (result == Z_OK) {
        remove(file); // compression succeeded, delete the original file
        return 1;
    } else {
        remove(outfile); // compression failed, delete the output file (PARTIAL)
        return result != DS2COMP_RETRY;
    }
}


/* ===========================================================================
 * Uncompress the given file and remove the original.
 * Returns 1 on success or if the user does not want to retry or has
 *   interrupted the process.
 * Returns 0 on failure if the user wants to retry.
 */
int GzipUncompress(file)
    const char  *file;
{
    local char buf[MAX_NAME_LEN];
    const char *infile, *outfile;
    FILE  *out;
    gzFile in;
    int    err;
    int len = strlen(file);

    strcpy(buf, file);

    if (len > SUFFIX_LEN && strcmp(file+len-SUFFIX_LEN, GZ_SUFFIX) == 0) {
        infile = file;
        buf[len-SUFFIX_LEN] = '\0';
        outfile = buf;
    } else {
        outfile = file;
        strcat(buf, GZ_SUFFIX);
        infile = buf;
    }

    {
        FILE *outCheck = fopen(outfile, "rb");
        if (outCheck) {
            // The uncompressed file exists. Ask the user if he or she wishes
            // to overwrite it.
            fclose(outCheck);  // ... after closing it
            InitMessage();
            draw_string_vcenter(DS2_GetSubScreen(), MESSAGE_BOX_TEXT_X, MESSAGE_BOX_TEXT_Y, MESSAGE_BOX_TEXT_SX, COLOR_MSSG, msg[MSG_DIALOG_OVERWRITE_EXISTING_FILE]);

            bool result = draw_yesno_dialog(DS_ENGINE_SUB, msg[MSG_FILE_OVERWRITE_WITH_A], msg[MSG_FILE_LEAVE_WITH_B]);
            FiniMessage();
            if (!result /* leave it */)
                return 1; // user aborted
        }
    }

    in = gzopen(infile, "rb");
    if (in == NULL) {
        gzerror(in, &err);
        // Even if err == Z_OK, a NULL gzFile is unusable.
        return error(msg[MSG_ERROR_COMPRESSED_FILE_READ]) != DS2COMP_RETRY;
    }

    { // Get the length of the compressed file for progress indication
        FILE *inCheck = fopen(infile, "rb");
        if (inCheck == NULL) { // It existed just now...
            gzclose(in);
            return error(msg[MSG_ERROR_INPUT_FILE_READ]) != DS2COMP_RETRY;
        }
        fseek(inCheck, 0, SEEK_END);
        InitProgress(msg[MSG_PROGRESS_DECOMPRESSING], infile, ftell(inCheck));
        fclose(inCheck);
    }

    out = fopen(outfile, "wb");
    if (out == NULL) {
        gzclose(in);
        return error(msg[MSG_ERROR_OUTPUT_FILE_OPEN]) != DS2COMP_RETRY;
    }

    int result = gz_uncompress(in, out);
    if (result == Z_OK) {
        remove(infile); // compression succeeded, delete the original file
        return 1;
    } else {
        remove(outfile); // compression failed, delete the output file (PARTIAL)
        return result != DS2COMP_RETRY;
    }
}
