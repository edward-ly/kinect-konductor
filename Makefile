# Author: Edward Ly
# Last Modified: 31 October 2016
# CS 488 Senior Capstone Project
# Makefile for Main Program

XKIN_LIBS_DIR = ./XKin/build/lib

EXECUTABLES = main
CFLAGS = -Wall -ggdb -fPIC `pkg-config --cflags opencv`
LIB_PATHS = -L${XKIN_LIBS_DIR}/body -L${XKIN_LIBS_DIR}/hand -L${XKIN_LIBS_DIR}/posture -L${XKIN_LIBS_DIR}/gesture -Wl,-R/usr/local/lib,-R${XKIN_LIBS_DIR}/body,-R${XKIN_LIBS_DIR}/hand,-R${XKIN_LIBS_DIR}/posture,-R${XKIN_LIBS_DIR}/gesture
LIBS = `pkg-config --libs opencv` -lfreenect -lfreenect_sync -lfreenect_cv -lbody -lhand -lposture -lgesture -lfluidsynth
# Other libraries already included from pkg-config command:
# -lm -lpthread

main: main.c include.h
	gcc ${CFLAGS} ${LIB_PATHS} $< -o $@ ${LIBS}

clean:
	rm -f ${EXECUTABLES}

all:
	@make clean
	@make ${EXECUTABLES}

