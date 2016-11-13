// File: main.c
// Author: Edward Ly
// Last Modified: 7 November 2016
// Description: A simple virtual conductor application for Kinect for Windows v1.

// Contains modified source code from the example files demogesture.c (XKin) and paex_saw.c (PortAudio) with the below licenses.

/* 
 * PortAudio is copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*  
 * XKin is copyright (c) 2012, Fabrizio Pedersoli
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <portaudio.h>

#include "./XKin/include/libbody.h"
#include "./XKin/include/libhand.h"
#include "./XKin/include/libposture.h"
#include "./XKin/include/libgesture.h"

typedef struct paData {
	float left_phase;
	float right_phase;
} paData;

typedef struct point_t {
	CvPoint point;
	clock_t time;
} point_t;

const int SCREENX = 1200, SCREENY = 800;
const int WIN_TYPE = CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE;
const int NUM_MILLISECONDS = 100, SAMPLE_RATE = 44100, A = 440;
const int WIDTH = 640, HEIGHT = 480, TIMER = 10;
const int MAX_POINTS = 5, THRESHOLD = 256;
const double MIN_DISTANCE = 12.0, MAX_DISTANCE = 224.0;
const double MAX_ACCEL = 32768.0, BEATS_PER_MEASURE = 4.0;

bool debug_stream = false;
bool debug_time = false;
bool debug_beat = false;

double currentBeat = -5.0;
static paData data;
clock_t time1, time2;
double seconds, BPM;
int front = 0, count = 0, vel1, vel2;
double accel;

const int NUM_NOTES = 8;
int notes[] = {60, 62, 63, 67, 68, 72, -1, -1};
float ramp = 0;

double         diffclock               (clock_t, clock_t);
bool           is_inside_window        (CvPoint);
double         distance                (CvPoint, CvPoint);
double         velocity_y              (point_t, point_t);
double         midi_to_freq            (int);
float          freq_to_ramp            (double);
float          midi_to_ramp            (int);
IplImage*      draw_depth_hand         (CvSeq*, int, point_t[]);
static int     paCallback              (const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);

//////////////////////////////////////////////////////

int main (int argc, char *argv[]) {
	const char *win_hand = "depth hand";
	point_t points[MAX_POINTS];
	bool beatIsReady = false;
	time1 = clock(); int n;

	for (n = 0; n < MAX_POINTS; n++) {
		points[n].point.x = WIDTH/2;
		points[n].point.y = HEIGHT/2;
		points[n].time = time1;
	}

	PaStream *stream;
	PaError err;
	data.left_phase = data.right_phase = 0.0;

	err = Pa_Initialize();
	if (err != paNoError) goto error;

	err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, paCallback, &data);
	if (err != paNoError) goto error;

	cvNamedWindow(win_hand, WIN_TYPE);
	cvMoveWindow(win_hand, SCREENX - WIDTH/2, HEIGHT/2);
	
	while (true) {
		IplImage *depth, *body, *hand, *a;
		CvSeq *cnt;
		CvPoint cent;
		int z, i;
		
		depth = freenect_sync_get_depth_cv(0);
		
		body = body_detection(depth);
		hand = hand_detection(body, &z);

		if (!get_hand_contour_basic(hand, &cnt, &cent))
			continue;

		if (!is_inside_window(cent))
			continue;

		if ((count == 0) || distance(cent, points[(front + count) % MAX_POINTS].point) < MAX_DISTANCE) {
			if (count < MAX_POINTS) {
				points[(front + count) % MAX_POINTS].time = clock();
				points[(front + count++) % MAX_POINTS].point = cent;
			}
			else {
				points[front].time = clock();
				points[front++].point = cent;
				front %= MAX_POINTS;
			}

			vel2 = -1 * velocity_y(points[(front + MAX_POINTS - 1) % MAX_POINTS], points[(front + MAX_POINTS - 3) % MAX_POINTS]);
			vel1 = -1 * velocity_y(points[(front + 2) % MAX_POINTS], points[front]);
			accel = vel2 - vel1 / diffclock(points[(front + MAX_POINTS - 1) % MAX_POINTS].time, points[(front + 2) % MAX_POINTS].time);

			if (debug_stream) {
				for (i = 0; i < MAX_POINTS; i++)
					fprintf(stderr, "(%i, %i), ", points[(front + i) % MAX_POINTS].point.x, points[(front + i) % MAX_POINTS].point.y);
				fprintf(stderr, "%i, %i, %f\n", vel1, vel2, accel);
			}

			if (debug_time) {
				for (i = 1; i < MAX_POINTS; i++)
					fprintf(stderr, "%f, ", diffclock(points[(front + i) % MAX_POINTS].time, points[(front + i - 1) % MAX_POINTS].time));
				fprintf(stderr, "\n");
			}

			if (beatIsReady
					&& (distance(points[(front + MAX_POINTS - 1) % MAX_POINTS].point, points[front].point) > MIN_DISTANCE)
					&& (vel1 < 0)
					&& (vel2 > THRESHOLD)
					&& (remainder(currentBeat, 1.0) != 0.0)) {
				time2 = clock();
				seconds = diffclock(time2, time1);
				BPM = 60.0 / seconds;
				time1 = time2;

				currentBeat += 0.5;
				int note = notes[rand() % NUM_NOTES];
				if (debug_beat) fprintf(stderr, "%f, %i, %f, %f, %f\n", currentBeat, note, accel, seconds, BPM);

				if (note != -1) {
					ramp = midi_to_ramp(note);

					err = Pa_StartStream(stream);
					if (err != paNoError) goto error;

					Pa_Sleep(NUM_MILLISECONDS);

					err = Pa_StopStream(stream);
					if (err != paNoError) goto error;
				}

				beatIsReady = false;
			}

			if (!beatIsReady && (vel1 > 0) && (vel2 < 0)
					&& (remainder(currentBeat, 1.0) == 0.0)) {
				currentBeat += 0.5;
				beatIsReady = true;
			}

			a = draw_depth_hand(cnt, (int)beatIsReady, points);
			cvShowImage(win_hand, a);
			cvResizeWindow(win_hand, WIDTH/2, HEIGHT/2);
		}

		// Press any key to quit.
		if (cvWaitKey(TIMER) != -1)
			break;
	}

	freenect_sync_stop();
	cvDestroyAllWindows();

	err = Pa_CloseStream(stream);
	if (err != paNoError) goto error;
	Pa_Terminate();

	return err;

error:
	Pa_Terminate();
	fprintf(stderr, "An error occured while using the portaudio stream\n");
	fprintf(stderr, "Error number: %d\n", err);
	fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
	return err;
}

//////////////////////////////////////////////////////

double diffclock (clock_t end, clock_t beginning) {
	return (end - beginning) / (double)CLOCKS_PER_SEC;
}

bool is_inside_window (CvPoint p) {
	return (p.x >= 0) && (p.y >= 0) && (p.x < WIDTH) && (p.y < HEIGHT);
}

double distance (CvPoint point1, CvPoint point2) {
	int x = point2.x - point1.x;
	int y = point2.y - point1.y;
	return sqrt((double)((x * x) + (y * y)));
}

double velocity_y (point_t end, point_t beginning) {
	return (end.point.y - beginning.point.y) / diffclock(end.time, beginning.time);
}

double midi_to_freq (int note) {
	return pow(2, (note - 69) / 12.0) * A;
}

float freq_to_ramp (double freq) {
	return freq / SAMPLE_RATE;
}

float midi_to_ramp (int note) {
	return freq_to_ramp(midi_to_freq(note));
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paCallback (const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo* timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData) {
	// Cast data passed through stream to our structure.
	paData *data = (paData*)userData;
	float *out = (float*)outputBuffer;
	unsigned int i;
	(void) inputBuffer; // Prevent unused variable warning.

	float volume = (float)accel / MAX_ACCEL;
	if (volume > 1.0) volume = 1.0;

	for (i = 0; i < framesPerBuffer; i++) {
		*out++ = data->left_phase;
		*out++ = data->right_phase;
		// Generate sawtooth phaser within range (-volume, +volume).
		data->left_phase += ramp * volume;
		// When signal reaches top, drop back down.
		if (data->left_phase >= volume) data->left_phase -= 2.0f * volume;
		// Higher pitch so we can distinguish left and right.
		data->right_phase += 2.0f * ramp * volume;
		if (data->right_phase >= volume) data->right_phase -= 2.0f * volume;
	}
	return 0;
}

IplImage* draw_depth_hand (CvSeq *cnt, int type, point_t points[]) {
	static IplImage *img = NULL;
	CvScalar color[] = {CV_RGB(255,0,0), CV_RGB(0,255,0)};

	if (img == NULL)
		img = cvCreateImage(cvSize(WIDTH, HEIGHT), 8, 3);

	cvZero(img);
	// cvDrawContours(img, cnt, color[type], CV_RGB(0,0,255), 0, CV_FILLED, 8, cvPoint(0,0));

	int i;
	for (i = 1; i < count; i++)
		cvLine(img, points[(front + i - 1) % MAX_POINTS].point, points[(front + i) % MAX_POINTS].point, color[type], 2, 8, 0);

	cvFlip(img, NULL, 1);

	return img;
}

