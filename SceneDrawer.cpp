/*
 * Copyright (c) 2011 Marian Tietz, Lucas Jenß
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "SceneDrawer.h"

#ifdef USE_GLUT
#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
        #include <GLUT/glut.h>
#else
        #include <GL/glut.h>
#endif
#else
#include "opengles.h"
#endif

#include <cmath>
#include <cv.h>
#include <highgui.h>

#include "SmudgeEffect/smudge_util.h"
#include "SmudgeEffect/Brush.h"

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;
extern xn::ImageGenerator g_ImageGenerator;
extern std::list<IplImage*> g_backgroundImages;

// g_currentBackgroundImage is only a position indicator which background
// image is currently active.
//
// g_pBgImg holds the address of the active background image.
std::list<IplImage*>::const_iterator g_currentBackgroundImage;
IplImage* g_pBgImg;
GLfloat g_pfTexCoords[8];


// Swipe detection ಠ_ಠ
// g_nHistorySize is the amount of points to capture
std::map<int, std::list<XnPoint3D> > g_History;
XnFloat* g_pfPositionBuffer;
int g_nHistorySize = 30;
// See DetectSwipe for details of the following 2 variables
int g_swipeMaxYDelta = 100;
int g_swipeMinWidth = 150;
// swipe to right: image fades from left (fadeDirection = -1)
// swipte to left: image fades from right (fadeDirection = 1)
int g_fadeDirection = 1; // -1 from left, 0 none, +1 from right
int g_fadeXPosition = 0; // Value of left/right edge of new image
int g_fadeStep = 50;	 // How many pixels fade per step


bool g_bDrawDebugInfo = true;


/* Brush settings */

// Smudge Algorithm parameters
int brushRadius = 10;       // Radius of the brush

int brushPressure = 100;    // Brush pressure goes from 0.0 to 1.0, so this is
// divided by 100 eventually.

int brushMaskBorder = 10;   // Border of $value pixels around the brush mask
// to allow correct anti alias of the drawn mask.

int brushDistance = 1;      // Defines how often we draw the brush, in this case
// we draw it whenever abs(cursor - lastCursor) > brushDistance

int brushSoftness = 50;

int smudgeBufferSize = 4;

Brush brushes[2];



struct TextureData {
	unsigned char*	data;
	int 			width;
	int 			height;
	float			XPos;
	float			YPos;
	int				XRes;
	int				YRes;
	GLuint			id;
};

typedef struct TextureData TextureData;


static unsigned int getClosestPowerOfTwo(unsigned int n)
{
	unsigned int m = 2;
	while(m < n) m<<=1;

	return m;
}


/**
 * Initialize texture for DrawDepthMap.
 *
 * RGBA texture with getClosestPowerOfTwo(width) x getClosestPowerOfTwo(height).
 */
static void initTexture(TextureData* pTexData, int nXRes, int nYRes)
{
	GLuint texID = 0;
	glGenTextures(1,&texID);

	pTexData->width = getClosestPowerOfTwo(nXRes);
	pTexData->height = getClosestPowerOfTwo(nYRes);

	pTexData->data = new unsigned char[pTexData->width * pTexData->height * 4];

	pTexData->XPos =(float)nXRes/pTexData->width;
	pTexData->YPos =(float)nYRes/pTexData->height;
	pTexData->XRes = nXRes;
	pTexData->YRes = nYRes;

	glBindTexture(GL_TEXTURE_2D,texID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	pTexData->id = texID;
}


static void DrawRectangle(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
	GLfloat verts[8] = {
		topLeftX, topLeftY,
		topLeftX, bottomRightY,
		bottomRightX, bottomRightY,
		bottomRightX, topLeftY
	};

	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glFlush();
}


static void DrawTexture(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, g_pfTexCoords);

	DrawRectangle(topLeftX, topLeftY, bottomRightX, bottomRightY);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}


