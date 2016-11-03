/*  
 * XKin copyright (c) 2012, Fabrizio Pedersoli
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

bool debug_stream = true;
bool update = true;
char *infile = NULL;
const int MAX_POINTS = 4;

IplImage*      draw_depth_hand         (CvSeq*, int, CvPoint[], int, int);
int            buffered_classfy        (int);
void           draw_trajectory         (IplImage*, CvSeq*);
void           parse_args              (int, char**);
void           usage                   (void);


int main (int argc, char *argv[])
{
	CvHMM *models;
	int num;
	ptseq seq;
	const char *win_hand = "depth hand";

	CvPoint points[MAX_POINTS];
	int front = 0, count = 0, accel;

	parse_args(argc,argv);

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

		accel = (points[(front + 3) % MAX_POINTS].y - points[(front + 2) % MAX_POINTS].y) - (points[(front + 1) % MAX_POINTS].y - points[front].y);

		if (debug_stream) {
			for (i = 0; i < MAX_POINTS; i++) {
				fprintf(stderr, "(%i, %i), ", points[(front + i) % MAX_POINTS].x, points[(front + i) % MAX_POINTS].y);
			}
			fprintf(stderr, "%i\n", accel);
		}

		if ((p = basic_posture_classification(cnt)) == -1)
			continue;

		if (cvhmm_get_gesture_sequence(p, cent, &seq)) {
			
			int g = cvhmm_classify_gesture(models, num, seq, 0); g++;

			switch (g) {
			case 0:
				fprintf(stderr, "Recognized beat 1\n\n");
				break;
			case 1:
				fprintf(stderr, "Recognized beat 2\n\n");
				break;
			}

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

