//
//  smudge_util.cpp
//  ActionPainting
//
//  Created by Lucas Jenß on 11/28/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <cv.h>

#include "smudge_util.h"

// Adds the first point onto the second point, with an alpha value (0-255)
// Assumes that the CvScalars have 3 "channels"
CvScalar addWithAlpha(CvScalar addThis, CvScalar ontoThis, double alpha)
{
    double invertedAlphaPercentage = alpha / 255;
    double alphaPercentage = 1 - invertedAlphaPercentage;
    
    double d0 = ontoThis.val[0] * invertedAlphaPercentage + addThis.val[0] * alphaPercentage;
    double d1 = ontoThis.val[1] * invertedAlphaPercentage + addThis.val[1] * alphaPercentage;
    double d2 = ontoThis.val[2] * invertedAlphaPercentage + addThis.val[2] * alphaPercentage;
    
    return cvScalar(d0, d1, d2);
}

void overlayImageWithAlpha(const IplImage* source, IplImage* target, double alpha)
{
    assert(source->width == target->width && source->height == target->height);
    
    for(int y=0; y<source->height; y++) {
        for(int x=0; x<source->width; x++) {
            CvScalar sourcePoint = cvGet2D(source, y, x);
            CvScalar targetPoint = cvGet2D(target, y, x);
            CvScalar result = addWithAlpha(sourcePoint, targetPoint, alpha);
            cvSet2D(target, y, x, result);
        }
    }
}

void overlayImageWithAlphaMask(const IplImage* source, IplImage* target, const IplImage* alphaMask)
{
    assert(source->width == target->width && source->height == target->height);
    
    for(int y=0; y<source->height; y++) {
        for(int x=0; x<source->width; x++) {
            CvScalar sourcePoint = cvGet2D(source, y, x);
            CvScalar targetPoint = cvGet2D(target, y, x);
            CvScalar maskPoint = cvGet2D(alphaMask, y, x);
            
            double alpha = maskPoint.val[0];
            
            CvScalar result = addWithAlpha(sourcePoint, targetPoint, alpha);
            cvSet2D(target, y, x, result);
        }
    }
}