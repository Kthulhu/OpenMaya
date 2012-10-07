#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MStringArray.h>
#include <maya/MString.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MSelectionList.h>

#include "utilities/tools.h"
#include "utilities/attrTools.h"
#include "utilities/pystring.h"


void rowToColumn(MMatrix& from, MMatrix& to)
{
	for( int i = 0; i < 4; i++)
		for( int k = 0; k < 4; k++)
			to[k][i] = from[i][k];
}

// replace :,| and . by _ ... the modern version
MString makeGoodString(MString& oldString)
{
	std::string old(oldString.asChar());
	std::string newString;
	newString = pystring::replace(old, ":", "__");
	newString = pystring::replace(newString, "|", "_");
	newString = pystring::replace(newString, ".", "_");
	return MString(newString.c_str());
}

// from bla|blubb|dingensShape make /bla/blubb/dingensShape
MString makeHierarchyString(MString& oldString)
{
	std::string old(oldString.asChar());
	std::string newString;
	newString = pystring::replace(old, "|", "/");
	newString = pystring::replace(newString, ":", "_");
	if( !pystring::startswith(newString, "/") )
		newString = std::string("/") + newString;
	return MString(newString.c_str());
}
MString makeGoodHierarchyString(MString& oldString)
{
	MString returnString, workString = oldString;
	MStringArray stringArray;
	workString.split('|', stringArray);
	if( stringArray.length() == 0)
		workString = oldString;
	else
		workString.clear();
	for( uint i = 0; i < stringArray.length(); i++)
	{
		if( i > 0)
			workString += "/" + stringArray[i];
		else{
			workString = "/";
			workString += stringArray[i];
		}
	}
	return workString;
}

MString getPlugName(MString& longPlugname)
{
	MStringArray stringArray;
	longPlugname.split('.', stringArray);
	if( stringArray.length() > 0)
		return(stringArray[stringArray.length() - 1]);
	else
		return(MString(""));
}

bool checkDirectory( MString& path)
{
	// check if directory already exists 0 -> exists, != 0 -> Problem
	if( _access(path.asChar(), 0) != 0 )
	{
		MGlobal::displayInfo( "Directory " + path + " seems not to exist, trying to create" );
		if( _mkdir( path.asChar() ) != 0 )
		{
			MGlobal::displayInfo( "Problem creating directory " + path );
			return false;
		}
		MGlobal::displayInfo( "Directory " + path + " successfully created." );
	}
	return true;
}

bool IsPathVisible( MDagPath& dagPath )
{
   MStatus stat = MStatus::kSuccess;
   while (stat == MStatus::kSuccess)
   {
      MFnDagNode node(dagPath.node());
      if (!IsVisible(node))
	  {
		 //MGlobal::displayInfo("IsPathVisible::Object " + node.name()+" is NOT visible");
         return false;
	  }else{
		  //MGlobal::displayInfo("IsPathVisible::Object " + node.name()+" IS visible");
	  }
      stat = dagPath.pop();
   }
   return true;
}

bool IsVisible(MFnDagNode& node)
{
	MStatus stat;

	//MGlobal::displayInfo(MString("IsVisible:Check dag node ") + node.name()  +" for visibility");
	if (node.isIntermediateObject())
		return false;

	bool visibility = true;
	MFnDependencyNode depFn(node.object());
	getBool(MString("visibility"), depFn, visibility);
	if( !visibility)
		return false;
	getBool(MString("overrideVisibility"), depFn, visibility);
	if( !visibility)
		return false;

	return true;
}

bool IsTemplated(MFnDagNode& node)
{
   MStatus status;

   MFnDependencyNode depFn(node.object());
   bool isTemplate = false;
   getBool(MString("template"), depFn, isTemplate);
   if( isTemplate )
	   return true;
   int intTempl = 0;
   getInt(MString("overrideDisplayType"), depFn, intTempl);
   if( intTempl == 1 )
	   return true;

   return false;
}

