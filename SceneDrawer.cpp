/*******************************************************************************
*                                                                              *
*   PrimeSense NITE 1.3 - Players Sample                                       *
*   Copyright (C) 2010 PrimeSense Ltd.                                         *
*                                                                              *
*******************************************************************************/

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

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;
extern xn::ImageGenerator g_ImageGenerator;

#define MAX_DEPTH 10000

IplImage* g_pBgImg;


unsigned int getClosestPowerOfTwo(unsigned int n)
{
	unsigned int m = 2;
	while(m < n) m<<=1;

	return m;
}


GLuint initTexture(void** buf, int& width, int& height)
{
	GLuint texID = 0;
	glGenTextures(1,&texID);

	width = getClosestPowerOfTwo(width);
	height = getClosestPowerOfTwo(height);
	*buf = new unsigned char[width*height*4];
	glBindTexture(GL_TEXTURE_2D,texID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return texID;
}


GLfloat texcoords[8];
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

	//TODO: Maybe glFinish needed here instead - if there's some bad graphics crap
	glFlush();
}


void DrawTexture(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

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


void initBackgroundImage(int width, int height) {
	IplImage* img = cvLoadImage("Data/background.jpg");

	int depth = img->depth;
	int channels = img->nChannels;

	IplImage* resizedImage = cvCreateImage(cv::Size(width,height), depth, channels);

	cvResize(img, resizedImage, CV_INTER_LINEAR);

	g_pBgImg = resizedImage;
}


// Return reference to the cv IplImage of the background image
IplImage* getBackgroundImage() {
	return g_pBgImg;
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


void DrawDepthMap(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd,
		const xn::ImageMetaData& imd, XnUserID player)
{
	static bool bInitialized = false;
	static GLuint depthTexID;
	static unsigned char* pDepthTexBuf;
	static int texWidth, texHeight;

	float texXpos;
	float texYpos;

	XnUInt16 nXRes = dmd.XRes();
	XnUInt16 nYRes = dmd.YRes();

	if(!bInitialized)
	{
		texWidth =  getClosestPowerOfTwo(nXRes);
		texHeight = getClosestPowerOfTwo(nYRes);

		// Initialize pDepthTexBuf char*texWidth*texHeight*4
		depthTexID = initTexture((void**)&pDepthTexBuf, texWidth, texHeight) ;

		texXpos =(float)nXRes/texWidth;
		texYpos  =(float)nYRes/texHeight;

		memset(texcoords, 0, 8*sizeof(float));
		texcoords[0] = texXpos,
		texcoords[1] = texYpos,
		texcoords[2] = texXpos,
		texcoords[7] = texYpos;

		initBackgroundImage(nXRes, nYRes);

		bInitialized = true;
	}

	unsigned char* pDestImage = pDepthTexBuf;
	const XnLabel* pLabels = smd.Data();

	// User pixels == filter(smd.Data(), fun(*d) { return *d != 0; })

	{
		// Real world image data
		const XnUInt8* pImage = imd.Data();
		unsigned int nImdXRes = imd.XRes();

		// Background image data
		IplImage* pCvBgImage = getBackgroundImage();
		const XnUInt8* pBgImage = (const XnUInt8*)pCvBgImage->imageData;

		// Prepare the texture map
		for (unsigned int nY=0; nY < nYRes; nY++)
		{
			for (unsigned int nX=0; nX < nXRes; nX++)
			{
				XnLabel label = *pLabels;

				if(label == 0) {
					// Draw background image
					XnUInt8 r,g,b;

					if(nY > pCvBgImage->height || nX > pCvBgImage->width) {
						r = 0; g = 0; b = 0;
					} else {
						r= pBgImage[2];
						g= pBgImage[1];
						b= pBgImage[0];
					}

					pDestImage[0] = r;
					pDestImage[1] = g;
					pDestImage[2] = b;

				} else {
					// Player detected, use player image
					int offset = nY * nImdXRes * 3 + nX * 3;

					pDestImage[0] = pImage[offset + 0];
					pDestImage[1] = pImage[offset + 1];
					pDestImage[2] = pImage[offset + 2];
				}


				pLabels++;
				pBgImage += 3;
				pDestImage += 3;
			}

			pDestImage += (texWidth - nXRes) *3;
			pBgImage += (pCvBgImage->widthStep - nXRes*3);
		}
	}

	glBindTexture(GL_TEXTURE_2D, depthTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0,
				GL_RGB, GL_UNSIGNED_BYTE, pDepthTexBuf);

	// Display the OpenGL texture map
	glColor4f(1,1,1,1);

	glEnable(GL_TEXTURE_2D);
	DrawTexture(nXRes,nYRes,0,0);
	glDisable(GL_TEXTURE_2D);

	DrawUserLabels(player);

	// Draw skeleton of user
	if (player != 0)
	{
		DrawPlayerSkeleton(player);
	}
}