XnFloat Colors[][3] =
{
	{0,1,1},
	{0,0,1},
	{0,1,0},
	{1,1,0},
	{1,0,0},
	{1,.5,0},
	{.5,1,0},
	{0,.5,1},
	{.5,0,1},
	{1,1,.5},
	{1,1,1}
};
XnUInt32 nColors = 10;


static void glPrintString(void *font, char *str)
{
	size_t i,l = strlen(str);

	for(i=0; i<l; i++)
	{
		glutBitmapCharacter(font,*str++);
	}
}


// Draws part of the player's skeleton, conneting two parts with a line
static void DrawLimb(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2)
{
	if (!g_UserGenerator.GetSkeletonCap().IsTracking(player))
	{
		printf("not tracked!\n");
		return;
	}

	XnSkeletonJointPosition joint1, joint2;
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);

	if (joint1.fConfidence < 0.5 || joint2.fConfidence < 0.5)
	{
		return;
	}

	XnPoint3D pt[2];
	pt[0] = joint1.position;
	pt[1] = joint2.position;

	g_DepthGenerator.ConvertRealWorldToProjective(2, pt, pt);
	glVertex3i(pt[0].X, pt[0].Y, 0);
	glVertex3i(pt[1].X, pt[1].Y, 0);
}




// Return reference to the cv IplImage of the background image
static IplImage* getBackgroundImage() {
	return g_pBgImg;
}


static void setBackgroundImage(IplImage* img) {
	g_pBgImg = img;
}


static void DrawUserLabels(XnUserID player) {
	char strLabel[20] = "";
	XnUserID aUsers[15];
	XnUInt16 nUsers = 15;
	g_UserGenerator.GetUsers(aUsers, nUsers);
	for (int i = 0; i < nUsers; ++i)
	{
		XnPoint3D com;
		g_UserGenerator.GetCoM(aUsers[i], com);
		g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

		if (aUsers[i] == player)
			sprintf(strLabel, "%d (Player)", aUsers[i]);
		else
			sprintf(strLabel, "%d. Thing", aUsers[i]);

		glColor4f(1-Colors[i%nColors][0], 1-Colors[i%nColors][1], 1-Colors[i%nColors][2], 1);

		glRasterPos2i(com.X, com.Y);
		glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
	}
}


static void DrawPlayerSkeleton(XnUserID player) {
	glBegin(GL_LINES);
	glColor4f(1-Colors[player%nColors][0], 1-Colors[player%nColors][1], 1-Colors[player%nColors][2], 1);
	DrawLimb(player, XN_SKEL_HEAD, XN_SKEL_NECK);

	DrawLimb(player, XN_SKEL_NECK, XN_SKEL_LEFT_SHOULDER);
	DrawLimb(player, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW);
	DrawLimb(player, XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND);

	DrawLimb(player, XN_SKEL_NECK, XN_SKEL_RIGHT_SHOULDER);
	DrawLimb(player, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW);
	DrawLimb(player, XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND);

	DrawLimb(player, XN_SKEL_LEFT_SHOULDER, XN_SKEL_TORSO);
	DrawLimb(player, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_TORSO);

	DrawLimb(player, XN_SKEL_TORSO, XN_SKEL_LEFT_HIP);
	DrawLimb(player, XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE);
	DrawLimb(player, XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT);

	DrawLimb(player, XN_SKEL_TORSO, XN_SKEL_RIGHT_HIP);
	DrawLimb(player, XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE);
	DrawLimb(player, XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT);
	glEnd();
}


// Fetch fade image according to g_fadeDirection:
// -1: The image left of the current
//  1: The image right of the current
//  0: NULL, no fade image needed
const std::list<IplImage*>::const_iterator getFadeBackgroundIterator() {
	std::list<IplImage*>::const_iterator currentImage(g_currentBackgroundImage);

	if(!g_fadeDirection)
		return currentImage;

	switch(g_fadeDirection) {
		// from left, so get the left
		case -1:
			if(g_backgroundImages.begin() == currentImage) {
				std::list<IplImage*>::const_iterator tmp(--g_backgroundImages.end());
				return tmp;
			}
			return --currentImage;

		// from right
		case  1:
			if(--g_backgroundImages.end() == currentImage) {
				std::list<IplImage*>::const_iterator tmp(g_backgroundImages.begin());
				return tmp;
			}
			return ++currentImage;
	}
	return currentImage;
}


