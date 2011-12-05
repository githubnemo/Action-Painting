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

#include <cv.h>
#include <highgui.h>

#include "SmudgeEffect/smudge_util.h"
#include "SmudgeEffect/Brush.h"

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;
extern xn::ImageGenerator g_ImageGenerator;
extern XnUserID g_nPlayer;
extern std::list<std::string> g_backgroundImages;

IplImage* g_pBgImg;
GLfloat g_pfTexCoords[8];

// Swipe detection ಠ_ಠ
// g_nHistorySize is the amount of points to capture
std::list<XnPoint3D> g_History;
XnFloat* g_pfPositionBuffer;
int g_nHistorySize = 15;


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

Brush currentBrush;




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


unsigned int getClosestPowerOfTwo(unsigned int n)
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
void initTexture(TextureData* pTexData, int nXRes, int nYRes)
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



void DrawRectangle(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
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


void DrawTexture(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
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


void glPrintString(void *font, char *str)
{
	size_t i,l = strlen(str);

	for(i=0; i<l; i++)
	{
		glutBitmapCharacter(font,*str++);
	}
}


// Draws part of the player's skeleton, conneting two parts with a line
void DrawLimb(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2)
{
	if (!g_UserGenerator.GetSkeletonCap().IsCalibrated(player))
	{
		printf("not calibrated!\n");
		return;
	}
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
IplImage* getBackgroundImage() {
	return g_pBgImg;
}


void setBackgroundImage(const char* path, int width, int height) {
	IplImage* img = cvLoadImage(path);

	int depth = img->depth;
	int channels = img->nChannels;

	IplImage* resizedImage = cvCreateImage(cv::Size(width,height), depth, channels);

	cvResize(img, resizedImage, CV_INTER_LINEAR);

	g_pBgImg = resizedImage;
}


void DrawUserLabels(XnUserID player) {
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


void DrawPlayerSkeleton(XnUserID player) {
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




inline void DrawBackground(TextureData& sceneTextureData)
{
	static bool bInitialized = false;
	int nXRes = sceneTextureData.XRes;
	int nYRes = sceneTextureData.YRes;

	if(!bInitialized) {
		// Take first image in list
		const char* path = (*g_backgroundImages.begin()).c_str();

		setBackgroundImage(path, nXRes, nYRes);

		bInitialized = true;
	}

	// Background image data
	IplImage* pCvBgImage = getBackgroundImage();
	const XnUInt8* pBgImage = (const XnUInt8*)pCvBgImage->imageData;

	// TODO load image directly as texture?

	unsigned char* pDestImage = sceneTextureData.data;

	// Prepare the texture map
	for (unsigned int nY=0; nY < nYRes; nY++)
	{
#if 1
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

			pDestImage[0] = r;
			pDestImage[1] = g;
			pDestImage[2] = b;

			pBgImage += 3;
			pDestImage += 3;
		}
#else
		// TODO optimize, use memcpy or something, could be done by setting
		// the texture to BGR and setting data to pBgImage
		printf("Writing %d (of %d)\n", nXRes*3, nYRes);
		memcpy(pDestImage, pBgImage, nXRes * 3);
#endif


		pDestImage += (sceneTextureData.width - nXRes) *3;
		pBgImage += (pCvBgImage->widthStep - nXRes*3);
	}
}


// Modify the real world image captured by the camera
inline void ProcessRealWorldImage(
		const TextureData& sceneTextureData,
		const XnUInt8* source,
		XnUInt8* target)
{
#if 0 // Uncomment if something useful is to happen here
	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;
	int texWidth = sceneTextureData.width;

	for(int nY=0; nY < nYRes; nY++) {
		for(int nX=0; nX < nXRes; nX++) {
			target += 3;
		}
	}
#endif

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


	//cvShowImage("image display", targetImage);
}


/**
 * Smooth the user's pixels (>0 for user pixel, 0 for not user)
 * using cvErode. Returns the pointer to the newly allocated
 * user pixels.
 */
inline XnLabel* SmoothenUserPixels(
		const TextureData &sceneTextureData,
		const XnLabel* pLabels)
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

	// Copy pixels to srcImage, only black and white
	for(unsigned int y=0; y < nYRes; y++) {
		for(unsigned int x=0; x < nXRes; x++) {
			XnLabel label = *pLabels;
			char value = (label != 0) ? 0xFF : 0;
			srcImageData[y*srcImage->widthStep+x] = value;
			pLabels++;
		}
	}

	srcImage->imageData = (char*) srcImageData;

	// Do the erosion, 2 iterations (more iterations -> more erosion)
	cvErode(srcImage, targetImage, erodeKernel, 1);

	/* Debug output
	cvShowImage("source image", srcImage);
	cvShowImage("test image", targetImage);
	cvWaitKey(1);
	*/


	// Copy pixels from eroded image to target user pixel memory
	for(unsigned int y=0; y < nYRes; y++) {
		for(unsigned int x=0; x < nXRes; x++) {
			char value = targetImage->imageData[y*srcImage->widthStep+x];
			pTargetLabels[y*nXRes+x] = (value != 0) ? 0xFF : 0;
		}
	}

	return pTargetLabels;
}

// Check the kernel sized maskSize*maskSize around point p for green
// color. If the red amount is high enough (>minPercent) true is returned.
static bool checkKernelForRed(
		const XnLabel* labels,
		const TextureData& texData,
		XnPoint3D p,
		short maskSize,
		short minPercent)
{
	unsigned char* img = texData.data;
	int red = 0, other = 1;

	for(int i=-(maskSize/2); i < maskSize/2; i++) {
		for(int j=-(maskSize/2); j < maskSize/2; j++) {
			int yoffset = texData.width*3*((int)p.Y+i);

			// FIXME bounds
			if((int)p.X+j < texData.width && (int)p.Y+i < texData.height
				&& labels[yoffset + (int)p.X+j])
			{
				unsigned char *current = &img[yoffset + ((int)p.X+j) * 3];
				other += current[0] + current[1] + current[2];
				red += current[0];
			}
		}
	}

	/*
	printf("%d, %d => %lf (%d)\n", red, other, (double)red/other * 100,
			(double)red/other * 100 > 40);
	*/

	return (double)red/other * 100 > minPercent;
}



// Return true if buffer has reached size limit
static bool CaptureHandMovement(XnUserID player, XnPoint3D projectivePoint) {
	// Add new position to the history buffer
	g_History.push_front(projectivePoint);

	// Keep size of history buffer
	if (g_History.size() > g_nHistorySize) {
		g_History.pop_back();
		return true;
	}
	return false;
}

static bool DetectSwipe(int LineSize, std::list<XnPoint3D> points, bool* fromLeft)
{
    int MinXDelta = 300; // required horizontal distance
    int MaxYDelta = 100; // max mount of vertical variation

    float x1 = (*(points.begin())).X;
    float y1 = (*(points.begin())).Y;
    float x2 = (*(points.end())).X;
    float y2 = (*(points.end())).Y;

#if 0
	cout << "x1: " << x1 << endl;
	cout << "y1: " << y1 << endl;
	cout << "x2: " << x2 << endl;
	cout << "y2: " << y2 << endl;
#endif

    if (abs(x1 - x2) < MinXDelta)
        return false;

    if (y1 - y2 > MaxYDelta)
        return false;

    //for (int i = 1; i < LineSize - 2; i++)
	std::list<XnPoint3D>::const_iterator PositionIterator;

	for(PositionIterator = ++(points.begin());
		PositionIterator != points.end();
		PositionIterator++)
    {
		XnPoint3D point(*PositionIterator);

        if (abs((point.Y - y1)) > MaxYDelta)
            return false;
		continue;
        float result =
            (y2 - y1) * point.X +
            (x2 - x1) * point.Y +
            (x1 * y2 - x2 * y1);

		//cout << "result: " << result << endl;

		// This should be probably the other way around.
		// result > |result| is never true. Negative values
		// (the only way for result to be different than |result|)
		// are never greater than a positive value.
        if (result < abs(result))
        {
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
                             const int y)
{
    static bool initialized=false;
	static IplImage* srcImage;
	static IplImage* targetImage;

    static cv::Mat* imageMat;
    static cv::Mat* targetImageMat;

    XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

    currentBrush.setRadius(brushRadius);
    currentBrush.setSoftness(brushSoftness / 100.0);
    currentBrush.createImageShape();


    if(!initialized) {
		//srcImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 3);
		//targetImage = cvCreateImage(cvSize(nXRes, nYRes), 8, 3);

        srcImage = getBackgroundImage();

        //int depth = srcImage->depth;
        //int channels = srcImage->nChannels;

        //targetImage = cvCreateImage(cvSize(nXRes, nYRes), depth, channels);

        //srcImage->imageData = (char*) sceneTextureData.data;
        //imageMat = new cv::Mat(srcImage);

        //targetImage->imageData = (char*) sceneTextureData.data;
        //targetImageMat = new cv::Mat(targetImage);

		initialized = true;

        //cv::medianBlur(*imageMat, *targetImageMat, 11);


        //cvReleaseImage(&g_pBgImg);
        //g_pBgImg = targetImage;
	} else {
        currentBrush.paint(srcImage, cvPoint(x, y));
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

		const XnLabel* pLabels = SmoothenUserPixels(sceneTextureData, pOrgLabels);

		ProcessRealWorldImage(sceneTextureData, pImage, pRealWorldImage);

		// Prepare the texture map
		for (unsigned int nY=0; nY < nYRes; nY++)
		{
			for (unsigned int nX=0; nX < nXRes; nX++)
			{
				XnLabel label = *pLabels;

				if(label) {
					// Player detected, use player image
					int offset = nY * nImdXRes * 3 + nX * 3;

					pDestImage[0] = pRealWorldImage[offset + 0] & 0xF0;
					pDestImage[1] = pRealWorldImage[offset + 1] & 0xF0;
					pDestImage[2] = pRealWorldImage[offset + 2] & 0xF0;
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


	DrawUserLabels(player);

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
			bool isRedLeft = false, isRedRight = false;

			g_DepthGenerator.ConvertRealWorldToProjective(2,points,points);

			isRedLeft = checkKernelForRed(pOrgLabels, sceneTextureData, points[0], 15, 60);
			isRedRight = checkKernelForRed(pOrgLabels, sceneTextureData, points[1], 15, 60);

			sprintf(positionString, "red=%d", isRedRight);
			glColor4f(1,1,1,1);
			glRasterPos2i(points[1].X, points[1].Y);
			glPrintString(GLUT_BITMAP_HELVETICA_18, positionString);

			sprintf(positionString, "red=%d", isRedLeft);
			glRasterPos2i(points[0].X, points[0].Y);
			glPrintString(GLUT_BITMAP_HELVETICA_18, positionString);

			// Swipe detection to change background
			{
				bool enoughPoints = CaptureHandMovement(player, points[1]);

				if(enoughPoints) {
					XnUInt32 nPoints = 0;
					XnUInt32 i = 0;

					// Go over all previous positions of current hand
					std::list<XnPoint3D>::const_iterator PositionIterator;
					for (PositionIterator = g_History.begin();
						PositionIterator != g_History.end();
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
					bool swiped = DetectSwipe(g_nHistorySize, g_History, &fromLeft);

					if(swiped) {
						glColor4f(0.5,0.5,1,1);
					} else {
						glColor4f(1,1,1,1);
					}


					glPointSize(2);
					glVertexPointer(3, GL_FLOAT, 0, g_pfPositionBuffer);
					glDrawArrays(GL_LINE_STRIP, 0, i);

					glPointSize(8);
					glDrawArrays(GL_POINTS, 0, 1);
					glFlush();

					//g_History.clear();
				}
			}

            SmudgeAtPosition(sceneTextureData, points[1].X, points[1].Y);
		}


	}

	// Draw skeleton of user
	if (player != 0)
	{
		DrawPlayerSkeleton(player);
	}
}


void DrawScene(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd,
		const xn::ImageMetaData& imd, XnUserID player) {

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

	DrawPlayer(sceneTextureData, smd, imd, player);
}


