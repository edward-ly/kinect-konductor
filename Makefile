# Author: Edward Ly
# Last Modified: 31 October 2016
# CS 488 Senior Capstone Project
# Makefile for Main Program

EXECUTABLES = main
CFLAGS = -Wall -ggdb -fPIC `pkg-config --cflags opencv`
LIBRARIES = `pkg-config --libs opencv` -lfreenect -lfreenect_sync -lfreenect_cv -lportaudio

XKIN_LIBS_DIR = ./XKin/build/lib
XKIN_LIBS = ${XKIN_LIBS_DIR}/body/libbody.so ${XKIN_LIBS_DIR}/hand/libhand.so ${XKIN_LIBS_DIR}/posture/libposture.so ${XKIN_LIBS_DIR}/gesture/libgesture.so

main: main.c
	gcc ${CFLAGS} $< -o $@ ${LIBRARIES} ${XKIN_LIBS}

clean:
	rm -f ${EXECUTABLES}

all:
	@make clean
	@make ${EXECUTABLES}

