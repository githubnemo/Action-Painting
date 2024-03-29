#ifndef XNV_POINT_DRAWER_H_
#define XNV_POINT_DRAWER_H_

#include <map>
#include <list>

#include <XnCppWrapper.h>
#include <XnVPointControl.h>
#include <cv.h>

void DrawScene(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd,
	const xn::ImageMetaData& imd);

void SetBackgroundImage(std::list<IplImage*>::const_iterator img);

void DoFadeFromLeft();

void DoFadeFromRight();

extern int g_fadeDirection;
extern bool g_bDrawDebugInfo;

#endif
