// File: main.c
// Author: Edward Ly
// Last Modified: 16 November 2016
// Description: A simple virtual conductor application for Kinect for Windows v1.

#include "include.h"

const int SCREENX = 1200, SCREENY = 800;
const int WIN_TYPE = CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE;
const int NUM_MILLISECONDS = 100, SAMPLE_RATE = 44100;
const int WIDTH = 640, HEIGHT = 480, TIMER = 16;
const int MAX_POINTS = 5, MAX_NOTES = 2048, THRESHOLD = 256;
const double MIN_DISTANCE = 12.0, MAX_DISTANCE = 224.0;
const double MAX_ACCEL = 32768.0, BEATS_PER_MEASURE = 4.0;

bool debug_input = false;
bool debug_stream = false;
bool debug_time = false;

double currentBeat = -5.0; // Don't start music immediately.
int currentNote = 0, notesRead = 0, velocity;
clock_t time1, time2;
double seconds, BPM;
double vel1, vel2, accel;

void        parse_notes          (char*, note_t[]);
void        fluid_init           (fluid_settings_t**, fluid_synth_t**, fluid_audio_driver_t**, int*, char*);
double      diffclock            (clock_t, clock_t);
bool        is_inside_window     (CvPoint);
double      distance             (CvPoint, CvPoint);
double      velocity_y           (point_t, point_t);
void        analyze_points       (point_t[], int);
void        calculate_BPM        (clock_t);
void        play_current_notes   (fluid_synth_t*, note_t[]);
IplImage*   draw_depth_hand      (CvSeq*, int, point_t[], int, int);

//////////////////////////////////////////////////////

int main (int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [music] [soundfont]\n", argv[0]);
		exit(-1);
	}

	note_t notes[MAX_NOTES];
	parse_notes(argv[1], notes);

	fluid_settings_t* settings;
	fluid_synth_t* synth;
	fluid_audio_driver_t* adriver;
	int sfont_id;
	fluid_init(&settings, &synth, &adriver, &sfont_id, argv[2]);

	const char *win_hand = "Simple Virtual Conductor";
	point_t points[MAX_POINTS];
	int front = 0, count = 0;
	bool beatIsReady = false;
	time1 = clock(); int n;

	for (n = 0; n < MAX_POINTS; n++) {
		points[n].point.x = WIDTH/2;
		points[n].point.y = HEIGHT/2;
		points[n].time = time1;
	}

	cvNamedWindow(win_hand, WIN_TYPE);
	cvMoveWindow(win_hand, SCREENX - WIDTH/2, HEIGHT/2);
	
	while (true) {
		IplImage *depth, *body, *hand, *a;
		CvSeq *cnt;
		CvPoint cent;
		int z;
		
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

			// Get velocity and acceleration.
			analyze_points(points, front);

			if (beatIsReady
					&& (distance(points[(front + count - 1) % MAX_POINTS].point, points[front].point) > MIN_DISTANCE)
					&& (vel1 < 0) && (vel2 > THRESHOLD)
					&& (remainder(currentBeat, 1.0) != 0.0)) {
				calculate_BPM(points[(front + count - 1) % MAX_POINTS].time);
				currentBeat += 0.5;
				play_current_notes(synth, notes);
				beatIsReady = false;
			}

			if (!beatIsReady && (vel1 > 0) && (vel2 < 0)
					&& (remainder(currentBeat, 1.0) == 0.0)) {
				currentBeat += 0.5;
				play_current_notes(synth, notes);
				beatIsReady = true;
			}

			a = draw_depth_hand(cnt, (int)beatIsReady, points, front, count);
			cvShowImage(win_hand, a);
			cvResizeWindow(win_hand, WIDTH/2, HEIGHT/2);
		}

		// Press any key to quit.
		if (cvWaitKey(TIMER) != -1) break;
	}

	freenect_sync_stop();
	cvDestroyAllWindows();

	delete_fluid_audio_driver(adriver);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);

	return 0;
}

//////////////////////////////////////////////////////

void parse_notes (char* filename, note_t notes[]) {
	FILE* file;
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: unable to open file %s\n", filename);
		exit(-2);
	}

	int n = 0;
	while (fscanf(file, "%lf, %i, %i, %i", &notes[n].beat, &notes[n].channel, &notes[n].key, &notes[n].noteOn) != EOF) {
		if (debug_input)
			fprintf(stderr, "%1lf, %i, %i, %i\n", notes[n].beat, notes[n].channel, notes[n].key, notes[n].noteOn);
		if (++n >= MAX_NOTES) {
			fprintf(stderr, "Warning: maximum number of notes reached, music will end after beat %1lf\n", notes[MAX_NOTES - 1].beat);
			break;
		}
	}
	notesRead = n;
	fclose(file);
}