inline void DrawBackground(TextureData& sceneTextureData)
{
	static bool bInitialized = false;
	int nXRes = sceneTextureData.XRes;
	int nYRes = sceneTextureData.YRes;

	if(!bInitialized) {
		// Take first image in list as initial background
		setBackgroundImage(*(g_backgroundImages.begin()));
		g_currentBackgroundImage = g_backgroundImages.begin();

		bInitialized = true;
	}

	// Background image data
	IplImage* pCvBgImage = getBackgroundImage();
	const XnUInt8* pBgImage = (const XnUInt8*)pCvBgImage->imageData;
	unsigned char* pDestImage = sceneTextureData.data;

	const IplImage* pFadeImage = NULL;
	const XnUInt8* pFadeImageData = NULL;
	int nFadeImageWidthStep = -1;

	// Setup fading image, if needed
	if(g_fadeDirection != 0) {
		pFadeImage = *(getFadeBackgroundIterator());
		pFadeImageData = (const XnUInt8*)pFadeImage->imageData;
		nFadeImageWidthStep = pFadeImage->widthStep;
	}

	// Prepare the texture map
	for (unsigned int nY=0; nY < nYRes; nY++)
	{
		for (unsigned int nX=0; nX < nXRes; nX++)
		{
			// Draw background image
			XnUInt8 r,g,b;

			if(nY > pCvBgImage->height || nX > pCvBgImage->width) {
				r = 0; g = 0; b = 0;
			} else {
				r = pBgImage[2];
				g = pBgImage[1];
				b = pBgImage[0];
			}

			switch(g_fadeDirection) {
				case -1:
					// Get left image
					if(nX <= g_fadeXPosition) {
						r = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 2];
						g = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 1];
						b = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 0];
					}
					break;
				case  1:
					// Get right image
					if(nX >= g_fadeXPosition) {
						r = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 2];
						g = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 1];
						b = pFadeImageData[nFadeImageWidthStep * nY + nX*3 + 0];
					}
					break;
			}

			pDestImage[0] = r;
			pDestImage[1] = g;
			pDestImage[2] = b;

			pBgImage += 3;
			pDestImage += 3;
		}

		pDestImage += (sceneTextureData.width - nXRes) *3;
		pBgImage += (pCvBgImage->widthStep - nXRes*3);
	}

	// Mark fading as finished, if it reached the goal
	if((g_fadeDirection == -1 && g_fadeXPosition >= nXRes) ||
	   (g_fadeDirection ==  1 && g_fadeXPosition <= 0))
	{
		g_currentBackgroundImage = getFadeBackgroundIterator();

		g_fadeDirection = 0;
		g_fadeXPosition = 0;

		setBackgroundImage(*g_currentBackgroundImage);
	} else if(g_fadeDirection == -1) {
		g_fadeXPosition += g_fadeStep;
	} else if(g_fadeDirection ==  1) {
		g_fadeXPosition -= g_fadeStep;
	}
}


// Modify the real world image captured by the camera
inline void ProcessRealWorldImage(
		const TextureData& sceneTextureData,
		const XnUInt8* source,
		XnUInt8* target)
{
	static bool initialized=false;
	static IplImage* srcImage;
	static IplImage* targetImage;

	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

	if(!initialized) {
		srcImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 3);
		targetImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 3);

		initialized = true;
	}

	srcImage->imageData = (char*) source;
	cv::Mat imageMat(srcImage);

	targetImage->imageData = (char*) target;
	cv::Mat targetImageMat(targetImage);

    //cv::medianBlur(imageMat, targetImageMat, 11);

	//cvShowImage("image display", targetImage);
}


/**
 * Smooth the user's pixels (>0 for user pixel, 0 for not user)
 * using cvErode. Returns the pointer to the newly allocated
 * user pixels.
 */
