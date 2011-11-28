//
//  smudge_util.h
//  ActionPainting
//
//  Created by Lucas Jenß on 11/28/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef ActionPainting_smudge_util_h
#define ActionPainting_smudge_util_h

CvScalar addWithAlpha(CvScalar addThis, CvScalar ontoThis, double alpha);

void overlayImageWithAlpha(const IplImage* source, IplImage* target, double alpha);

void overlayImageWithAlphaMask(const IplImage* source, IplImage* target, const IplImage* alphaMask);

#endif
