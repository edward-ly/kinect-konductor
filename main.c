// File: main.c
// Author: Edward Ly
// Last Modified: 22 November 2016
// Description: A simple virtual conductor application for Kinect for Windows v1.
// See the LICENSE file for license information.

#include "include.h"

const int SCREENX = 1200, SCREENY = 800;
const int WIN_TYPE = CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE;
const int WIDTH = 640, HEIGHT = 480, TIMER = 1;
const int MAX_CHANNELS = 16, MAX_BEATS = 4;
const int MAX_POINTS = 5, THRESHOLD = 64;
const double MIN_DISTANCE = 12.0, MAX_ACCEL = 16384.0;

bool debug_input = false;
bool debug_stream = false;
bool debug_time = false;

int currentBeat = -5; // Don't start music immediately.
int currentNote = 0, programCount, noteCount;
unsigned short velocity;
unsigned int PPQN, ticksPerBeat, time1, time2;
double vel1, vel2, accel;

fluid_settings_t* settings;
fluid_synth_t* synth;
fluid_audio_driver_t* adriver;
fluid_sequencer_t* sequencer;
int sfont_id;
short synthSeqID, mySeqID;
unsigned int now;
double ticksPerSecond;

void      fluid_init         (char*, int[]);
double    diffclock          (unsigned int, unsigned int);
double    distance           (CvPoint, CvPoint);
double    velocity_y         (point_t, point_t);
void      analyze_points     (point_t[], int);
void      send_note_on       (int, int, unsigned short, unsigned int);
void      send_note_off      (int, int, unsigned int);
void      play_current_notes (fluid_synth_t*, note_t[]);
IplImage* draw_depth_hand    (CvSeq*, int, point_t[], int, int);

