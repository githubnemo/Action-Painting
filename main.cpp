/*******************************************************************************
*                                                                              *
*   PrimeSense NITE 1.3 - Players Sample                                       *
*   Copyright (C) 2010 PrimeSense Ltd.                                         *
*                                                                              *
*******************************************************************************/

#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>
#include "SceneDrawer.h"


#include <XnVNite.h>
#include <XnVPointControl.h>
#include <XnVHandPointContext.h>

#include <iostream>
#include <fstream>	// open background images config

#include <cv.h>		 // Loading background images
#include <highgui.h> // Debug
#include <unistd.h>  // access(2)

xn::Context g_Context;
xn::ScriptNode g_ScriptNode;
xn::DepthGenerator g_DepthGenerator;
xn::ImageGenerator g_ImageGenerator;
xn::UserGenerator g_UserGenerator;
xn::GestureGenerator g_GestureGenerator;
xn::SkeletonCapability* g_SkeletonCap;

// NITE specifics
XnVSessionManager* g_pSessionManager;
XnVFlowRouter* g_pFlowRouter;

XnUserID g_nPlayer = 0; // XXX deprecated
XnBool g_bCalibrated = FALSE;
XnBool g_bNeedPose = FALSE;
XnChar g_strPose[20] = "";

std::list<IplImage*> g_backgroundImages;


#ifdef USE_GLUT
#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
        #include <GLUT/glut.h>
#else
        #include <GL/glut.h>
#endif
#endif

#ifndef USE_GLUT
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;
#endif

#define GL_WIN_SIZE_X 960
#define GL_WIN_SIZE_Y 720

XnBool g_bPause = false;
XnBool g_bRecord = false;
XnBool g_bQuit = false;


#define SAMPLE_XML_PATH "./Data/Sample-Players.xml"
#define CALIBRATION_PATH "./Data/calibration.dat"
#define IMAGES_CFG_PATH	"./Data/backgrounds.cfg"


#define CHECK_RC(rc, what)											\
	if (rc != XN_STATUS_OK)											\
	{																\
		printf("%s failed: %s\n", what, xnGetStatusString(rc));		\
		return rc;													\
	}

#define CHECK_ERRORS(rc, errors, what)		\
	if (rc == XN_STATUS_NO_NODE_PRESENT)	\
{										\
	XnChar strError[1024];				\
	errors.ToString(strError, 1024);	\
	printf("%s\n", strError);			\
	return (rc);						\
}


void CleanupExit()
{
	exit (1);
}


void XN_CALLBACK_TYPE NewUser(xn::UserGenerator& generator, XnUserID user, void* pCookie)
{
	printf("New User %d\n", user);

	// New user found
	if (g_bNeedPose)
	{
		g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, user);
	}
	else
	{
		g_UserGenerator.GetSkeletonCap().RequestCalibration(user, TRUE);
	}
}


void XN_CALLBACK_TYPE LostUser(xn::UserGenerator& generator, XnUserID user, void* pCookie)
{
	printf("Lost user %d\n", user);
}


void XN_CALLBACK_TYPE PoseDetected(xn::PoseDetectionCapability& pose,
		const XnChar* strPose, XnUserID user, void* cxt)
{
	printf("Found pose \"%s\" for user %d\n", strPose, user);
	g_UserGenerator.GetPoseDetectionCap().StopPoseDetection(user);
	g_UserGenerator.GetSkeletonCap().RequestCalibration(user, TRUE);
}


void XN_CALLBACK_TYPE CalibrationStarted(xn::SkeletonCapability& skeleton,
		XnUserID user, void* cxt)
{
	printf("Calibration started\n");
}


void XN_CALLBACK_TYPE CalibrationCompleted(xn::SkeletonCapability& skeleton,
		XnUserID user, XnCalibrationStatus eStatus, void* cxt)
{
	printf("Calibration done [%d] %ssuccessfully\n",
			user, (eStatus == XN_CALIBRATION_STATUS_OK)?"":"un");

	if (eStatus == XN_CALIBRATION_STATUS_OK)
	{
		// Calibration succeeded
		printf("Calibration complete, start tracking user %d\n", user);
		g_UserGenerator.GetSkeletonCap().StartTracking(user);
	}
	else
	{
		// Calibration failed
		printf("Calibration failed for user %d\n", user);
        if(eStatus==XN_CALIBRATION_STATUS_MANUAL_ABORT)
        {
            printf("Manual abort occured, stop attempting to calibrate!");
            return;
        }

		if (g_bNeedPose)
		{
			g_UserGenerator.GetPoseDetectionCap().StartPoseDetection(g_strPose, user);
		}
		else
		{
			g_UserGenerator.GetSkeletonCap().RequestCalibration(user, TRUE);
		}
	}
}


