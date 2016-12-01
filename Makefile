# Author: Edward Ly
# Last Modified: 1 December 2016
# CS 488 Senior Capstone Project
# Makefile for Main Program
# Kinect Konductor: a simple virtual conductor application for Kinect for Windows v1.

XKIN_LIBS_DIR = XKin/build/lib

EXECUTABLES = main
CFLAGS = -Wall -ggdb -fPIC `pkg-config --cflags opencv`

LIB_PATHS = -L${XKIN_LIBS_DIR}/body -L${XKIN_LIBS_DIR}/hand -L${XKIN_LIBS_DIR}/posture -L${XKIN_LIBS_DIR}/gesture -Wl,-R/usr/local/lib,-R${XKIN_LIBS_DIR}/body,-R${XKIN_LIBS_DIR}/hand,-R${XKIN_LIBS_DIR}/posture,-R${XKIN_LIBS_DIR}/gesture

# Other libraries already included from pkg-config: -lm -lpthread
OPENCV_LIBS = `pkg-config --libs opencv`
FREENECT_LIBS = -lfreenect -lfreenect_sync -lfreenect_cv
XKIN_LIBS = -lbody -lhand -lposture -lgesture
FLUIDSYNTH_LIBS = -lfluidsynth
LIBS = ${OPENCV_LIBS} ${FREENECT_LIBS} ${XKIN_LIBS} ${FLUIDSYNTH_LIBS}

main: main.c app.h include.h
	gcc ${CFLAGS} ${LIB_PATHS} $< -o $@ ${LIBS}

clean:
	rm -f ${EXECUTABLES}

all:
	@make clean
	@make ${EXECUTABLES}

