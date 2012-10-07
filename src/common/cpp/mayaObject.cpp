#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlugArray.h>

#include "mayaObject.h"
#include "utilities/logging.h"
#include "utilities/tools.h"
#include "utilities/attrTools.h"

static Logging logger;

bool MayaObject::geometryShapeSupported()
{
	return true;
}

bool MayaObject::shadowMapCastingLight()
{
	if(!this->mobject.hasFn(MFn::kLight))
		return false;
	
	MFnDependencyNode lightFn(this->mobject);

	bool useDepthMapShadows = false;
	if(!getBool(MString("useDepthMapShadows"), lightFn,  useDepthMapShadows))
		return false;
	
	if(!useDepthMapShadows)
		return false;

	return true;
}

void MayaObject::updateObject()
{
	this->transformMatrices.clear();
	this->transformMatrices.push_back(this->dagPath.inclusiveMatrix());
}

MayaObject::MayaObject(MObject& mobject)
{
	this->isInstancerObject = false;
	this->origObject = NULL;
	this->mobject = mobject;
	this->supported = false;
	this->shortName = getObjectName(mobject);
	MFnDagNode dagNode(mobject);
	dagNode.getPath(this->dagPath);
	MString fullPath = dagNode.fullPathName();
	logger.debug(MString("fullpath: ") + fullPath);
	this->fullName = fullPath;
	this->fullNiceName = makeGoodString(fullPath);
	this->transformMatrices.push_back(this->dagPath.inclusiveMatrix());
	this->instanceNumber = 0;
	// check for light connection. If the geo is connected to a areaLight it is not supposed to be rendered
	// but used as light geometry.
	//checkLightConnection();

	//getObjectShadingGroups(this->dagPath);
	MFnDependencyNode depFn(mobject);
	this->motionBlurred = true;
	this->geometryMotionblur = false;

	bool mb = true;
	if( getBool(MString("motionBlur"), depFn, mb) )
		this->motionBlurred = mb;

	this->perObjectMbSteps = 1;
	this->index = -1;
	this->scenePtr = NULL;
	this->shapeConnected = false;
	this->lightExcludeList = true; // In most cases only a few lights are ignored, so the list is shorter with excluded lights
	this->shadowExcludeList = true; // in most cases only a few objects ignore shadows, so the list is shorter with ignoring objects
	this->animated = this->isObjAnimated();
	this->shapeConnected = this->isShapeConnected();
	this->parent = NULL;
	this->visible = true;
}

MayaObject::MayaObject(MDagPath& objPath)
{
	this->isInstancerObject = false;
	this->instancerParticleId = -1;
	this->instanceNumber = 0;
	this->attributes = NULL;
	this->origObject = NULL;
	this->mobject = objPath.node();
	this->supported = false;
	this->shortName = getObjectName(mobject);
	MFnDagNode dagNode(mobject);
	MString fullPath = objPath.fullPathName();
	this->dagPath = objPath;
	this->fullName = fullPath;
	this->fullNiceName = makeGoodString(fullPath);
	this->transformMatrices.push_back(this->dagPath.inclusiveMatrix());
	this->instanceNumber = dagPath.instanceNumber();
	MFnDependencyNode depFn(mobject);
	this->motionBlurred = true;
	this->geometryMotionblur = false;

	bool mb = true;
	if( getBool(MString("motionBlur"), depFn, mb) )
		this->motionBlurred = mb;

	this->perObjectMbSteps = 1;
	this->index = -1;
	this->scenePtr = NULL;
	this->shapeConnected = false;
	this->lightExcludeList = true; // In most cases only a few lights are ignored, so the list is shorter with excluded lights
	this->shadowExcludeList = true; // in most cases only a few objects ignore shadows, so the list is shorter with ignoring objects
	this->animated = this->isObjAnimated();
	this->shapeConnected = this->isShapeConnected();
	this->parent = NULL;
	this->visible = true;
	this->hasInstancerConnection = false;
	MDagPath dp;
	dp = objPath;
	if (!IsVisible(dagNode) || IsTemplated(dagNode) || !IsInRenderLayer(dp) || !IsPathVisible(dp))
		this->visible = false;

	// get instancer connection
	MStatus stat;
	MPlug matrixPlug = depFn.findPlug(MString("matrix"), &stat);
	if( !stat)
		logger.debug(MString("Could not find matrix plug"));
	else{
		MPlugArray outputs;
		if( matrixPlug.isConnected())
		{
			matrixPlug.connectedTo(outputs, false, true);
			for( uint i = 0; i < outputs.length(); i++)
			{
				MObject otherSide = outputs[i].node();
				logger.debug(MString("matrix is connected to ") + getObjectName(otherSide));
				if( otherSide.hasFn(MFn::kInstancer))
				{
					logger.debug(MString("other side is instancer"));
					this->hasInstancerConnection = true;
				}
			}
		}	
	}
}

// to check if an object is animated, we need to check e.g. its transform inputs
bool MayaObject::isObjAnimated()
{
	MStatus stat;
	bool returnValue = false;
	if( this->mobject.hasFn(MFn::kTransform))
	{
		MFnDependencyNode depFn(this->mobject, &stat);
		if(stat)
		{
			MPlugArray connections;
			depFn.getConnections(connections);
			if( connections.length() == 0)
				returnValue = false;
			else{
				for( uint cId = 0; cId < connections.length(); cId++)
				{
					if( connections[cId].isDestination())
					{
						returnValue = true;
					}
				}
			}
			
		}
	}
	return returnValue;
}

// to check if shape is connected, simply test for the common shape inputs, nurbs: create mesh: inMesh
bool MayaObject::isShapeConnected()
{
	MStatus stat;
	bool returnValue = false;
	MPlug inPlug;
	MFnDependencyNode depFn(this->mobject, &stat);
	if(stat)
	{
		MFn::Type type = this->mobject.apiType();
		switch(type)
		{
		case MFn::kMesh:
			inPlug = depFn.findPlug(MString("inMesh"), &stat);
			if( stat)
				if( inPlug.isConnected())
					returnValue = true;
			break;
		case MFn::kNurbsSurface:
			inPlug = depFn.findPlug(MString("create"), &stat);
			if( stat)
				if( inPlug.isConnected())
					returnValue = true;
			break;
		case MFn::kNurbsCurve:
			inPlug = depFn.findPlug(MString("create"), &stat);
			if( stat)
				if( inPlug.isConnected())
					returnValue = true;
			break;
		}
	}

	return returnValue;
}

MayaObject::~MayaObject()
{}