//////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [music] [soundfont]\n", argv[0]);
		exit(-1);
	}

	FILE* file; // Start opening music file and parse input.
	char* filename = argv[1];
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: unable to open file %s\n", filename);
		exit(-2);
	}

	if (fscanf(file, "%i %i %u", &programCount, &noteCount, &PPQN) == EOF) {
		fclose(file);
		fprintf(stderr, "Error: unable to read counts from file %s\n", filename);
		exit(-3);
	}

	int channel, program, i;
	int programs[MAX_CHANNELS];
	// Initialize program changes to none.
	for (i = 0; i < MAX_CHANNELS; i++)
		programs[i] = -1;

	// Get program changes.
	for (i = 0; i < programCount; i++) {
		if (fscanf(file, "%i %i", &channel, &program) == EOF) {
			fclose(file);
			fprintf(stderr, "Error: unable to read program change from file %s\n", filename);
			exit(-4);
		}
		else if ((channel < 0) || (channel >= MAX_CHANNELS)) {
			fclose(file);
			fprintf(stderr, "Error: invalid channel number %i read from file %s\n", channel, filename);
			exit(-5);
		}
		else {
			programs[channel] = program;
			if (debug_input)
				fprintf(stderr, "%i, %i\n", channel, program);
		}
	}

	// Now initialize array of note messages and get messages.
	note_t notes[noteCount];
	for (i = 0; i < noteCount; i++) {
		if (fscanf(file, "%i %u %i %hi %i", &notes[i].beat, &notes[i].tick, &notes[i].channel, &notes[i].key, &notes[i].noteOn) == EOF) {
			fclose(file);
			fprintf(stderr, "Error: invalid note message %i of %i read from file %s\n", i + 1, noteCount, filename);
			exit(-6);
		}
		else if (debug_input)
			fprintf(stderr, "%i, %i, %i, %i, %i\n", notes[i].beat, notes[i].tick, notes[i].channel, notes[i].key, notes[i].noteOn);
	}

	fclose(file);

	fluid_init(argv[2], programs); // Initialize FluidSynth.

	const char* win_hand = "Kinect Konductor";
	point_t points[MAX_POINTS]; // Queue of last known hand positions.
	int pointsFront = 0, pointsCount = 0;
	int clockTicks[MAX_BEATS]; // Queue of synth ticks elapsed b/t beats.
	int clockTicksFront = 0, clockTicksCount = 0;
	bool beatIsReady = false;
	time1 = fluid_sequencer_get_tick(sequencer);

	// Initialize queues to prevent erratic results.
	for (i = 0; i < MAX_POINTS; i++) {
		points[i].point.x = WIDTH/2;
		points[i].point.y = HEIGHT/2;
		points[i].time = time1;
	}
	for (i = 0; i < MAX_BEATS; i++)
		clockTicks[i] = 0;

	cvNamedWindow(win_hand, WIN_TYPE);
	cvMoveWindow(win_hand, SCREENX - WIDTH/2, HEIGHT/2);

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
		if (pointsCount < MAX_POINTS) {
			points[(pointsFront + pointsCount) % MAX_POINTS].time = fluid_sequencer_get_tick(sequencer);
			points[(pointsFront + pointsCount++) % MAX_POINTS].point = cent;
		}
		else {
			points[pointsFront].time = fluid_sequencer_get_tick(sequencer);
			points[pointsFront++].point = cent;
			pointsFront %= MAX_POINTS;
		}

		// Update velocity and acceleration.
		analyze_points(points, pointsFront);

		if (beatIsReady
				&& (distance(points[(pointsFront + pointsCount - 1) % MAX_POINTS].point, points[pointsFront].point) > MIN_DISTANCE)
				&& (vel1 < 0) && (vel2 > THRESHOLD)) {
			// Add elapsed clock ticks to queue.
			time2 = points[(pointsFront + pointsCount - 1) % MAX_POINTS].time;
			if (clockTicksCount < MAX_BEATS)
				clockTicks[(clockTicksFront + clockTicksCount++) % MAX_BEATS] = time2 - time1;
			else {
				clockTicks[clockTicksFront++] = time2 - time1;
				clockTicksFront %= MAX_BEATS;
			}
			time1 = time2;

			ticksPerBeat = 0;
			for (i = 0; i < clockTicksCount; i++)
				ticksPerBeat += clockTicks[(clockTicksFront + i) % MAX_BEATS];
			ticksPerBeat /= clockTicksCount;

			currentBeat++;
			play_current_notes(synth, notes);
			beatIsReady = false;
		}

		if (!beatIsReady && (vel1 > 0) && (vel2 < 0))
			beatIsReady = true;

		a = draw_depth_hand(cnt, (int)beatIsReady, points, pointsFront, pointsCount);
		cvShowImage(win_hand, a);
		cvResizeWindow(win_hand, WIDTH/2, HEIGHT/2);

		// Press any key to quit.
		if (cvWaitKey(TIMER) != -1) break;
	}

	freenect_sync_stop();
	cvDestroyAllWindows();

	delete_fluid_sequencer(sequencer);
	delete_fluid_audio_driver(adriver);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);

	return 0;
}

//////////////////////////////////////////////////////

void fluid_init (char* soundfont, int programs[]) {
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

	sfont_id = fluid_synth_sfload(synth, soundfont, 1);
	if (sfont_id == FLUID_FAILED) {
		fprintf(stderr, "FluidSynth: unable to open soundfont %s\n", soundfont);
		delete_fluid_audio_driver(adriver);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		exit(4);
	}

	// Set instruments / make program changes.
	int i;
	for (i = 0; i < MAX_CHANNELS; i++) {
		if (programs[i] != -1)
			fluid_synth_program_select(synth, i, sfont_id, 0, programs[i]);
	}

	sequencer = new_fluid_sequencer2(0);
	synthSeqID = fluid_sequencer_register_fluidsynth(sequencer, synth);
	mySeqID = fluid_sequencer_register_client(sequencer, "me", NULL, NULL);
	ticksPerSecond = fluid_sequencer_get_time_scale(sequencer);
}

double diffclock (unsigned int end, unsigned int beginning) {
	return (end - beginning) / (double)ticksPerSecond;
}

