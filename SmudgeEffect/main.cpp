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
std::vector<IplImage*> smudgeBuffer(0);
IplImage* brushMask;
int lastX = -1;
int lastY = -1;
double lastLeftOverDistance = 0;


// Adds the first point onto the second point, with an alpha value (0-255)
// Assumes that the CvScalars have 3 "channels"
CvScalar addWithAlpha(CvScalar addThis, CvScalar ontoThis, double alpha) {
    double invertedAlphaPercentage = alpha / 255;
    double alphaPercentage = 1 - invertedAlphaPercentage;
    
    double d0 = ontoThis.val[0] * invertedAlphaPercentage + addThis.val[0] * alphaPercentage;
    double d1 = ontoThis.val[1] * invertedAlphaPercentage + addThis.val[1] * alphaPercentage;
    double d2 = ontoThis.val[2] * invertedAlphaPercentage + addThis.val[2] * alphaPercentage;
    
    return cvScalar(d0, d1, d2);
}


/*
 * Begin: Stuff according to
 * http://losingfight.com/blog/2007/08/18/how-to-implement-a-basic-bitmap-brush/
 */


// Draw the passed IplImage centered on the given coordinates
void stampMaskAt(const IplImage* mask, IplImage* image, double x, double y)
{
    int drawX = (int) (x - (mask->width / 2.0));
    int drawY = (int) (y - (mask->height / 2.0));
    
    // These are the coordinates in the mask-image where we start drawing.
    // They come into play when part of the image to be drawn lies outside 
    // of the actual canvas.
    int startX = 0,
        startY = 0,
        stopX = mask->width,
        stopY = mask->height;
    
    
    // Out of bounds checking for the upper and left edges
    if(drawX < 0) {
        startX = abs(drawX);
        drawX = 0;
    }
    
    if(drawY < 0) {
        startY = abs(drawY);
        drawY = 0;
    }
    
    
    // Same here, out of bounds checking for the right and lower edges
    int temp = image->width -(drawX + stopX);
    if(temp < 0) {
        stopX += temp;
    }
    
    temp = image->height - (drawY + stopY);
    if(temp < 0) {
        stopY += temp;
    }
    
    
    // And finally the "stamping"
    for(int yy = startY; yy < stopY; yy++) {
        for(int xx = startX; xx < stopX; xx++) {
            CvScalar newPoint = cvGet2D(mask, yy, xx);
            CvScalar currentPoint = cvGet2D(image, drawY + yy, drawX + xx);
            CvScalar alphaPoint = cvGet2D(brushMask, yy, xx);
            
            CvScalar newPointWithAlpha = addWithAlpha(newPoint, currentPoint, alphaPoint.val[0]);
            
            cvSet2D(image, drawY + yy, drawX + xx, newPointWithAlpha);
        }
    }
}


// Draw a line with the given mask
double lineStampMask(const IplImage* mask, IplImage* image, CvPoint startPoint, CvPoint endPoint, double leftOverDistance) {
    
    // Set the spacing between the stamps. 1/10th of the brush width is a good value.
    double spacing = mask->width * 0.1;
    
    // Anything less that half a pixel is overkill and could hurt performance.
    if(spacing < 0.5) {
        spacing = 0.5;
    }
    
    // Determine the delta of the x and y. This will determine the slope
    // of the line we want to draw.
    double deltaX = endPoint.x - startPoint.x;
    double deltaY = endPoint.y - startPoint.y;
    
    // Normalize the delta vector we just computed, and that becomes our step increment
    // for drawing our line, since the distance of a normalized vector is always 1
    double distance = sqrt( deltaX * deltaX + deltaY * deltaY );
    double stepX = 0.0;
    double stepY = 0.0;
    if( distance > 0.0 ) {
        double invertDistance = 1.0 / distance;
        stepX = deltaX * invertDistance;
        stepY = deltaY * invertDistance;
    }
    
    double offsetX = 0.0;
    double offsetY = 0.0;
    
    // We're careful to only stamp at the specified interval, so its possible
    // that we have the last part of the previous line left to draw. Be sure
    // to add that into the total distance we have to draw.
    double totalDistance = leftOverDistance + distance;
    
    // While we still have distance to cover, stamp
    while ( totalDistance >= spacing ) {
        // Increment where we put the stamp
        if ( leftOverDistance > 0 ) {
            // If we're making up distance we didn't cover the last
            //	time we drew a line, take that into account when calculating
            //	the offset. leftOverDistance is always < spacing.
            offsetX += stepX * (spacing - leftOverDistance);
            offsetY += stepY * (spacing - leftOverDistance);
            
            leftOverDistance -= spacing;
        } else {
            // The normal case. The offset increment is the normalized vector
            //	times the spacing
            offsetX += stepX * spacing;
            offsetY += stepY * spacing;
        }
        
        stampMaskAt(mask, image, startPoint.x + offsetX, startPoint.y + offsetY);
        
        // Remove the distance we just covered
        totalDistance -= spacing;
    }
    
    // Return the distance that we didn't get to cover when drawing the line.
    //	It is going to be less than spacing.
    return totalDistance;
}





