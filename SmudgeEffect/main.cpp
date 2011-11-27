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

int smudgeBufferSize = 4;

// Algorithm specific variables
//int smudgeBufferSize = 0;
//IplImage** smudgeBuffer;
std::vector<IplImage*> smudgeBuffer(0);
IplImage* brushMask;
int lastX;
int lastY;


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
	cvCircle(screenBuffer, cvPoint(x,y), brushRadius, color, 1, CV_AA, 0);
}

IplImage* cvCopySubImage(IplImage* src, int x, int y, int width, int height)
{
    IplImage* dst;

    // Set the region of intereset to the portion we want to extract
    cvSetImageROI(src, cvRect(x, y, width, height));

    // Create our destination image
    dst = cvCreateImage( cvSize(width, height), src->depth, src->nChannels );

    // Copy the region of interest to our destination image
    cvCopy(src, dst);

    // We want to reset the region of interest here, or it will persist
    // even after we leave this function
    cvResetImageROI(src);

    return dst;
}

void drawImageAlphaMask(const IplImage* src, const IplImage* dst, const IplImage* mask) {
    assert(src->width == mask->width && src->height == mask->height);



}

IplImage* createBrushMask(int brushRadius, int brushMaskBorder)
{
    int imageSize = (brushRadius * 2) + (brushMaskBorder * 2);

    brushMask = cvCreateImage(cvSize(imageSize , imageSize), 8, 1);
    cvSet(brushMask, cvScalar(255));

    cvCircle(
        brushMask,
        cvPoint(brushRadius+brushMaskBorder, brushRadius+brushMaskBorder),
        brushRadius,
        CV_RGB(0, 0, 0),
        CV_FILLED,
        CV_AA,
        0
    );

    cvSetImageROI(brushMask, cvRect(brushMaskBorder, brushMaskBorder, brushRadius*2, brushRadius*2));

    cvShowImage("test", brushMask);

    return brushMask;
}

CvScalar getValueForPoint(int x, int y, int xi, int yi) {
    //IplImage* src = smudgeBuffer.at(0);
    
    double r = 0;
    double g = 0;
    double b = 0;
    
    double percentages[] = {0.5, 0.25, 0.15, 0.1};
    
    unsigned long size = MIN(smudgeBufferSize, smudgeBuffer.size());
    for(int i=0; i < size; i++) {
        IplImage * current = smudgeBuffer.at(i);
        CvScalar point = cvGet2D(current, yi, xi);
        
        
        
        
        b += point.val[0] * percentages[i];
        g += point.val[1] * percentages[i];
        r += point.val[2] * percentages[i];
        
        //printf("i=%d -> in_r: %f in_g: %f in_b: %f\n", i, r, g, b);
    }
    
    //printf("r: %f g: %f b: %f\n", r, g, b);
    

    
    return cvScalar(b, g, r);
}


void drawSingleSmudgeStep(int x, int y) {
    IplImage* src = smudgeBuffer.at(0);
    
    
    // The mouse cursor is the center of our brush, but we need (x, y) to
    // be the upper right corner of our brush.
    x = x - (src->width/2);
    y = y - (src->width/2);
    
    
    // Make sure we don't run out of bounds. Begin.
    if(x < 0) {
        x = 0;
    }
    
    if(y < 0) {
        y = 0;
    }
    
    if(x > (img->width - src->width)) {
        x = img->width - src->width;
    }
    
    if(y > (img->height - src->height)) {
        y = img->height - src->height;
    }
    // Make sure we don't run out of bounds. End.
    
    
    
    if(abs(x - lastX) < brushDistance && abs(y - lastY) < brushDistance) {
        return;
    }
    
    lastX = x;
    lastY = y;
    
    //cvSetImageROI(img, cvRect(x, y, src->width, src->height));
    //cvAddWeighted(img, 0.5, src, 0.5, 0, img);
    //cvResetImageROI(img);
    
    for(int yi=0; yi < src->height && y+yi < img->height;  yi++) {
        for(int xi=0; xi < src->width && x+xi < img->width; xi++) {
            
            
            CvScalar source = cvGet2D(img, y+yi, x+xi);
            //CvScalar value = cvGet2D(src, yi, xi);
            CvScalar value = getValueForPoint(x, y, xi, yi);
            CvScalar mask = cvGet2D(brushMask, yi, xi);
            
            double nalpha = mask.val[0] / 255;
            double alpha = 1 - nalpha;
            value.val[0] = source.val[0] * nalpha + value.val[0] * alpha;
            value.val[1] = source.val[1] * nalpha + value.val[1] * alpha;
            value.val[2] = source.val[2] * nalpha + value.val[2] * alpha;
            
            cvSet2D(img, (y+yi), (x+xi), value);
            
        }
    }

}

