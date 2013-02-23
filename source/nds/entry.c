//entry.c

#include "ds2io.h"
#include "gui.h"

int ds2compress_main (int argc, char **argv);

int ds2compress_main (int argc, char **argv)
{
	//Initialize GUI
	gui_init(0);

	while (1)
	{
		menu();
	}

    return (0);
}

