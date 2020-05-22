#############################
##
## Makefile for Panorama
## Stitching using OpenCV
##
## Author: Jonathan Kelaty
## - MacOS compilation -
##
#############################

COMPILER = clang++
C++FLAGS = -std=c++17 -stdlib=libc++
PROGRAM_NAME = panorama
OPENCV = -I/usr/local/Cellar/opencv/4.3.0/include/opencv4
LIBS = `pkg-config --cflags --libs opencv4`

all: 
	$(COMPILER) $(C++FLAGS) $(PROGRAM_NAME).cpp -o $(PROGRAM_NAME) $(OPENCV) $(LIBS)

clean:
	rm -f $(PROGRAM_NAME)
