// File: main.c
// Author: Edward Ly
// Last Modified: 6 November 2016
// Description:

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

#define SCREENX 1200
#define SCREENY 800
#define F 1.25
#define WIN_TYPE CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE

#define NUM_MILLISECONDS   (50)
#define SAMPLE_RATE   (44100)

enum {
	W=640,
	H=480,
	GW=128,
	GH=128,
	T=10,
	MAX_STR_LEN=50,
	NUM=3,
	UP=0,
	DOWN=1,
	LEFT=3,
	RIGHT=2,
};

typedef struct paData {
    float left_phase;
    float right_phase;
} paData;

bool debug_stream = true;
bool update = true;
char *infile = NULL;
const int MAX_POINTS = 5;
static paData data;

IplImage*      draw_depth_hand         (CvSeq*, int, CvPoint[], int, int);
int            buffered_classfy        (int);
void           draw_trajectory         (IplImage*, CvSeq*);
void           parse_args              (int, char**);
void           usage                   (void);
static int     paCallback              (const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);


int main (int argc, char *argv[])
{
	CvHMM *models;
	int num;
	ptseq seq;
	const char *win_hand = "depth hand";

	CvPoint points[MAX_POINTS];
	int front = 0, count = 0, vel1, vel2, accel;
	// bool beatIsReady = true;

	PaStream *stream;
	PaError err;
	data.left_phase = data.right_phase = 0.0;

	parse_args(argc, argv);

	err = Pa_Initialize();
	if (err != paNoError) goto error;

    err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, paCallback, &data);
    if (err != paNoError) goto error;

	seq = ptseq_init();
	models = cvhmm_read(infile, &num);

	cvNamedWindow(win_hand, WIN_TYPE);
	cvMoveWindow(win_hand, SCREENX-W/2, H/2);
	
	while (true) {
		IplImage *depth, *body, *hand;
		IplImage *a;
		CvSeq *cnt;
		CvPoint cent;
		int z, p, i;
		
		depth = freenect_sync_get_depth_cv(0);
		
		body = body_detection(depth);
		hand = hand_detection(body, &z);

		if (!get_hand_contour_basic(hand, &cnt, &cent))
			continue;

		if (count < MAX_POINTS) {
			points[(front + count) % MAX_POINTS] = cent;
			count++;
		}
		else {
			points[front] = cent;
			front = (front + 1) % MAX_POINTS;
		}

		vel2 = points[(front + MAX_POINTS - 1) % MAX_POINTS].y - points[(front + MAX_POINTS - 2) % MAX_POINTS].y;
		vel1 = points[(front + MAX_POINTS - 3) % MAX_POINTS].y - points[(front + MAX_POINTS - 4) % MAX_POINTS].y;
		accel = vel2 - vel1;

		if (debug_stream) {
			for (i = 0; i < MAX_POINTS; i++) {
				fprintf(stderr, "(%i, %i), ", points[(front + i) % MAX_POINTS].x, points[(front + i) % MAX_POINTS].y);
			}
			fprintf(stderr, "%i, %i, %i\n", vel1, vel2, accel);
		}

		if ((p = basic_posture_classification(cnt)) == -1)
			continue;

		if (cvhmm_get_gesture_sequence(p, cent, &seq)) {
			int g = cvhmm_classify_gesture(models, num, seq, 0);

			switch (g) {
			case 0:
				fprintf(stderr, "Recognized beat 1\n\n");
				break;
			case 1:
				fprintf(stderr, "Recognized beat 2\n\n");
				break;
			}

			err = Pa_StartStream(stream);
			if (err != paNoError) goto error;

			Pa_Sleep(NUM_MILLISECONDS);

			err = Pa_StopStream(stream);
			if (err != paNoError) goto error;

			update = true;
		} else {
			update = false;
		}

		a = draw_depth_hand(cnt, p, points, front, count);
		cvShowImage(win_hand, a);
		cvResizeWindow(win_hand, W/2, H/2);

		// Press any key to quit.
		if (cvWaitKey(T) != -1)
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
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	return err;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paCallback( const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo* timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData ) {
	// Cast data passed through stream to our structure.
	paData *data = (paData*)userData;
	float *out = (float*)outputBuffer;
	unsigned int i;
	(void) inputBuffer; // Prevent unused variable warning.

	for (i = 0; i < framesPerBuffer; i++) {
		*out++ = data->left_phase;
		*out++ = data->right_phase;
		// Generate simple sawtooth phaser that ranges between -1.0 and 1.0.
		data->left_phase += 0.01f;
		// When signal reaches top, drop back down.
		if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;
		// Higher pitch so we can distinguish left and right.
		data->right_phase += 0.03f;
		if( data->right_phase >= 1.0f ) data->right_phase -= 2.0f;
	}
	return 0;
}

IplImage* draw_depth_hand (CvSeq *cnt, int type, CvPoint points[], int front, int count)
{
	static IplImage *img = NULL;
	CvScalar color[] = {CV_RGB(255,0,0), CV_RGB(0,255,0)};

	if (img == NULL)
		img = cvCreateImage(cvSize(W, H), 8, 3);

	cvZero(img);
	// cvDrawContours(img, cnt, color[type], CV_RGB(0,0,255), 0, CV_FILLED, 8, cvPoint(0,0));

	int i;
	for (i = 1; i < count; i++)
		cvLine(img, points[(front + i - 1) % MAX_POINTS], points[(front + i) % MAX_POINTS], color[type], 2, 8, 0);

	cvFlip(img, NULL, 1);

	return img;
}

void parse_args (int argc, char **argv)
{
	int c;

	opterr=0;
	while ((c = getopt(argc,argv,"i:h")) != -1) {
		switch (c) {
		case 'i':
			infile = optarg;
			break;
		case 'h':
		default:
			usage();
			exit(-1);
		}
	}
	if (infile == NULL) {
		usage();
		exit(-1);
	}
}

void usage (void)
{
	printf("usage: ./main -i [file] [-h]\n");
	printf("  -i  gestures models yml file\n");
	printf("  -h  show this message\n");
}

