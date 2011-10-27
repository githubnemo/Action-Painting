# Tracking

The following is an attempt to document the PointViewer example
code of the NITE package.


## Setup

The code as found in main.cpp:

	// Create NITE objects
	g_pSessionManager = new XnVSessionManager;
	rc = g_pSessionManager->Initialize(&g_Context, "Click,Wave", "RaiseHand");
	CHECK_RC(rc, "SessionManager::Initialize");

	g_pSessionManager->RegisterSession(NULL, SessionStarting, SessionEnding, FocusProgress);

	g_pDrawer = new XnVPointDrawer(20, g_DepthGenerator);
	g_pFlowRouter = new XnVFlowRouter;
	g_pFlowRouter->SetActive(g_pDrawer);

	g_pSessionManager->AddListener(g_pFlowRouter);

	g_pDrawer->RegisterNoPoints(NULL, NoHands);

`g_pDrawer` is our class defining the event handlers. Which handlers
are used is described in the [Events](#Events) section.

By adding `g_pDrawer` to the FlowRouter and adding the FlowRouter
to the session, we connect the callbacks.

As you may have noticed, there are several strings supplied to the
session manager upon initialization. Those are the gestures to
track. The session manager will emit start events whenever those
gestures are recognized. (TODO: verify)


## Events

	void XnVPointDrawer::OnPointUpdate(const XnVHandPointContext* cxt)
	{
		// positions are kept in projective coordinates, since they are only used for drawing
		XnPoint3D ptProjective(cxt->ptPosition);

		m_DepthGenerator.ConvertRealWorldToProjective(1, &ptProjective,
				&ptProjective);

		m_History[cxt->nID].push_front(ptProjective);
		if (m_History[cxt->nID].size() > m_nHistorySize)
			m_History[cxt->nID].pop_back();
	}

As one can see here, the point update event offers us a HandPointContext
which can tell us the position of the hand point. We can map these
X, Y and Z coordinates to our projection (X and Y only) by using
`ConvertRealWorldToProjective`.

The `cxt->nID` is used to determine which hand was used. So in this
code we track the point history of each hand.

There are other events as well:

* OnPointCreate
* OnPointDestroy


## FOV Edge Touching

To track the FOV (Field Of View) edges, we can use the hand touching
capability:

	if (g_HandsGenerator.IsCapabilitySupported(XN_CAPABILITY_HAND_TOUCHING_FOV_EDGE))
	{
		g_HandsGenerator.GetHandTouchingFOVEdgeCap().RegisterToHandTouchingFOVEdge(TouchingCallback, NULL, h);
	}

The callback looks like this:

	void XN_CALLBACK_TYPE TouchingCallback(xn::HandTouchingFOVEdgeCapability& generator, XnUserID id, const XnPoint3D* pPosition, XnFloat fTime, XnDirection eDir, void* pCookie)
	{
			g_pDrawer->SetTouchingFOVEdge(id);
	}

The `XnUserId` is the id of the user, touching the edge.
The function `SetTouchingFOVEdge` called, stores the id in a vector.

	void XnVPointDrawer::SetTouchingFOVEdge(XnUInt32 nID)
	{
		m_TouchingFOVEdge.push_front(nID);
	}


In the example code this is used to color the tracked line
drawn with the hand if it touches the FOV edge.

Executing code:

	std::map<XnUInt32, std::list<XnPoint3D> >::const_iterator PointIterator;

	// Go over each existing hand
	for (PointIterator = m_History.begin();
		PointIterator != m_History.end();
		++PointIterator)
	{
		XnUInt32 Id = PointIterator->first;
		...

		if(IsTouching(Id)) {
			glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
		}
		...
	}

`m_History` is our point history vector, the `Id` is the `XnUserId`.
`IsTouching` just returns if the FOV points for the given Id was found.


# Drawing

The Handgenerator/tracker emits an event whenever the
state of the tracked hand changes. This can be used to
append points to a queue and draw these points using
OpenGL.

## OpenGL

One can use the following functions for drawing the points
tracked:

	glPointSize(2);
	glVertexPointer(3, GL_FLOAT, 0, m_pfPositionBuffer);
	glDrawArrays(GL_LINE_STRIP, 0, i);

glVertexPointer takes an float array with points

	[(x1,y1,z1),(x2,y2,z3),...]

A point value can be ignored by setting it to 0.

glDrawArrays seems to connect lines supplied by the
glVertexPointer function.

## Smoothing

Smoothing can be accomplished by setting a float value
with `HandsGenerator::SetSmoothing()`.

This seems to trigger the hand tracking more often
and therefore to receive more tracking events, resulting
in more points and therefore a smoother line.


