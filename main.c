// File: main.c
// Author: Edward Ly
// Last Modified: 1 December 2016
// Description: A simple virtual conductor application for Kinect for Windows v1.
// See the LICENSE file for license information.

#include "app.h"

int main (int argc, char* argv[]) {
	parse_args(argc, argv);
	parse_music();
	fluid_init();

	main_loop();

	release_all();
	return 0;
}

