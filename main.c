// File: main.c
// Author: Edward Ly
// Last Modified: 30 November 2016
// Description: A simple virtual conductor application for Kinect for Windows v1.
// See the LICENSE file for license information.

#include "include.h"

const int    WIN_TYPE     = CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE,
             SCREEN_X     = 1200,
             SCREEN_Y     = 800,
             WIDTH        = 640,
             HEIGHT       = 480,
             TIMER        = 1,
             MAX_CHANNELS = 16,
             MAX_BEATS    = 4,
             MAX_POINTS   = 5,
             THRESHOLD    = 8;
const double MIN_DISTANCE = 8.0,
             MAX_ACCEL    = 16384.0;
const char*  win_hand     = "Kinect Konductor";

char *music = NULL, *font = NULL;

int currentBeat = -5; // Don't start music immediately.
int currentNote = 0, programCount, noteCount;
unsigned short velocity;
unsigned int PPQN, ticksPerBeat, time1, time2;
double vel1, vel2, accel;
int* programs;
note_t* notes;

fluid_settings_t* settings;
fluid_synth_t* synth;
fluid_audio_driver_t* adriver;
fluid_sequencer_t* sequencer;
int sfont_id;
short synthSeqID, mySeqID;
unsigned int now;
double ticksPerSecond;

void      parse_args         (int, char*[]);
void      parse_music        (void);
void      fluid_init         (void);
void      fluid_set_programs (void);
void      main_loop          (void);
double    diffclock          (unsigned int, unsigned int);
double    distance           (CvPoint, CvPoint);
double    velocity_y         (point_t, point_t);
void      analyze_points     (point_t[], int);
void      send_note          (int, int, unsigned short, unsigned int, int);
void      play_current_notes (void);
IplImage* draw_depth_hand    (CvSeq*, int, point_t[], int, int);
void      release_all        (void);
void      usage              (char*);

//////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
	parse_args(argc, argv);
	parse_music();
	fluid_init();

	main_loop();

	release_all();
	return 0;
}

//////////////////////////////////////////////////////

void parse_args (int argc, char* argv[]) {
	int c;
	while ((c = getopt(argc, argv, "m:f:h")) != -1) {
		switch (c) {
		case 'm':
			music = optarg;
			break;
		case 'f':
			font = optarg;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(-1);
		}
	}

	if (music == NULL || font == NULL) {
		usage(argv[0]);
		exit(-1);
	}
}

void parse_music (void) {
	FILE* file = fopen(music, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: unable to open file %s\n", music);
		exit(-2);
	}

	if (fscanf(file, "%i %i %u", &programCount, &noteCount, &PPQN) == EOF) {
		fclose(file);
		fprintf(stderr, "Error: unable to read counts from file %s\n", music);
		exit(-3);
	}

	int channel, program, i;
	// Malloc program array and initialize changes to none.
	programs = (int*)malloc(sizeof(int) * MAX_CHANNELS);
	for (i = 0; i < MAX_CHANNELS; i++)
		programs[i] = -1;

	// Get program changes.
	for (i = 0; i < programCount; i++) {
		if (fscanf(file, "%i %i", &channel, &program) == EOF) {
			fclose(file);
			fprintf(stderr, "Error: unable to read program change from file %s\n", music);
			exit(-4);
		}
		else if (channel < 0 || channel >= MAX_CHANNELS) {
			fclose(file);
			fprintf(stderr, "Error: invalid channel number %i read from file %s\n", channel, music);
			exit(-5);
		}
		else programs[channel] = program;
	}

	// Now malloc array of note messages and get messages.
	notes = (note_t*)malloc(sizeof(note_t) * noteCount);
	for (i = 0; i < noteCount; i++) {
		if (fscanf(file, "%i %u %i %hi %i",
                         &notes[i].beat,
                         &notes[i].tick,
                         &notes[i].channel,
                         &notes[i].key,
                         &notes[i].noteOn) == EOF) {
			fclose(file);
			fprintf(stderr, "Error: invalid note message %i of %i read from file %s\n", i + 1, noteCount, music);
			exit(-6);
		}
	}

	fclose(file);
}

