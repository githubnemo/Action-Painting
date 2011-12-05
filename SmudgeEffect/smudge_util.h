//
//  smudge_util.h
//  ActionPainting
//
//  Created by Lucas Jenß on 11/28/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef ActionPainting_smudge_util_h
#define ActionPainting_smudge_util_h

#include "Brush.h"

CvScalar addWithAlpha(CvScalar addThis, CvScalar ontoThis, double alpha);

void overlayImageWithAlpha(const IplImage* source, IplImage* target, double alpha);

void overlayImageWithAlphaMask(const IplImage* source, IplImage* target, const IplImage* alphaMask);

void stampMaskAt(const IplImage* mask, IplImage* image, IplImage* alphaMask, double x, double y);

double lineStampMask(Brush* brush, IplImage* image, CvPoint startPoint, CvPoint endPoint, double leftOverDistance);

IplImage* cvCopySubImage(IplImage* src, int x, int y, int width, int height);

#endif
