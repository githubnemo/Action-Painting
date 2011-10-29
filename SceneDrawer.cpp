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
extern XnUserID g_nPlayer;

IplImage* g_pBgImg;
GLfloat g_pfTexCoords[8];


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


void initBackgroundImage(int width, int height) {
	IplImage* img = cvLoadImage("Data/background.jpg");

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
		initBackgroundImage(nXRes, nYRes);
	}

	// Background image data
	IplImage* pCvBgImage = getBackgroundImage();
	const XnUInt8* pBgImage = (const XnUInt8*)pCvBgImage->imageData;

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
		XnUInt8* pImage)
{
	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;
	int texWidth = sceneTextureData.width;

	for(int nY=0; nY < nYRes; nY++) {
		for(int nX=0; nX < nXRes; nX++) {
			pImage[0] = 0;
			pImage += 3;
		}
	}
}


inline void DrawPlayer(
		const TextureData& sceneTextureData,
		const xn::SceneMetaData& smd,
		const xn::ImageMetaData& imd, XnUserID player)
{
	static bool bInitialized = false;
	static XnUInt8* pRealWorldImage;

	if(!bInitialized) {
		pRealWorldImage = new XnUInt8[smd.XRes()*smd.YRes()*3];
		bInitialized = true;
	}

	XnUInt16 nXRes = sceneTextureData.XRes;
	XnUInt16 nYRes = sceneTextureData.YRes;

	unsigned char* pDepthTexBuf = sceneTextureData.data;
	GLuint depthTexID = sceneTextureData.id;

	int texWidth = sceneTextureData.width;
	int texHeight = sceneTextureData.height;

	float texXpos = sceneTextureData.XPos;
	float texYpos = sceneTextureData.YPos;

	unsigned char* pDestImage = pDepthTexBuf;
	const XnLabel* pLabels = smd.Data();

	{
		// Real world image data
		const XnUInt8* pImage = imd.Data();
		unsigned int nImdXRes = imd.XRes();

		memcpy(pRealWorldImage, pImage, imd.XRes()*imd.YRes()*3);

		ProcessRealWorldImage(sceneTextureData, pRealWorldImage);

		// Prepare the texture map
		for (unsigned int nY=0; nY < nYRes; nY++)
		{
			for (unsigned int nX=0; nX < nXRes; nX++)
			{
				XnLabel label = *pLabels;

				if(label != 0) {
					// Player detected, use player image
					int offset = nY * nImdXRes * 3 + nX * 3;

					pDestImage[0] = pRealWorldImage[offset + 0];
					pDestImage[1] = pRealWorldImage[offset + 1];
					pDestImage[2] = pRealWorldImage[offset + 2];
				}

				pLabels++;
				pDestImage += 3;
			}

			pDestImage += (texWidth - nXRes) *3;
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

		bInitialized = true;
	}

	DrawBackground(sceneTextureData);

	DrawPlayer(sceneTextureData, smd, imd, player);
}



/* SceneDrawer definitions
 *
 * The scene drawer's Update method is called each frame.
 * The rendering process is started from there.
 */

SceneDrawer::SceneDrawer(XnUInt32 nHistory) :
	m_nHistorySize(nHistory)
{
	m_pfPositionBuffer = new XnFloat[nHistory*3];
}


// Destructor. Clear all data structures
SceneDrawer::~SceneDrawer()
{
	std::map<XnUInt32, std::list<XnPoint3D> >::iterator iter;
	for (iter = m_History.begin(); iter != m_History.end(); ++iter)
	{
		iter->second.clear();
	}
	m_History.clear();

	delete []m_pfPositionBuffer;
}


void SceneDrawer::Update(XnVMessage* pMessage) {
	// PointControl's Update calls all callbacks for each hand
	XnVPointControl::Update(pMessage);

	DrawHands();
}


void SceneDrawer::OnPointCreate(const XnVHandPointContext* cxt)
{
	// Create entry for the hand
	m_History[cxt->nID].clear();
	OnPointUpdate(cxt);
}


// Handle new position of an existing hand
void SceneDrawer::OnPointUpdate(const XnVHandPointContext* cxt)
{
	// positions are kept in projective coordinates, since they are only used for drawing
	XnPoint3D ptProjective(cxt->ptPosition);

	g_DepthGenerator.ConvertRealWorldToProjective(1, &ptProjective, &ptProjective);

	// Add new position to the history buffer
	m_History[cxt->nID].push_front(ptProjective);

	// Keep size of history buffer
	if (m_History[cxt->nID].size() > m_nHistorySize)
		m_History[cxt->nID].pop_back();
}


// Handle destruction of an existing hand
void SceneDrawer::OnPointDestroy(XnUInt32 nID)
{
	// No need for the history buffer
	m_History.erase(nID);
}


void SceneDrawer::DrawHands() const {
	std::map<XnUInt32, std::list<XnPoint3D> >::const_iterator PointIterator;

	// Go over each existing hand
	for (PointIterator = m_History.begin();
		PointIterator != m_History.end();
		++PointIterator)
	{
		// Clear buffer
		XnUInt32 nPoints = 0;
		XnUInt32 i = 0;
		XnUInt32 Id = PointIterator->first;

		// Go over all previous positions of current hand
		std::list<XnPoint3D>::const_iterator PositionIterator;
		for (PositionIterator = PointIterator->second.begin();
			PositionIterator != PointIterator->second.end();
			++PositionIterator, ++i)
		{
			// Add position to buffer
			XnPoint3D pt(*PositionIterator);
			m_pfPositionBuffer[3*i] = pt.X;
			m_pfPositionBuffer[3*i + 1] = pt.Y;
			m_pfPositionBuffer[3*i + 2] = 0;//pt.Z();
		}

		// Set color
		XnUInt32 nColor = Id % nColors;
		XnUInt32 nSingle = GetPrimaryID();
		if (Id == GetPrimaryID())
			nColor = 6;
		// Draw buffer:
		glColor4f(Colors[nColor][0],
				Colors[nColor][1],
				Colors[nColor][2],
				1.0f);
		glPointSize(2);
		glVertexPointer(3, GL_FLOAT, 0, m_pfPositionBuffer);
		glDrawArrays(GL_LINE_STRIP, 0, i);


		glPointSize(8);
		glDrawArrays(GL_POINTS, 0, 1);
		glFlush();
	}
}
