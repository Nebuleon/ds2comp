//entry.c

#include "ds2io.h"
#include "gui.h"

int sfc_main (int argc, char **argv);

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

