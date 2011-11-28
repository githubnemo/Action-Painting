//
//  main.cpp
//  SmudgeEffect
//
//  Created by Lucas Jenß on 11/14/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <iostream>

#include <cv.h>
#include <highgui.h>

#include "smudge_util.h"
#include "Brush.h"



bool debug = FALSE;


IplImage* img;
IplImage* screenBuffer;
bool dragging = FALSE;





// Algorithm parameters
int brushRadius = 10;       // Radius of the brush

int brushPressure = 100;    // Brush pressure goes from 0.0 to 1.0, so this is
                            // divided by 100 eventually.

int brushMaskBorder = 10;   // Border of $value pixels around the brush mask
                            // to allow correct anti alias of the drawn mask.

int brushDistance = 1;      // Defines how often we draw the brush, in this case
                            // we draw it whenever abs(cursor - lastCursor) > brushDistance

int brushSoftness = 50;

int smudgeBufferSize = 4;

// Algorithm specific variables
std::vector<IplImage*> smudgeBuffer(0);
IplImage* brushMask;
int lastX = -1;
int lastY = -1;
double lastLeftOverDistance = 0;



Brush currentBrush;



void drawCursor(int x, int y)
{
    if(screenBuffer != NULL) {
        cvReleaseImage(&screenBuffer);
    }
    
    //Get clean copy of image
    screenBuffer = cvCloneImage(img);

    CvScalar color;
    if(dragging) {
        color = CV_RGB(0, 255, 0);
    } else {
        color = CV_RGB(255, 0, 0);
    }

    //Draw a circle where is the mouse
	cvCircle(screenBuffer, cvPoint(x,y), brushRadius, color, 1, CV_AA, 0);
}


void onMouse( int event, int x, int y, int flags, void* param )
{
    
	drawCursor(x,y);
    
    
    if(dragging && lastX >= 0 && lastY >= 0) {
        currentBrush.setRadius(brushRadius);
        currentBrush.setSoftness(brushSoftness / 100.0);
        currentBrush.createImageShape();
        currentBrush.paint(img, cvPoint(x, y));
    }

	if(event==CV_EVENT_LBUTTONDOWN) {
        dragging = TRUE;
    } else if(event==CV_EVENT_LBUTTONUP) {
        dragging = FALSE;
        currentBrush.resetState();
    }
    
    lastX = x;
    lastY = y;
    
    cvShowImage("Smudge Demo", screenBuffer);
}

int main (int argc, const char* argv[])
{    
    const char* windowName = "Smudge Demo";

    cvNamedWindow(windowName, CV_WINDOW_AUTOSIZE);
    //cvNamedWindow("BrushMask", CV_WINDOW_AUTOSIZE);

    // Some sliders to modify the algorithm values
    cvCreateTrackbar("Brush Size", windowName, &brushRadius, 20);
    cvCreateTrackbar("Brush Pressure", windowName, &brushPressure, 100);
    cvCreateTrackbar("Brush Softness", windowName, &brushSoftness, 100);

    // Create our initial image, which we will smudge
    img = cvCreateImage(cvSize(640, 480), 8, 3);
    cvSet(img, CV_RGB(255, 255, 255));
    cvRectangle(img, cvPoint(150, 150), cvPoint(300, 300), CV_RGB(0, 0, 255), CV_FILLED);

    // The screen buffer is whats actually displayed to the user, containing,
    // for example, the cursor position.
    screenBuffer = cvCloneImage(img);

    cvSetMouseCallback(windowName, &onMouse, 0);
    
    printf("Brush radius: %d \n", brushRadius);
    //createBrushMask(brushRadius, brushMaskBorder);
    
    cvShowImage(windowName, img);
    
    //cvShowImage(windowName, screenBuffer);
    while(TRUE) {
        cvWaitKey(1000);
    }

    return 0;
}

