# Kinect Konductor

A virtual orchestra conductor application using the Microsoft Kinect for Windows v1. Developed as an Earlham College senior capstone research project in collaboration with the Music and Computer Science Departments.

This application tracks your hand's position and uses its vertical velocity and acceleration to detect beats and determine volume, respectively. A MIDI-like CSV file is used to store the desired music for playback. For more information, you can read the accompanying paper found in the `pdf` folder.

### [Demonstration Video](https://youtu.be/bJLTbwUDpNs)

## Technical Overview

This application was developed using the following libraries. Some notable dependencies are also listed here.

* [libfreenect](https://openkinect.org/wiki/Main_Page)
* [XKin](https://github.com/fpeder/XKin)
  * [OpenCV](http://opencv.org/)
  * [FFTW](http://fftw.org/)
* [FluidSynth](http://www.fluidsynth.org/)
* [JACK](http://jackaudio.org/)

## Setup

### Linux

>Ensure that all the required libraries listed above (except XKin, see below) are installed and configured for your computer. If you are configuring libfreenect with `cmake`, be sure to use the option `-DBUILD_CV=ON`.

1. Clone this repository and `cd` to the source directory.
2. Create a new folder called `XKin` and clone the [XKin library repository](https://github.com/fpeder/XKin) into that folder. Follow the instructions provided to compile it.
3. Return to the source directory and run `make`.

## Running the Program

### Linux

The command to run the program is `konductor -m [music] -f [font]`, where `[music]` is the CSV file containing the score to be played, and `[font]` is the SoundFont file containing the desired MIDI instruments for playback. For example, while in the source directory to play the CSV music file `examples/ensemble.csv` with the Fluid (R3) General MIDI SoundFont located at `/usr/share/sounds/sf2/FluidR3_GM.sf2`:

```
$ ./konductor -m examples/ensemble.csv -f /usr/share/sounds/sf2/FluidR3_GM.sf2
```

>When you run the program for the first time, the JACK server should start up as well if it hasn't started already. If JACK is unable to use real-time scheduling, run the commands `sudo dpkg-reconfigure -p high jackd` to give JACK high priority and `sudo adduser [your-username] audio` to add your user account to the "audio" group, then restart your computer.

## Creating Your Own Score

Our program reads a CSV file consisting of lines of numbers in a certain format in order to play music. The first line consists of three numbers: the number of program changes to read, the number of note messages to read, and the PPQN (pulses/ticks per quarter note) value for the music.

The following lines, equal to the expected number of program changes, each consist of a channel number (0-15) and a program number (0-127 for General MIDI), telling FluidSynth which program/instrument maps to which channel.

Each of the remaining lines then represent a MIDI "note on" or "note off" message, which consists of the following five numbers in order:

* beat number
* ticks after current beat
* channel number
* key/note number
* 1 for "note on" or 0 for "note off"

All numbers must be separated by whitespace. See our example CSV files in the `examples` folder for reference. Ensure that each message is sorted by increasing beat number (and then preferably by increasing tick count too) as each MIDI message will be read and scheduled in the same order as in the file.

## Music & Usage Information

* `examples/ensemble.csv`
  * Original Composition: [Ode to Joy by L. V. Beethoven (1770–1827)](http://www.mutopiaproject.org/cgibin/piece-info.cgi?id=528)
  * Arranged for Woodwind Quintet by Edward Ly
  * Distributed under [Creative Commons](http://creativecommons.org/) license

## Last Updated

23 February 2017