// this function is called each frame
void glutDisplay (void)
{

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup the OpenGL viewpoint
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();


	XnMapOutputMode mode;
	g_DepthGenerator.GetMapOutputMode(mode);
	glOrtho(0, mode.nXRes, mode.nYRes, 0, -1.0, 1.0);


	glDisable(GL_TEXTURE_2D);

	if (!g_bPause)
	{
		// Read next available data
		g_Context.WaitOneUpdateAll(g_DepthGenerator);

		{
			xn::SceneMetaData sceneMD;
			xn::DepthMetaData depthMD;
			xn::ImageMetaData imageMD;

			g_DepthGenerator.GetMetaData(depthMD);
			g_UserGenerator.GetUserPixels(g_nPlayer, sceneMD);
			g_ImageGenerator.GetMetaData(imageMD);

			DrawScene(depthMD, sceneMD, imageMD);
		}
	}

	/*
	if (g_nPlayer != 0)
	{
		XnPoint3D com;
		g_UserGenerator.GetCoM(g_nPlayer, com);
		if (com.Z == 0)
		{
			g_nPlayer = 0;
			FindPlayer();
		}
	}
	*/

	#ifdef USE_GLUT
	glutSwapBuffers();
	#endif
}


#ifdef USE_GLUT
void glutIdle (void)
{
	if (g_bQuit) {
		CleanupExit();
	}

	// Display the frame
	glutPostRedisplay();
}


