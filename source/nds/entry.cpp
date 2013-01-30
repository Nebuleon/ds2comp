//entry.c
#include <stdio.h>
#include <string.h>

#include "ds2_types.h"
#include "ds2_cpu.h"
#include "ds2_timer.h"
#include "ds2io.h"
#include "fs_api.h"

#include "draw.h"
#include "gui.h"
#include "entry.h"

const char *S9xBasename (const char *f)
{
    const char *p;
    if ((p = strrchr (f, '/')) != NULL || (p = strrchr (f, '\\')) != NULL)
	return (p + 1);

    return (f);
}

void _makepath (char *path, const char *, const char *dir,
		const char *fname, const char *ext)
{
    if (dir && *dir)
    {
	strcpy (path, dir);
	strcat (path, "/");
    }
    else
	*path = 0;
    strcat (path, fname);
    if (ext && *ext)
    {
        strcat (path, ".");
        strcat (path, ext);
    }
}

void _splitpath (const char *path, char *drive, char *dir, char *fname,
		 char *ext)
{
    *drive = 0;

    char *slash = strrchr (path, '/');
    if (!slash)
	slash = strrchr (path, '\\');

    char *dot = strrchr (path, '.');

    if (dot && slash && dot < slash)
	dot = NULL;

    if (!slash)
    {
	strcpy (dir, "");
	strcpy (fname, path);
        if (dot)
        {
	    *(fname + (dot - path)) = 0;
	    strcpy (ext, dot + 1);
        }
	else
	    strcpy (ext, "");
    }
    else
    {
	strcpy (dir, path);
	*(dir + (slash - path)) = 0;
	strcpy (fname, slash + 1);
        if (dot)
	{
	    *(fname + (dot - slash) - 1) = 0;
    	    strcpy (ext, dot + 1);
	}
	else
	    strcpy (ext, "");
    }
}

extern "C" int sfc_main (int argc, char **argv);

int sfc_main (int argc, char **argv)
{
	//Initialize GUI
	gui_init(0);

	while (1)
	{
		menu();
	}

    return (0);
}

unsigned int ReadInputDuringCompression ()
{
    struct key_buf inputdata;

	ds2_getrawInput(&inputdata);

	return inputdata.key & ~(KEY_LID);
}

