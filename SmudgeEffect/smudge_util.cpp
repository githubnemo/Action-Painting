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


/*
 * Begin: Stuff according to
 * http://losingfight.com/blog/2007/08/18/how-to-implement-a-basic-bitmap-brush/
 */


// Draw the passed IplImage centered on the given coordinates
void stampMaskAt(const IplImage* mask, IplImage* image, IplImage* alphaMask, double x, double y)
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
            CvScalar alphaPoint = cvGet2D(alphaMask, yy, xx);
            
            CvScalar newPointWithAlpha = addWithAlpha(newPoint, currentPoint, alphaPoint.val[0]);
            
            cvSet2D(image, drawY + yy, drawX + xx, newPointWithAlpha);
        }
    }
}


// Draw a line with the given mask
double lineStampMask(const IplImage* mask, IplImage* image, IplImage* alphaMask, CvPoint startPoint, CvPoint endPoint, double leftOverDistance)
{
    
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
        
        stampMaskAt(mask, image, alphaMask, startPoint.x + offsetX, startPoint.y + offsetY);
        
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
