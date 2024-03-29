//
//  Brush.h
//  ActionPainting
//
//  Created by Lucas Jenß on 11/28/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef ActionPainting_Brush_h
#define ActionPainting_Brush_h

#include <cv.h>
//#include <highgui.h>

#define IS_INVALID_POINT(cvPoint) (cvPoint.x == -1 && cvPoint.y == -1)

class Brush {
    
    // Brush parameters
    double mRadius;
    CvScalar mColor;
    double mSoftness;
    double mPressure;
    bool mHard;
    
    // Algorithm stuff
    CvPoint mLastPoint;
    CvPoint mLastRenderPosition;
    double mLeftOverDistance;
    
    // Brush mask
    IplImage* mBrushMask;
    
    // Foreground
    IplImage* mForeground;
    
public:
    Brush();
    
    void createImageShape();
    void paint(IplImage* canvas, CvPoint point);
    void resetState();
    
    void stampMaskAt(IplImage* image, double x, double y);
    double spacing();
    
    // Setters
    void setRadius(double radius) { assert(radius > 0); mRadius = radius; }
    void setSoftness(double softness) { assert(softness >= 0 && softness <= 1); mSoftness = softness; }
    
    
};

#endif