void fluid_init (void) {
	settings = new_fluid_settings();
	if (settings == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the settings\n");
		exit(1);
	}

	// Automatically connect FluidSynth to system output if using JACK.
	char* device;
	fluid_settings_getstr(settings, "audio.driver", &device);
	if (strcmp(device, "jack") == 0)
		fluid_settings_setint(settings, "audio.jack.autoconnect", 1);

	synth = new_fluid_synth(settings);
	if (synth == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the synthesizer\n");
		delete_fluid_settings(settings);
		exit(2);
	}

	adriver = new_fluid_audio_driver(settings, synth);
	if (adriver == NULL) {
		fprintf(stderr, "FluidSynth: failed to create the audio driver\n");
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		exit(3);
	}

	sfont_id = fluid_synth_sfload(synth, font, 1);
	if (sfont_id == FLUID_FAILED) {
		fprintf(stderr, "FluidSynth: unable to open soundfont %s\n", font);
		delete_fluid_audio_driver(adriver);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		exit(4);
	}

	fluid_set_programs();

	sequencer = new_fluid_sequencer2(0);
	synthSeqID = fluid_sequencer_register_fluidsynth(sequencer, synth);
	mySeqID = fluid_sequencer_register_client(sequencer, "me", NULL, NULL);
	ticksPerSecond = fluid_sequencer_get_time_scale(sequencer);
}

void fluid_set_programs (void) {
	int i;
	for (i = 0; i < MAX_CHANNELS; i++) {
		if (programs[i] != -1)
			fluid_synth_program_select(synth, i, sfont_id, 0, programs[i]);
	}
}

void main_loop (void) {
	point_t points[MAX_POINTS]; // Queue of last known hand positions.
	int ticks[MAX_BEATS]; // Queue of synth ticks elapsed b/t beats.
	int p_front = 0, p_count = 0, c_front = 0, c_count = 0, i;
	bool beatIsReady = false;
	time1 = fluid_sequencer_get_tick(sequencer);

	// Initialize queues to prevent erratic results.
	for (i = 0; i < MAX_POINTS; i++) {
		points[i].point.x = WIDTH / 2;
		points[i].point.y = HEIGHT / 2;
		points[i].time = time1;
	}
	for (i = 0; i < MAX_BEATS; i++)
		ticks[i] = 0;

	cvNamedWindow(win_hand, WIN_TYPE);
	cvMoveWindow(win_hand, SCREEN_X - WIDTH / 2, 0);

	// Main loop: detect and watch hand for beats.
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

		// Add point to queue.
		now = fluid_sequencer_get_tick(sequencer);
		if (p_count < MAX_POINTS) {
			points[(p_front + p_count) % MAX_POINTS].time = now;
			points[(p_front + p_count++) % MAX_POINTS].point = cent;
		}
		else {
			points[p_front].time = now;
			points[p_front++].point = cent;
			p_front %= MAX_POINTS;
		}

		// Update velocity and acceleration.
		analyze_points(points, p_front);

		CvPoint prev = points[(p_front + p_count - 2) % MAX_POINTS].point;
		CvPoint last = points[p_front].point;
		if (beatIsReady && (vel1 < 0) && (vel2 > THRESHOLD)
				&& (prev.y - last.y) > MIN_DISTANCE) {
			// Add elapsed clock ticks to queue.
			time2 = points[(p_front + p_count - 1) % MAX_POINTS].time;
			if (c_count < MAX_BEATS)
				ticks[(c_front + c_count++) % MAX_BEATS] = time2 - time1;
			else {
				ticks[c_front++] = time2 - time1;
				c_front %= MAX_BEATS;
			}
			time1 = time2;

			ticksPerBeat = 0;
			for (i = 0; i < c_count; i++)
				ticksPerBeat += ticks[(c_front + i) % MAX_BEATS];
			ticksPerBeat /= c_count;

			currentBeat++;
			play_current_notes();
			beatIsReady = false;
		}

		if (!beatIsReady && (vel1 > 0) && (vel2 < 0))
			beatIsReady = true;

		a = draw_depth_hand(cnt, (int)beatIsReady, points, p_front, p_count);
		cvShowImage(win_hand, a);
		cvResizeWindow(win_hand, WIDTH / 2, HEIGHT / 2);

		// Press any key to quit.
		if (cvWaitKey(TIMER) != -1) break;
	}
}

