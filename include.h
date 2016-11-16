// File: include.h
// Author: Edward Ly
// Last Modified: 15 November 2016
// Includes all libraries and structs used in main application.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <libfreenect/libfreenect_sync.h>
#include <libfreenect/libfreenect_cv.h>
#include <fluidsynth.h>

#include "XKin/include/libbody.h"
#include "XKin/include/libhand.h"
#include "XKin/include/libposture.h"
#include "XKin/include/libgesture.h"

typedef struct point_t {
	CvPoint point;
	clock_t time;
} point_t;

typedef struct note_t {
	double beat;
	int channel;
	int key;
	int noteOn;
} note_t;

