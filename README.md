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

To compile the program, simply clone the repository and run `make all` in the project directory. Ensure that all the required libraries listed above are installed and configured for your computer.

>Note: if you are configuring libfreenect with `cmake`, be sure to use the option `-DBUILD_CV=ON`. Similarly, use the option `-DWITH_OPENGL=ON` when configuring OpenCV.

