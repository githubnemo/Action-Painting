//
//  Brush.cpp
//  ActionPainting
//
//  Created by Lucas Jenß on 11/28/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include "Brush.h"
#include "smudge_util.h"


using namespace std;


Brush::Brush() {
    mBrushMask = NULL;
    mForeground = NULL;
    
    mColor = CV_RGB(0, 0, 255);
    
    mRadius = 10.0;
    mSoftness = 0.5;
    mPressure = 0.5;
    mHard = false;
    
    mLastPoint = cvPoint(-1, -1);
    mLastRenderPosition = cvPoint(-1, -1);
    mLeftOverDistance = 0.0;    
    
    //cvNamedWindow("BrushTest", CV_WINDOW_AUTOSIZE);
}

void Brush::createImageShape()
{    
    // 3 times because the width of the generated brush is 2 times the radius,
    // plus we need to leave a border of 1 times the border radius so that 
    // OpenCV's "cvCircle" function correctly antialiases the drawn circle
    CvSize imageSize = cvSize(mRadius * 3, mRadius * 3);
    CvPoint imageCenter = cvPoint(imageSize.width / 2, imageSize.height / 2);
    
    IplImage* img = cvCreateImage(imageSize, 8, 1);
    cvSet(img, cvScalar(255));
    
    IplImage* fg = cvCreateImage(imageSize, 8, 1);
    cvSet(fg, cvScalar(0));
    
    // The way we achieve "softness" on the edges of the brush is to draw
    // the shape full size with some transparency, then keep drawing the shape
    // at smaller sizes with the same transparency level. Thus, the center
    // builds up and is darker, while edges remain partially transparent.
    
    // First, based on the softness setting, determine the radius of the fully
    // opaque pixels.
    int innerRadius = (int) ceil(mSoftness * (0.5 - mRadius) + mRadius);
    int outerRadius = (int) ceil(mRadius);
    int i = 0;
    
    // The alpha level is always proportial to the difference between the inner, opaque
    // radius and the outer, transparent radius.
    double alphaStep = 1.0 / (outerRadius - innerRadius + 1);
    double alpha = 255 - (alphaStep * 255);
    //alpha = alpha * mPressure;
    
    for(i = outerRadius; i >= innerRadius; --i) {
        IplImage* temp = cvCreateImage(imageSize, 8, 1);
        cvSet(temp, cvScalar(255));
        
        cvCircle(
                 temp,
                 imageCenter,
                 i,
                 CV_RGB(alpha, alpha, alpha),
                 CV_FILLED,
                 CV_AA,
                 0
        );

        overlayImageWithAlphaMask(fg, img, temp);
        cvReleaseImage(&temp);

    }

    if(mBrushMask != NULL) {
        cvReleaseImage(&mBrushMask);
    }

    if(mForeground != NULL) {
        cvReleaseImage(&mForeground);
    }

    mBrushMask = cvCreateImage(cvSize(mRadius * 2 + 1 , mRadius * 2 +1), 8, 1);
    cvSetImageROI(img, cvRect(mRadius / 2, mRadius / 2, mRadius * 2+ 1, mRadius * 2 +1));
    cvCopy(img, mBrushMask);

    mForeground = cvCreateImage(cvSize(mRadius * 2 + 1 , mRadius * 2 +1), 8, 3);
    cvSet(mForeground, mColor);

    cvReleaseImage(&fg);
    cvReleaseImage(&img);


}


void Brush::paint(IplImage* canvas, CvPoint point)
{

    CvPoint startPoint;
    if(IS_INVALID_POINT(mLastPoint)) {
        startPoint = point;
    } else {
        startPoint = mLastPoint;
    }

    if(
       point.x > mRadius &&
       point.x < (canvas->width - mRadius) &&
       point.y > mRadius &&
       point.y < (canvas->height - mRadius)
    ) {
        mLeftOverDistance = lineStampMask(this, canvas, startPoint, point, mLeftOverDistance);
        mLastPoint = point;
    }




}


void Brush::resetState()
{
    mLastPoint = cvPoint(-1, -1);
    mLastRenderPosition = cvPoint(-1, -1);
    mLeftOverDistance = 0.0;
    
}

double Brush::spacing()
{
    // Set the spacing between the stamps. 1/10th of the brush width is a good value.
    return 1;
}

// Draw a single stamp on the given coordinates
void Brush::stampMaskAt(IplImage* image, double x, double y)
{
    if(!IS_INVALID_POINT(mLastRenderPosition)) {
        int maskSize = mBrushMask->width;
        CvRect lastRoi = cvRect(mLastRenderPosition.x - (maskSize * 0.5), mLastRenderPosition.y - (maskSize * 0.5), maskSize, maskSize);
        CvRect currentRoi = cvRect(x - (maskSize * 0.5), y - (maskSize * 0.5), maskSize, maskSize);
        
        // Get the image from where our cursor was last
        IplImage* last = cvCopySubImage(image, lastRoi.x, lastRoi.y, maskSize, maskSize);
        
        // Create a targetSection image as large as the brush mask, and copy
        // our current ROI (where our cursor currently lies) into it.
        // We can't "overlayImageWithAlphaMask" directly on the passed image,
        // because mentioned function expects both images to be of the same
        // size
        cvSetImageROI(image, currentRoi);
        IplImage* targetSection = cvCreateImage(cvSize(maskSize, maskSize), image->depth, image->nChannels);
        cvCopy(image, targetSection);
        
        // Do the actual "stamping"
        overlayImageWithAlphaMask(last, targetSection, mBrushMask);
        
        // Copy processed ROI back to the canvas
        cvCopy(targetSection, image);
        
        cvResetImageROI(image);
        cvReleaseImage(&targetSection);
        cvReleaseImage(&last);
    }
    
    mLastRenderPosition = cvPoint(x, y);
}