/*
 * End: Stuff according to
 * http://losingfight.com/blog/2007/08/18/how-to-implement-a-basic-bitmap-brush/
 */






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



void createBrushMask(int brushRadius, int brushMaskBorder)
{
    int imageSize = (brushRadius * 2) + (brushMaskBorder * 2);

    IplImage * tempBrushMask = cvCreateImage(cvSize(imageSize , imageSize), 8, 1);
    cvSet(tempBrushMask, cvScalar(255));
    
    brushMask = cvCreateImage(cvSize(brushRadius*2, brushRadius*2), 8, 1);

    cvCircle(
        tempBrushMask,
        cvPoint(brushRadius+brushMaskBorder, brushRadius+brushMaskBorder),
        brushRadius,
        CV_RGB(0, 0, 0),
        CV_FILLED,
        CV_AA,
        0
    );

    cvSetImageROI(tempBrushMask, cvRect(brushMaskBorder, brushMaskBorder, brushRadius*2, brushRadius*2));
    cvCopy(tempBrushMask, brushMask);

    cvShowImage("BrushMask", brushMask);
}




void onMouse( int event, int x, int y, int flags, void* param )
{
    
	drawCursor(x,y);
    
    
    if(dragging && lastX >= 0 && lastY >= 0) {
        CvPoint startPoint = cvPoint(lastX, lastY);
        CvPoint endPoint = cvPoint(x, y);
        lastLeftOverDistance = lineStampMask(brushMask, img, startPoint, endPoint, lastLeftOverDistance);
    }

	if(event==CV_EVENT_LBUTTONDOWN) {
        dragging = TRUE;
    } else if(event==CV_EVENT_LBUTTONUP) {
        dragging = FALSE;
    }
    
    lastX = x;
    lastY = y;
    
    cvShowImage("Smudge Demo", screenBuffer);
}

int main (int argc, const char* argv[])
{

    const char* windowName = "Smudge Demo";

    cvNamedWindow(windowName, CV_WINDOW_AUTOSIZE);
    cvNamedWindow("BrushMask", CV_WINDOW_AUTOSIZE);

    // Some sliders to modify the algorithm values
    cvCreateTrackbar("Brush Size", windowName, &brushRadius, 20);
    cvCreateTrackbar("Brush Pressure", windowName, &brushPressure, 100);

    // Create our initial image, which we will smudge
    img = cvCreateImage(cvSize(640, 480), 8, 3);
    cvSet(img, CV_RGB(255, 255, 255));
    cvRectangle(img, cvPoint(150, 150), cvPoint(300, 300), CV_RGB(0, 0, 255), CV_FILLED);

    // The screen buffer is whats actually displayed to the user, containing,
    // for example, the cursor position.
    screenBuffer = cvCloneImage(img);

    cvSetMouseCallback(windowName, &onMouse, 0);
    
    printf("Brush radius: %d \n", brushRadius);
    createBrushMask(brushRadius, brushMaskBorder);
    
    
    cvShowImage(windowName, screenBuffer);
    while(TRUE) {
        cvWaitKey(1000);
    }

    return 0;
}

