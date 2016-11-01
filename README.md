# Music Motion

A virtual orchestra conductor application using the Microsoft Kinect for Windows v1. Developed as an Earlham College senior capstone research project in collaboration with the Music and Computer Science Departments.

More info to come soon.

## Technical Overview

This application was developed using the following libraries. Some notable dependencies are also listed here.

* [libfreenect](https://openkinect.org/wiki/Main_Page)
* [XKin](https://github.com/fpeder/XKin)
  * [OpenCV](http://opencv.org/)
  * [FFTW](http://fftw.org/)
* [PortAudio](http://www.portaudio.com/)
* [OpenGL](https://www.opengl.org/)

## Setup

### Linux

>Note: Ensure that all the required libraries listed above are installed and configured for your computer. If you are configuring libfreenect with `cmake`, be sure to use the option `-DBUILD_CV=ON`. Similarly, use the option `-DWITH_OPENGL=ON` when configuring OpenCV.

1. Clone this repository and `cd` to the source directory.
2. Create a new folder called `XKin` and clone the [XKin library repository](https://github.com/fpeder/XKin) into that folder. Follow the instructions provided to compile it.
3. Return to the source directory and run `make`.