inline XnLabel* SmoothenUserPixels(
		const TextureData &sceneTextureData,
		const XnLabel* pLabels,
		const XnUserID playerId)
{
	static bool initialized=false;
	static IplImage* srcImage;
	static char*	srcImageData;
	static IplImage* targetImage;
	static IplConvKernel* erodeKernel;
	static XnLabel*	pTargetLabels;

	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

	if(!initialized) {
		srcImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 1);
		targetImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 1);
		// erode kernel (7x7 ellipsis)
		erodeKernel = cvCreateStructuringElementEx(7,7,2,2,CV_SHAPE_ELLIPSE );

		srcImageData = new char[nXRes * nYRes * 1];
		pTargetLabels = new XnLabel[nXRes * nYRes];

		initialized = true;
	}

	if(playerId == 0) {
		return (XnLabel*)pLabels;
	}

	// Copy pixels to srcImage, only black and white
	for(unsigned int y=0; y < nYRes; y++) {
		for(unsigned int x=0; x < nXRes; x++) {
			XnLabel label = *pLabels;
			char value = (label == playerId) ? 0xFF : 0;
			srcImageData[y*srcImage->widthStep+x] = value;
			pLabels++;
		}
	}

	srcImage->imageData = (char*) srcImageData;

	// Do the erosion, 2 iterations (more iterations -> more erosion)
	cvErode(srcImage, targetImage, erodeKernel, 1);

#if 0
	/* Debug output */
	cvShowImage("source image", srcImage);
	cvShowImage("test image", targetImage);
	cvWaitKey(1);
#endif


	// Copy pixels from eroded image to target user pixel memory
	for(unsigned int y=0; y < nYRes; y++) {
		for(unsigned int x=0; x < nXRes; x++) {
			char value = targetImage->imageData[y*srcImage->widthStep+x];
			pTargetLabels[y*nXRes+x] = (value != 0) ? playerId : 0;
		}
	}

	return pTargetLabels;
}

// Check the kernel sized maskSize*maskSize around point p for green
// color. If the green amount is high enough (>minPercent) true is returned.
static bool checkKernelForGreen(
		const TextureData& texData,
		const XnUInt8* img,
		XnPoint3D p,
		short maskSize,
		short minPercent,
		bool print)
{
	int green = 0, other = 1;

	for(int i=-(maskSize/2); i < maskSize/2; i++) {
		int yoffset = texData.XRes * ((int)p.Y+i) * 3;

		for(int j=-(maskSize/2); j < maskSize/2; j++) {
			int offset = yoffset + ((int)p.X + j) * 3;

			if((int)p.X+j < texData.XRes && (int)p.Y+i < texData.YRes)
			{
				const XnUInt8* current = &img[offset];

				other += current[0] + current[1] + current[2];
				green += current[1];
			}
		}
	}

	if(print)
		printf("%d, %d => %lf (%d)\n", green, other, (double)green/other * 100,
			(double)green/other * 100 > minPercent);

	return (double)green/other * 100 > minPercent;
}


double CosineInterpolate(
   double y1,double y2,
   double mu)
{
   double mu2;

   mu2 = (1-cos(mu*M_PI))/2;
   return(y1*(1-mu2)+y2*mu2);
}



// Return true if buffer has reached size limit
static bool CaptureHandMovement(
	XnUserID player,
	int handId,
	XnPoint3D projectivePoint)
{

	size_t historySize = g_History[handId].size();

	if(historySize > 0) {
		double sY = CosineInterpolate(g_History[handId].back().Y, projectivePoint.Y, 1);
		projectivePoint.Y = sY;
	}


	// Add new position to the history buffer
	g_History[handId].push_front(projectivePoint);

	// Keep size of history buffer
	if (historySize > g_nHistorySize) {
		g_History[handId].pop_back();
		return true;
	}

	return false;
}



