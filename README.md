# Music Motion

A virtual orchestra conductor application using the Microsoft Kinect for Windows v1. Developed as an Earlham College senior capstone research project in collaboration with the Music and Computer Science Departments.

This application tracks your hand's position and uses nothing but its vertical velocity and acceleration to detect beats and determine volume, respectively. A MIDI-like CSV file is used to store the desired music for playback.

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

>Ensure that all the required libraries listed above are installed and configured for your computer. If you are configuring libfreenect with `cmake`, be sure to use the option `-DBUILD_CV=ON`.

1. Clone this repository and `cd` to the source directory.
2. Create a new folder called `XKin` and clone the [XKin library repository](https://github.com/fpeder/XKin) into that folder. Follow the instructions provided to compile it.
3. Return to the source directory and run `make`.
4. Run the program with `./main <csv-file> <soundfont-file>`.

>Ensure that you have a SoundFont file that conforms to the General MIDI standard and note its location on your computer. On Linux, we recommend the Fluid (R3) General MIDI SoundFont located at `/usr/share/sounds/sf2/FluidR3_GM.sf2`.

>If JACK is unable to use real-time scheduling, run the commands `sudo dpkg-reconfigure -p high jackd` to give JACK high priority and `sudo adduser [your-username] audio` to add your user account to the "audio" group, and restart your computer.

