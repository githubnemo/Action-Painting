/*******************************************************************************
*                                                                              *
*   PrimeSense NITE 1.3 - Players Sample                                       *
*   Copyright (C) 2010 PrimeSense Ltd.                                         *
*                                                                              *
*******************************************************************************/

#ifndef XNV_POINT_DRAWER_H_
#define XNV_POINT_DRAWER_H_

#include <map>
#include <list>

#include <XnCppWrapper.h>
#include <XnVPointControl.h>

void DrawScene(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd,
	const xn::ImageMetaData& imd, XnUserID player);

class SceneDrawer : public XnVPointControl {
public:
	SceneDrawer(XnUInt32 nHistory);

	virtual ~SceneDrawer();

	void DrawHands() const;

	/**
	 * Handle a new message.
	 * Calls other callbacks for each point, then draw the depth map (if needed) and the points
	 */
	void Update(XnVMessage* pMessage);
	/**
	 * Handle creation of a new point
	 */
	void OnPointCreate(const XnVHandPointContext* cxt);
	/**
	 * Handle new position of an existing point
	 */
	void OnPointUpdate(const XnVHandPointContext* cxt);
	/**
	 * Handle destruction of an existing point
	 */
	void OnPointDestroy(XnUInt32 nID);

protected:
	std::map<XnUInt32, std::list<XnPoint3D> > m_History;
	XnUInt32 m_nHistorySize;
	XnFloat* m_pfPositionBuffer;

};

#endif