static void CaptureSomeHandMovements(XnUserID player, int times) {
	for(int i=0; i < times; i++) {
		XnSkeletonJointPosition rightHandJoint, leftHandJoint;

		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player,
				XN_SKEL_LEFT_HAND, leftHandJoint);
		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player,
				XN_SKEL_RIGHT_HAND, rightHandJoint);

		if(rightHandJoint.fConfidence >= 0.5 && leftHandJoint.fConfidence >= 0.5) {
			XnPoint3D points[] = {leftHandJoint.position, rightHandJoint.position};

			g_DepthGenerator.ConvertRealWorldToProjective(2,points,points);

			CaptureHandMovement(player, 0, points[0]);
			CaptureHandMovement(player, 1, points[1]);
		}
	}
}


/**
 * Computes a function f(x) from the given p0 and pN
 * and returns the rounded up y value of the given x.
 */
static int calcF(double x, XnPoint3D &p0, XnPoint3D &pN) {
	double m = (pN.Y - p0.Y) / (pN.X - p0.X);
	double b = -m * p0.X + p0.Y;

	return m*x + b;
}

// Stores the swipe direction in fromLeft parameter if it's not NULL
static bool DetectSwipe(int LineSize, std::list<XnPoint3D> points, bool* fromLeft)
{
    int MinWidth = g_swipeMinWidth; // required horizontal distance
    int MaxYDelta = g_swipeMaxYDelta; // max mount of vertical variation

	XnPoint3D begin = *points.begin();
	XnPoint3D end = *(--points.end());

    float x1 = begin.X;
    float y1 = begin.Y;
    float x2 = end.X;
    float y2 = end.Y;

	XnPoint3D p0;
	XnPoint3D pN;

	if(x1 < x2) {
		p0 = begin;
		pN = end;
	} else {
		p0 = end;
		pN = begin;
	}

#if 0
	std::cout << "x1: " << x1 << std::endl;
	std::cout << "y1: " << y1 << std::endl;
	std::cout << "x2: " << x2 << std::endl;
	std::cout << "y2: " << y2 << std::endl;
#endif

    if (abs(x1 - x2) < MinWidth)
        return false;

    if (abs(y1 - y2) > MaxYDelta)
        return false;

    //for (int i = 1; i < LineSize - 2; i++)
	std::list<XnPoint3D>::const_iterator PositionIterator;

	for(PositionIterator = ++(points.begin());
		PositionIterator != points.end();
		PositionIterator++)
    {
		XnPoint3D point(*PositionIterator);

		int okY = calcF(point.X, p0, pN);

		if(abs(point.Y - okY) > MaxYDelta) {
			return false;
		}
    }

	if(fromLeft != NULL) {
		*fromLeft = (x1 < x2);
	}

    return true;
}


inline void SmudgeAtPosition(
	const TextureData& sceneTextureData,
	const int x,
	const int y,
	const int handId)
{
    static bool initialized=false;
	static IplImage* targetImage;

    static cv::Mat* imageMat;
    static cv::Mat* targetImageMat;

    XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

    brushes[handId].setRadius(brushRadius);
    brushes[handId].setSoftness(brushSoftness / 100.0);
    brushes[handId].createImageShape();

	brushes[handId].paint(getBackgroundImage(), cvPoint(x, y));
}


void DoFadeFromRight() {
	g_fadeDirection = 1;
	g_fadeXPosition = getBackgroundImage()->width;
}

void DoFadeFromLeft() {
	g_fadeDirection = -1;
	g_fadeXPosition = 0;
}


