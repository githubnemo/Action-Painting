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


IplImage* img;
IplImage* screenBuffer;
bool dragging = FALSE;


void drawCursor(int x, int y)
{
    //Get clean copy of image
    screenBuffer = cvCloneImage(img);

    CvScalar color;
    if(dragging) {
        color = CV_RGB(0, 255, 0);
    } else {
        color = CV_RGB(255, 0, 0);
    }

    //Draw a circle where is the mouse
	cvCircle(screenBuffer, cvPoint(x,y), 5, color, 1, CV_AA, 0);
}

void onMouse( int event, int x, int y, int flags, void* param )
{
	drawCursor(x,y);
	if(event==CV_EVENT_LBUTTONDOWN) {
        puts("down");
        dragging = TRUE;
    } else if(event==CV_EVENT_LBUTTONUP) {
        puts("up");
        dragging = FALSE;
    }

}

int main (int argc, const char* argv[])
{

    cvNamedWindow("Smudge Demo", CV_WND_PROP_AUTOSIZE);

    // Create our initial image, which we will smudge
    img = cvCreateImage(cvSize(640, 480), 8, 3);
    cvSet(img, CV_RGB(255, 255, 255));
    cvRectangle(img, cvPoint(150, 150), cvPoint(300, 300), cvScalar(0), CV_FILLED);

    // The screen buffer is whats actually displayed to the user, containing,
    // for example, the cursor position.
    screenBuffer = cvCloneImage(img);

    cvSetMouseCallback("Smudge Demo",&onMouse, 0 );

    // Re-render our image ~ every 10ms
    while(TRUE) {
        cvShowImage("Smudge Demo", screenBuffer);
        cvWaitKey(10);
    }

    return 0;
}