double diffclock (unsigned int end, unsigned int start) {
	return (end - start) / (double)ticksPerSecond;
}

double distance (CvPoint p1, CvPoint p2) {
	int x = p2.x - p1.x;
	int y = p2.y - p1.y;
	return sqrt((double)((x * x) + (y * y)));
}

double velocity_y (point_t end, point_t start) {
	return (end.point.y - start.point.y) / diffclock(end.time, start.time);
}

void analyze_points (point_t points[], int front) {
	point_t point4 = points[(front + MAX_POINTS - 1) % MAX_POINTS];
	point_t point3 = points[(front + MAX_POINTS - 3) % MAX_POINTS];
	point_t point2 = points[(front + 2) % MAX_POINTS];
	point_t point1 = points[front];

	vel2 = -1 * velocity_y(point4, point3);
	vel1 = -1 * velocity_y(point2, point1);
	accel = abs(vel2 - vel1) / diffclock(point4.time, point2.time);
}

void send_note (int chan, int key, unsigned short vel, unsigned int date, int on) {
	fluid_event_t* evt = new_fluid_event();
	fluid_event_set_source(evt, -1);
	fluid_event_set_dest(evt, synthSeqID);
	if (on) fluid_event_noteon(evt, chan, key, vel);
	else fluid_event_noteoff(evt, chan, key);
	fluid_sequencer_send_at(sequencer, evt, date, 1);
	delete_fluid_event(evt);
}

void play_current_notes (void) {
	velocity = (unsigned short)(accel * 127.0 / MAX_ACCEL);
	if (velocity > 127) velocity = 127;
	if (velocity < 15) velocity = 15;

	now = fluid_sequencer_get_tick(sequencer);

	while ((currentNote < noteCount) && (notes[currentNote].beat <= currentBeat)) {
		int channel = notes[currentNote].channel;
		short key = notes[currentNote].key;
		unsigned int at_tick = now;
		at_tick += ticksPerBeat * notes[currentNote].tick / PPQN;
		bool noteOn = notes[currentNote].noteOn;

		send_note(channel, key, velocity, at_tick, noteOn);
		currentNote++;
	}

	if ((currentNote >= noteCount) && (notes[noteCount - 1].beat < currentBeat)) {
		// End of music reached, reset music.
		// Turn off any stray notes, redo program changes.
		fluid_synth_system_reset(synth);
		fluid_set_programs();
		currentNote = 0;
		currentBeat = -4;
	}
}

IplImage* draw_depth_hand (CvSeq *cnt, int type, point_t pt[], int fr, int ct) {
	static IplImage *img = NULL; int i;
	CvScalar color[] = {CV_RGB(255, 0, 0), CV_RGB(0, 255, 0), CV_RGB(0, 0, 255)};

	if (img == NULL) img = cvCreateImage(cvSize(WIDTH, HEIGHT), 8, 3);

	cvZero(img);
	cvDrawContours(img, cnt, color[type], color[2], 0, 1, 8, cvPoint(0, 0));
	for (i = 1; i < ct; i++) {
		CvPoint point1 = pt[(fr + i - 1) % MAX_POINTS].point;
		CvPoint point2 = pt[(fr + i) % MAX_POINTS].point;
		cvLine(img, point1, point2, color[type], 2, 8, 0);
	}
	cvFlip(img, NULL, 1);

	return img;
}

void release_all (void) {
	free(notes);
	free(programs);
	freenect_sync_stop();
	cvDestroyAllWindows();

	delete_fluid_sequencer(sequencer);
	delete_fluid_audio_driver(adriver);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);
}

void usage (char* program) {
	fprintf(stderr, "Usage: %s -m [music] -f [font] [-h]\n", program);
	fprintf(stderr, "  -m  Music CSV file\n");
	fprintf(stderr, "  -f  SoundFont file\n");
	fprintf(stderr, "  -h  Show this message\n");
}