// Do swipe action if possible
static void doSwipe(XnUserID player, XnPoint3D* points) {
	// Swipe detection to change background.
	// Search for swipe gesture on each hand.
	for(unsigned int handId=0; handId < 2; handId++)
	{
		bool enoughPoints = CaptureHandMovement(player, handId, points[handId]);

		if(enoughPoints) {
			XnUInt32 nPoints = 0;
			XnUInt32 i = 0;

			// Go over all previous positions of current hand
			std::list<XnPoint3D>::const_iterator PositionIterator;
			for (PositionIterator = g_History[handId].begin();
				PositionIterator != g_History[handId].end();
				++PositionIterator, ++i)
			{
				// Add position to buffer
				XnPoint3D pt(*PositionIterator);
				g_pfPositionBuffer[3*i + 0] = pt.X;
				g_pfPositionBuffer[3*i + 1] = pt.Y;
				g_pfPositionBuffer[3*i + 2] = 0;//pt.Z();
			}

			// Set color
			// Draw buffer:
			bool fromLeft = false;
			bool swiped = DetectSwipe(g_nHistorySize, g_History[handId], &fromLeft);

			// draw red line if swiped from left, draw
			// blue one if swiped from right. Otherwise white.
			if(swiped) {
				if(fromLeft) {
					glColor4f(1,0,0,1);
					DoFadeFromRight();
				} else {
					glColor4f(0,0,1,1);
					DoFadeFromLeft();
				}
				break;
			} else {
				glColor4f(1,1,1,1);
			}

			if(g_bDrawDebugInfo) {
				glPointSize(2);
				glVertexPointer(3, GL_FLOAT, 0, g_pfPositionBuffer);
				glDrawArrays(GL_LINE_STRIP, 0, i);

				glPointSize(8);
				glDrawArrays(GL_POINTS, 0, 1);
			}
			glFlush();
		}
	}
}