bool IsInRenderLayer(MDagPath& dagPath)
{
   MObject renderLayerObj = MFnRenderLayer::currentLayer();

   MFnRenderLayer curLayer(renderLayerObj);

   bool isInRenderLayer = curLayer.inCurrentRenderLayer(dagPath);

   if (isInRenderLayer)
      return true;
   else
      return false;
}

bool CheckVisibility( MDagPath& dagPath )
{
	MFnDagNode node(dagPath);
	//if( !IsVisible( node ))
	//	return false;
	if( !IsPathVisible( dagPath ))
		return false;
	//if( IsTemplated( node ) )
	//	return false;
	return true;
}

MString matrixToString(MMatrix& matrix)
{
	MString matString("");
	for( int i = 0; i < 4; i++)
		for( int j = 0; j < 4; j++)
		{
			matString += MString(" ") + matrix[i][j];
		}
	return matString;
}

MObject getOtherSideNode(MString& plugName, MObject& thisObject)
{
	MStatus stat;
	MObject result = MObject::kNullObj;
	MFnDependencyNode depFn(thisObject, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlug plug = depFn.findPlug(plugName, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlugArray plugArray;
	plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return result;
	if( plugArray.length() == 0)
		return result;
	MPlug otherSidePlug = plugArray[0];
	result = otherSidePlug.node();
	return result;
}

MObject getOtherSideNode(MString& plugName, MObject& thisObject, MStringArray& otherSidePlugNames)
{
	MStatus stat;
	MObject result = MObject::kNullObj;
	MFnDependencyNode depFn(thisObject, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlug plug = depFn.findPlug(plugName, &stat);	if( stat != MStatus::kSuccess)return result;
	if( !plug.isConnected() )
	{
		int numChildConnects = plug.numConnectedChildren();
		if( numChildConnects == 0)
			return result;
		else{
			for( int i = 0; i < numChildConnects; i++)
			{
				MPlug child = plug.child(i);
				MString otherSidePlugName;
				MObject childObj = getOtherSideNode(child.partialName(false), thisObject, otherSidePlugName);
				if( childObj != MObject::kNullObj)
				{
					otherSidePlugNames.append(otherSidePlugName);
					result = childObj;
				}else
					otherSidePlugNames.append(MString(""));
			}
		}	
	}else{
		MPlugArray plugArray;
		plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return result;
		if( plugArray.length() == 0)
			return result;
		MPlug otherSidePlug = plugArray[0];
		result = otherSidePlug.node();
		otherSidePlugNames.append(otherSidePlug.name());
	}
	return result;
}

void getConnectedNodes(MObject& thisObject, MObjectArray& nodeList)
{
	MFnDependencyNode depFn(thisObject);
	MPlugArray connectedPlugs;
	depFn.getConnections(connectedPlugs);
	int numConnections = connectedPlugs.length();
	
	for( int i = 0; i <  numConnections; i++)
	{
		// check for incoming connections only. Outgoing connections are not relevant
		MPlug plug = connectedPlugs[i];
		// an plug can be source AND destination at the same time, like the displacement attribute of a displacementShader
		if( plug.isSource() && !plug.isDestination())
			continue;
		MObject plugObject = getOtherSideNode(plug);
		if( plugObject != MObject::kNullObj)
			nodeList.append(plugObject);
	}
	//return (numConnections > 0);
}

MObject getOtherSideNode(MPlug& plug)
{
	MStatus stat;
	MObject result = MObject::kNullObj;

	MPlugArray plugArray;
	plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return result;
	if( plugArray.length() == 0)
		return result;
	MPlug otherSidePlug = plugArray[0];
	result = otherSidePlug.node();
	return result;
}

MObject getOtherSideNode(MString& plugName, MObject& thisObject, MString& otherSidePlugName)
{
	MStatus stat;
	MObject result = MObject::kNullObj;
	MFnDependencyNode depFn(thisObject, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlug plug = depFn.findPlug(plugName, &stat);	if( stat != MStatus::kSuccess)return result;
	if( !plug.isConnected() )
		return result;
	MPlugArray plugArray;
	plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return result;
	if( plugArray.length() == 0)
		return result;
	MPlug otherSidePlug = plugArray[0];
	result = otherSidePlug.node();
	otherSidePlugName = otherSidePlug.name();
	otherSidePlugName = otherSidePlug.partialName(false, false, false, false, false, true);
	return result;
}

bool getConnectedPlugs(MString& plugName, MObject& thisObject, MPlug& inPlug, MPlug& outPlug)
{
	MStatus stat;
	bool result = false;
	MFnDependencyNode depFn(thisObject, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlug plug = depFn.findPlug(plugName, &stat);	if( stat != MStatus::kSuccess) return result;
	MPlugArray plugArray;
	plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return result;
	if( plugArray.length() == 0)
		return result;
	MPlug otherSidePlug = plugArray[0];
	inPlug = plug;
	outPlug = otherSidePlug;
	return true;
}

bool getOtherSidePlugName(MString& plugName, MObject& thisObject, MString& otherSidePlugName)
{
	MStatus stat;
	MFnDependencyNode depFn(thisObject, &stat);	if( stat != MStatus::kSuccess) return false;
	MPlug plug = depFn.findPlug(plugName, &stat);	if( stat != MStatus::kSuccess) return false;
	MPlugArray plugArray;
	plug.connectedTo(plugArray, 1, 0, &stat);if( stat != MStatus::kSuccess) return false;
	if( plugArray.length() == 0)
		return false;
	MPlug otherSidePlug = plugArray[0];
	otherSidePlugName = otherSidePlug.name();
	return true;
}

MString getObjectName(MObject& mobject)
{
	MFnDependencyNode depFn(mobject);
	return depFn.name();
}

MString pointToUnderscore(MString& inString)
{
	std::string ss(inString.asChar());
	ss = pystring::replace(ss, ".", "_");
	return MString(ss.c_str());
}

void writeTMatrixList( std::ofstream& outFile, std::vector<MMatrix>& transformMatrices, bool inverse, float scaleFactor)
{
	for( int matrixId = 0; matrixId < transformMatrices.size(); matrixId++)
	{
		MMatrix tm = transformMatrices[matrixId];
		if( inverse )
			tm = tm.inverse();

		if( matrixId == 0)
		{
			outFile << "\t\tray_transform " << matrixToString(tm) << "\n"; // normal transform
		}
		else{
			outFile << "\t\tray_mtransform " << matrixToString(tm) << "\n"; // motion transform
		}
	}
}

void writeTMatrixList( std::ofstream *outFile, std::vector<MMatrix>& transformMatrices, bool inverse, float scaleFactor)
{
	for( int matrixId = 0; matrixId < transformMatrices.size(); matrixId++)
	{
		MMatrix tm = transformMatrices[matrixId];
		if( inverse )
			tm = tm.inverse();

		if( matrixId == 0)
		{
			*outFile << "\t\tray_transform " << matrixToString(tm) << "\n"; // normal transform
		}
		else{
			*outFile << "\t\tray_mtransform " << matrixToString(tm) << "\n"; // motion transform
		}
	}
}

MString lightColorAsString(MFnDependencyNode& depFn)
{
	float intensity = 1.0;
	MColor color(0,0,1);
	getFloat(MString("intensity"), depFn, intensity);
	getColor(MString("color"), depFn, color);
	MString colorString = MString("") + color.r * intensity + " " + color.g * intensity + " " + color.b * intensity + " ";
	return colorString;
}
float shadowColorIntensity(MFnDependencyNode& depFn)
{
	MColor shadowColor;
	getColor(MString("shadColor"), depFn, shadowColor);
	float shadowI = 1.0f - ((shadowColor.r + shadowColor.g + shadowColor.b) / 3.0f); 
	return shadowI;
}

MObject objectFromName(MString& name)
{
    MObject obj;
    MStatus stat;
    MSelectionList list;
    
    // Attempt to add the given name to the selection list,
    // then get the corresponding dependency node handle.
    if (!list.add(name) ||
        !list.getDependNode(0, obj))
    {
        // Failed.
        stat = MStatus::kInvalidParameter;
        return obj;
    }

    // Successful.
    stat = MStatus::kSuccess;
    return obj;
}