void glutKeyboard (unsigned char key, int x, int y)
{
	static short fullscreen = 0;

	switch (key)
	{
	case 27:
		CleanupExit();
	case 'f':
		if(fullscreen) {
			glutReshapeWindow(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
			fullscreen = 0;
		} else {
			glutFullScreen();
			fullscreen = 1;
		}
		break;
	case 's':
		g_bDrawDebugInfo = !g_bDrawDebugInfo;
		break;
	case 'l':
		DoFadeFromLeft();
		break;
	case 'r':
		DoFadeFromRight();
		break;
	case'p':
		g_bPause = !g_bPause;
		break;
	}
}


void glInit (int * pargc, char ** argv)
{
	glutInit(pargc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow ("PrimeSense Nite Players Viewer");
	glutSetCursor(GLUT_CURSOR_NONE);

	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}
#endif


// Debug output callbacks for hand generator

void XN_CALLBACK_TYPE GestureIntermediateStageCompletedHandler(
		xn::GestureGenerator& generator, const XnChar* strGesture,
		const XnPoint3D* pPosition, void* pCookie)
{
	printf("Gesture %s: Intermediate stage complete (%f,%f,%f)\n",
			strGesture, pPosition->X, pPosition->Y, pPosition->Z);
}

void XN_CALLBACK_TYPE GestureReadyForNextIntermediateStageHandler(
		xn::GestureGenerator& generator, const XnChar* strGesture,
		const XnPoint3D* pPosition, void* pCookie)
{
	printf("Gesture %s: Ready for next intermediate stage (%f,%f,%f)\n",
			strGesture, pPosition->X, pPosition->Y, pPosition->Z);
}

void XN_CALLBACK_TYPE GestureProgressHandler(
		xn::GestureGenerator& generator, const XnChar* strGesture,
		const XnPoint3D* pPosition, XnFloat fProgress, void* pCookie)
{
	printf("Gesture %s progress: %f (%f,%f,%f)\n",
			strGesture, fProgress, pPosition->X, pPosition->Y, pPosition->Z);
}


bool loadBackgroundImages(const char* path) {
	std::string line;
	std::ifstream myfile(path);

	const int width = 640;
	const int height = 480;

	if (myfile.is_open())
	{
		while ( myfile.good() )
		{
			std::getline (myfile,line);

			if(line.empty())
				continue;

			IplImage* img = cvLoadImage(line.c_str());

			if(img == NULL) {
				printf("Failed to load image '%s'.\n", line.c_str());
				continue;
			}

			// Resize background to width x height
			{
				int depth = img->depth;
				int channels = img->nChannels;
				IplImage* resizedImage = cvCreateImage(
						cv::Size(width,height), depth, channels);

				cvResize(img, resizedImage, CV_INTER_LINEAR);

				cvReleaseImage(&img);
				img = resizedImage;
			}

			g_backgroundImages.push_front(img);
		}
		myfile.close();
		return true;
	} else {
		printf("Could not open file %s.\n", path);
		return false;
	}
}


int main(int argc, char **argv)
{
	if(!loadBackgroundImages(IMAGES_CFG_PATH)) {
		printf("Failed to load image paths "IMAGES_CFG_PATH"\n");
		return XN_STATUS_ERROR;
	}

	if(g_backgroundImages.empty()) {
		puts("No background images loaded!");
		return XN_STATUS_ERROR;
	}

	XnStatus rc = XN_STATUS_OK;
	xn::EnumerationErrors errors;

	rc = g_Context.InitFromXmlFile(SAMPLE_XML_PATH, g_ScriptNode, &errors);
	CHECK_ERRORS(rc, errors, "InitFromXmlFile");
	CHECK_RC(rc, "InitFromXml");

	rc = g_Context.FindExistingNode(XN_NODE_TYPE_DEPTH, g_DepthGenerator);
	CHECK_RC(rc, "Find depth generator");
	rc = g_Context.FindExistingNode(XN_NODE_TYPE_USER, g_UserGenerator);
	CHECK_RC(rc, "Find user generator");
	rc = g_Context.FindExistingNode(XN_NODE_TYPE_IMAGE, g_ImageGenerator);
	CHECK_RC(rc, "Find image generator");
	rc = g_Context.FindExistingNode(XN_NODE_TYPE_GESTURE, g_GestureGenerator);
	CHECK_RC(rc, "Find gesture generator");


	if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON) ||
		!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
	{
		printf("User generator doesn't support either skeleton or pose detection.\n");
		return XN_STATUS_ERROR;
	}

	g_SkeletonCap = new xn::SkeletonCapability(g_UserGenerator.GetSkeletonCap());
	g_SkeletonCap->SetSkeletonProfile(XN_SKEL_PROFILE_ALL);


	if (g_UserGenerator.GetSkeletonCap().NeedPoseForCalibration())
	{
		g_bNeedPose = TRUE;

		if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
		{
			printf("Pose required, but not supported\n");
			return XN_STATUS_ERROR;
		}
	}



	// Orientate the depth screen at the real world image so there's
	// no offset if we project the user's image to the depth map.
	XnStatus s = g_DepthGenerator.GetAlternativeViewPointCap().SetViewPoint(g_ImageGenerator);


	// Debug callbacks for gesture recognition
	XnCallbackHandle hGestureIntermediateStageCompleted, hGestureProgress,
					 hGestureReadyForNextIntermediateStage;

	g_GestureGenerator.RegisterToGestureIntermediateStageCompleted(
			GestureIntermediateStageCompletedHandler, NULL,
			hGestureIntermediateStageCompleted);

	g_GestureGenerator.RegisterToGestureReadyForNextIntermediateStage(
			GestureReadyForNextIntermediateStageHandler, NULL,
			hGestureReadyForNextIntermediateStage);

	g_GestureGenerator.RegisterGestureCallbacks(
			NULL, GestureProgressHandler, NULL, hGestureProgress);



	// User callbacks
	XnCallbackHandle hUserCBs, hCalibrationStartCB, hCalibrationCompleteCB, hPoseCBs;

	g_UserGenerator.RegisterUserCallbacks(NewUser, LostUser, NULL, hUserCBs);

	// Skeleton callbacks
	rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationStart(
			CalibrationStarted, NULL, hCalibrationStartCB);
	CHECK_RC(rc, "Register to calbiration start");

	rc = g_UserGenerator.GetSkeletonCap().RegisterToCalibrationComplete(
			CalibrationCompleted, NULL, hCalibrationCompleteCB);
	CHECK_RC(rc, "Register to calibration complete");

	rc = g_UserGenerator.GetPoseDetectionCap().RegisterToPoseDetected(
			PoseDetected, NULL, hPoseCBs);
	CHECK_RC(rc, "Register to pose detected");


	// Fire up all generators
	rc = g_Context.StartGeneratingAll();
	CHECK_RC(rc, "StartGenerating");


	glInit(&argc, argv);

	/* Debug effect windows
	cvNamedWindow("test image", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("source image", CV_WINDOW_AUTOSIZE);
	*/

	glutMainLoop();
}
