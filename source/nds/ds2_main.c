/* ds2_main.c
 *
 * Copyright (C) 2017 Nebuleon Fumika <nebuleon.fumika@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
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

#include <ds2/ds.h>
#include <ds2/pm.h>
#include <stdlib.h>
#include "gui.h"

int main(int argc, char** argv)
{
	DS2_HighClockSpeed();

	DS2_SetPixelFormat(DS_ENGINE_BOTH, DS2_PIXEL_FORMAT_BGR555);
	DS2_SetScreenSwap(true);

	gui_init(0);

	while (1) {
		menu();
	}

	return EXIT_SUCCESS;
}