inline void DrawPlayer(
		const TextureData& sceneTextureData,
		const xn::SceneMetaData& smd,
		const xn::ImageMetaData& imd,
		XnUserID player)
{
	static bool bInitialized = false;
	static XnUInt8* pRealWorldImage;
	static XnLabel* pLabels;

	if(!bInitialized) {
		pRealWorldImage = new XnUInt8[smd.XRes()*smd.YRes()*3];

		bInitialized = true;
	}

	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

	unsigned char* pTexBuf = sceneTextureData.data;

	unsigned char* pDestImage = pTexBuf;
	const XnLabel* pOrgLabels = smd.Data();

	{
		// Real world image data
		const XnUInt8* pImage = imd.Data();
		unsigned int nImdXRes = imd.XRes();
		int texWidth = sceneTextureData.width;

		memcpy(pRealWorldImage, pImage, imd.XRes()*imd.YRes()*3);

		const XnLabel* pLabels = SmoothenUserPixels(sceneTextureData, pOrgLabels, player);

		ProcessRealWorldImage(sceneTextureData, pImage, pRealWorldImage);

		// Prepare the texture map
		for (unsigned int nY=0; nY < nYRes; nY++)
		{
			for (unsigned int nX=0; nX < nXRes; nX++)
			{
				XnLabel label = *pLabels;

				if(label == player) {
					// Player detected, use player image
					int offset = nY * nImdXRes * 3 + nX * 3;

					int value = (pRealWorldImage[offset + 2] - 127) / 2;

					if((pDestImage[0] + value < 255 && pDestImage[0] + value > 0)
					&& (pDestImage[1] + value < 255 && pDestImage[1] + value > 0)
					&& (pDestImage[2] + value < 255 && pDestImage[2] + value > 0))
					{
						pDestImage[0] += value;

						pDestImage[1] += value;

						pDestImage[2] += value;
					} else if((pDestImage[0] + value > 255)
					|| (pDestImage[0] + value > 255)
					|| (pDestImage[0] + value > 255))
					{
						pDestImage[0] = 255;
						pDestImage[1] = 255;
						pDestImage[2] = 255;

					} else if((pDestImage[0] + value < 0)
					|| (pDestImage[0] + value < 0)
					|| (pDestImage[0] + value < 0))
					{
						pDestImage[0] = 0;
						pDestImage[1] = 0;
						pDestImage[2] = 0;
					}

					/*
					if((pDestImage[0] + value < 255 && pDestImage[0] + value > 0)
					&& (pDestImage[1] + value < 255 && pDestImage[1] + value > 0)
					&& (pDestImage[2] + value < 255 && pDestImage[2] + value > 0))
					{
						pDestImage[0] += value;

						pDestImage[1] += value;

						pDestImage[2] += value;
					}*/
				}

				pLabels++;
				pDestImage += 3;
			}

			pDestImage += (texWidth - nXRes) *3;
		}
	}


	glBindTexture(GL_TEXTURE_2D, sceneTextureData.id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sceneTextureData.width,
		sceneTextureData.height, 0,	GL_RGB, GL_UNSIGNED_BYTE,
		sceneTextureData.data);

	// Display the OpenGL texture map
	glColor4f(1,1,1,1);


	glEnable(GL_TEXTURE_2D);
	DrawTexture(nXRes,nYRes,0,0);
	glDisable(GL_TEXTURE_2D);


	if(g_bDrawDebugInfo) {
		DrawUserLabels(player);
	}


	// Just look for some hand movements
	CaptureSomeHandMovements(player, 2);


	// Sponge detection
	{
		XnSkeletonJointPosition rightHandJoint, leftHandJoint;

		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player,
				XN_SKEL_LEFT_HAND, leftHandJoint);
		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player,
				XN_SKEL_RIGHT_HAND, rightHandJoint);

		if(rightHandJoint.fConfidence >= 0.5 && leftHandJoint.fConfidence >= 0.5) {
			XnPoint3D points[2] = {leftHandJoint.position, rightHandJoint.position};
			char positionString[25];
			bool isGreenLeft = false, isGreenRight = false;

			g_DepthGenerator.ConvertRealWorldToProjective(2,points,points);

			isGreenLeft = checkKernelForGreen(sceneTextureData,
					imd.Data(), points[0], 15, 32, false);
			isGreenRight = checkKernelForGreen(sceneTextureData,
					imd.Data(), points[1], 15, 32, false);

			if(g_bDrawDebugInfo) {
				sprintf(positionString, "green=%d", isGreenRight);
				glColor4f(1,1,1,1);
				glRasterPos2i(points[1].X, points[1].Y);
				glPrintString(GLUT_BITMAP_HELVETICA_18, positionString);

				sprintf(positionString, "green=%d", isGreenLeft);
				glRasterPos2i(points[0].X, points[0].Y);
				glPrintString(GLUT_BITMAP_HELVETICA_18, positionString);
			}

			// TODO only if red side of sponge is shown
			doSwipe(player, points);

			// TODO reset last point of brush so that gaps are not smudged.
			// Imagine you smuding at point A then disabling smudge, going to
			// point B and smudging again. What happens is, that the line
			// between A and B is smudged, because A is the last known point.
			//
			// One could simply clear the point buffer?

			if(true || isGreenLeft) {
				SmudgeAtPosition(sceneTextureData, points[0].X, points[0].Y, 0);
			}

			if(true || isGreenRight) {
				SmudgeAtPosition(sceneTextureData, points[1].X, points[1].Y, 1);
			}
		}
	}



	// Draw skeleton of user
	if (g_bDrawDebugInfo)
	{
		DrawPlayerSkeleton(player);
	}
}


void DrawScene(
	const xn::DepthMetaData& dmd,
	const xn::SceneMetaData& smd,
	const xn::ImageMetaData& imd)
{
	static bool bInitialized = false;
	static TextureData sceneTextureData;

	if(!bInitialized) {
		initTexture(&sceneTextureData, dmd.XRes(), dmd.YRes());

		memset(g_pfTexCoords, 0, 8*sizeof(float));
		g_pfTexCoords[0] = sceneTextureData.XPos,
		g_pfTexCoords[1] = sceneTextureData.YPos,
		g_pfTexCoords[2] = sceneTextureData.XPos,
		g_pfTexCoords[7] = sceneTextureData.YPos;

		g_pfPositionBuffer = new XnFloat[g_nHistorySize*3];

		bInitialized = true;
	}

	DrawBackground(sceneTextureData);

	// Fetch player
	XnUserID aUsers[1];
	XnUInt16 nUsers = 1;

	g_UserGenerator.GetUsers(aUsers, nUsers);

	DrawPlayer(sceneTextureData, smd, imd, aUsers[0]);
}