double distance (CvPoint p1, CvPoint p2) {
	int x = p2.x - p1.x;
	int y = p2.y - p1.y;
	return sqrt((double)((x * x) + (y * y)));
}

double velocity_y (point_t end, point_t beginning) {
	return (end.point.y - beginning.point.y) / diffclock(end.time, beginning.time);
}

void analyze_points (point_t points[], int pointsFront) {
	vel2 = -1 * velocity_y(points[(pointsFront + MAX_POINTS - 1) % MAX_POINTS], points[(pointsFront + MAX_POINTS - 3) % MAX_POINTS]);
	vel1 = -1 * velocity_y(points[(pointsFront + 2) % MAX_POINTS], points[pointsFront]);
	accel = abs(vel2 - vel1) / diffclock(points[(pointsFront + MAX_POINTS - 1) % MAX_POINTS].time, points[(pointsFront + 2) % MAX_POINTS].time);

	if (debug_stream) {
		int i;
		for (i = 0; i < MAX_POINTS; i++)
			fprintf(stderr, "(%i, %i), ", points[(pointsFront + i) % MAX_POINTS].point.x, points[(pointsFront + i) % MAX_POINTS].point.y);
		fprintf(stderr, "%lf, %lf, %lf\n", vel1, vel2, accel);
	}

	if (debug_time) {
		int i;
		for (i = 1; i < MAX_POINTS; i++)
			fprintf(stderr, "%lf, ", diffclock(points[(pointsFront + i) % MAX_POINTS].time, points[(pointsFront + i - 1) % MAX_POINTS].time));
		fprintf(stderr, "\n");
	}
}

void send_note_on (int chan, int key, unsigned short vel, unsigned int date) {
    fluid_event_t *evt = new_fluid_event();
    fluid_event_set_source(evt, -1);
    fluid_event_set_dest(evt, synthSeqID);
    fluid_event_noteon(evt, chan, key, vel);
    fluid_sequencer_send_at(sequencer, evt, date, 1);
    delete_fluid_event(evt);
}

void send_note_off (int chan, int key, unsigned int date) {
    fluid_event_t *evt = new_fluid_event();
    fluid_event_set_source(evt, -1);
    fluid_event_set_dest(evt, synthSeqID);
    fluid_event_noteoff(evt, chan, key);
    fluid_sequencer_send_at(sequencer, evt, date, 1);
    delete_fluid_event(evt);
}

void play_current_notes (fluid_synth_t* synth, note_t notes[]) {
	velocity = (unsigned short)(accel * 127.0 / MAX_ACCEL);
	if (velocity > 127) velocity = 127;

	now = fluid_sequencer_get_tick(sequencer);

	while ((currentNote < noteCount)
			&& (notes[currentNote].beat <= currentBeat)) {
		unsigned int at_tick = now + (ticksPerBeat * notes[currentNote].tick / PPQN);

		if (notes[currentNote].noteOn)
			send_note_on(notes[currentNote].channel, notes[currentNote].key, velocity, at_tick);
		else send_note_off(notes[currentNote].channel, notes[currentNote].key, at_tick);
		currentNote++;
	}

	if (currentNote >= noteCount) {
		// End of music reached, reset music.
		currentNote = 0;
		currentBeat = -5;
	}
}

IplImage* draw_depth_hand (CvSeq *cnt, int type, point_t points[], int pointsFront, int pointsCount) {
	static IplImage *img = NULL;
	CvScalar color[] = {CV_RGB(255, 0, 0), CV_RGB(0, 255, 0)};

	if (img == NULL) img = cvCreateImage(cvSize(WIDTH, HEIGHT), 8, 3);

	cvZero(img);
	// cvDrawContours(img, cnt, color[type], CV_RGB(0, 0, 255), 0, CV_FILLED, 8, cvPoint(0, 0));

	int i;
	for (i = 1; i < pointsCount; i++)
		cvLine(img, points[(pointsFront + i - 1) % MAX_POINTS].point, points[(pointsFront + i) % MAX_POINTS].point, color[type], 2, 8, 0);

	cvFlip(img, NULL, 1);

	return img;
}