void applySmudge(int x, int y)
{
    if(!dragging) return;

    while(smudgeBuffer.size() > smudgeBufferSize) {
        smudgeBuffer.erase(smudgeBuffer.begin());
    }
    
    IplImage* brush = cvCopySubImage(img, x-brushRadius, y-brushRadius, brushRadius*2, brushRadius*2);
    smudgeBuffer.push_back(brush);
    
    if(smudgeBuffer.size() == 1) {
        lastX = x;
        lastY = y;

        brushMask = createBrushMask(brushRadius, brushMaskBorder);
        //cvReleaseImage(&brushMask);
    } else {

        int deltaX = x - lastX;
        int deltaY = y - lastY;
        
        if(deltaX > 0) {
            // y = slope*x + b
            double slope = (double) deltaY / (double) deltaX;
            double b = y - (x * slope);
            
            int cX = lastX;
            while(cX < x) {
                cX++;
                double cY = cX * slope + b;
                drawSingleSmudgeStep(cX, cY);
                
                if(debug)
                    printf("--> cX: %d, cY: %f slope: %f \n", cX, cY, slope);
            }
            
            
        }
        
        printf("x: %d, y: %d \n", x, y);
        drawSingleSmudgeStep(x, y);
        
        //cvSetImageROI(img, cvRect(x, x, brushRadius, brushRadius));
        
    }
    
    
}

void onMouse( int event, int x, int y, int flags, void* param )
{
    applySmudge(x, y);
	drawCursor(x,y);

	if(event==CV_EVENT_LBUTTONDOWN) {
        dragging = TRUE;
    } else if(event==CV_EVENT_LBUTTONUP) {
        dragging = FALSE;
        smudgeBuffer.clear();
    }

}

int main (int argc, const char* argv[])
{

    const char* windowName = "Smudge Demo";

    cvNamedWindow(windowName, CV_WINDOW_AUTOSIZE);
    cvNamedWindow("test", CV_WINDOW_AUTOSIZE);



    // Some sliders to modify the algorithm values
    cvCreateTrackbar("Brush Size", windowName, &brushRadius, 20);
    cvCreateTrackbar("Brush Pressure", windowName, &brushPressure, 100);

    // Create our initial image, which we will smudge
    img = cvCreateImage(cvSize(640, 480), 8, 3);
    cvSet(img, CV_RGB(255, 255, 255));
    cvRectangle(img, cvPoint(150, 150), cvPoint(300, 300), cvScalar(0), CV_FILLED);

    // The screen buffer is whats actually displayed to the user, containing,
    // for example, the cursor position.
    screenBuffer = cvCloneImage(img);

    cvSetMouseCallback(windowName,&onMouse, 0 );


    /* Begin: Simulate some smudging */
    dragging = TRUE;
    applySmudge(285, 195);
    applySmudge(296, 191);
    debug = TRUE;
    applySmudge(349, 174);
    debug = FALSE;
    applySmudge(385, 163);
    applySmudge(445, 149);
    applySmudge(454, 146);
    applySmudge(454, 146);
    dragging = FALSE;
    smudgeBuffer.clear();
    screenBuffer = cvCloneImage(img);
    /* End: Simulate some smudging */
    
    
    // Re-render our image ~ every 10ms
    while(TRUE) {
        cvShowImage(windowName, screenBuffer);
        cvWaitKey(10);
    }

    return 0;
}