void fluid_init (fluid_settings_t** settings, fluid_synth_t** synth, fluid_audio_driver_t** adriver, int* sfont_id, char* soundfont) {
	*settings = new_fluid_settings();
	if (*settings == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the settings\n");
		exit(1);
	}
	*synth = new_fluid_synth(*settings);
	if (*synth == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the synthesizer\n");
		delete_fluid_settings(*settings);
		exit(2);
	}
	*adriver = new_fluid_audio_driver(*settings, *synth);
	if (*adriver == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the audio driver\n");
		delete_fluid_synth(*synth);
		delete_fluid_settings(*settings);
		exit(3);
	}
	*sfont_id = fluid_synth_sfload(*synth, soundfont, 1);
	if (*sfont_id == FLUID_FAILED) {
		fprintf(stderr, "FluidSynth: unable to open soundfont %s\n", soundfont);
		delete_fluid_audio_driver(*adriver);
		delete_fluid_synth(*synth);
		delete_fluid_settings(*settings);
		exit(4);
	}
}

double diffclock (clock_t end, clock_t beginning) {
	return (end - beginning) / (double)CLOCKS_PER_SEC;
}

bool is_inside_window (CvPoint p) {
	return (p.x >= 0) && (p.y >= 0) && (p.x < WIDTH) && (p.y < HEIGHT);
}

double distance (CvPoint p1, CvPoint p2) {
	int x = p2.x - p1.x;
	int y = p2.y - p1.y;
	return sqrt((double)((x * x) + (y * y)));
}

double velocity_y (point_t end, point_t beginning) {
	return (end.point.y - beginning.point.y) / diffclock(end.time, beginning.time);
}

void analyze_points (point_t points[], int front) {
	vel2 = -1 * velocity_y(points[(front + MAX_POINTS - 1) % MAX_POINTS], points[(front + MAX_POINTS - 3) % MAX_POINTS]);
	vel1 = -1 * velocity_y(points[(front + 2) % MAX_POINTS], points[front]);
	accel = abs(vel2 - vel1) / diffclock(points[(front + MAX_POINTS - 1) % MAX_POINTS].time, points[(front + 2) % MAX_POINTS].time);

	if (debug_stream) {
		int i;
		for (i = 0; i < MAX_POINTS; i++)
			fprintf(stderr, "(%i, %i), ", points[(front + i) % MAX_POINTS].point.x, points[(front + i) % MAX_POINTS].point.y);
		fprintf(stderr, "%lf, %lf, %lf\n", vel1, vel2, accel);
	}

	if (debug_time) {
		int i;
		for (i = 1; i < MAX_POINTS; i++)
			fprintf(stderr, "%lf, ", diffclock(points[(front + i) % MAX_POINTS].time, points[(front + i - 1) % MAX_POINTS].time));
		fprintf(stderr, "\n");
	}
}

void calculate_BPM (clock_t end) {
	time2 = end;
	seconds = diffclock(time2, time1);
	BPM = 60.0 / seconds;
	time1 = time2;
}

void play_current_notes (fluid_synth_t* synth, note_t notes[]) {
	velocity = (int)(accel * 127.0 / MAX_ACCEL);
	if (velocity > 127) velocity = 127;

	while ((currentNote < notesRead)
			&& (notes[currentNote].beat <= currentBeat)) {
		if (notes[currentNote].noteOn)
			fluid_synth_noteon(synth, notes[currentNote].channel, notes[currentNote].key, velocity);
		else fluid_synth_noteoff(synth, notes[currentNote].channel, notes[currentNote].key);
		currentNote++;
	}

	if (currentNote >= notesRead) {
		// Reset music.
		currentNote = 0;
		currentBeat = -5.0;
	}
}

IplImage* draw_depth_hand (CvSeq *cnt, int type, point_t points[], int front, int count) {
	static IplImage *img = NULL;
	CvScalar color[] = {CV_RGB(255, 0, 0), CV_RGB(0, 255, 0)};

	if (img == NULL)
		img = cvCreateImage(cvSize(WIDTH, HEIGHT), 8, 3);

	cvZero(img);
	// cvDrawContours(img, cnt, color[type], CV_RGB(0, 0, 255), 0, CV_FILLED, 8, cvPoint(0, 0));

	int i;
	for (i = 1; i < count; i++)
		cvLine(img, points[(front + i - 1) % MAX_POINTS].point, points[(front + i) % MAX_POINTS].point, color[type], 2, 8, 0);

	cvFlip(img, NULL, 1);

	return img;
}

