#include "Indigo.h"
#include <maya/MPoint.h>
#include "maya/MFnMesh.h"
#include "maya/MItMeshPolygon.h"
#include <maya/MPointArray.h>
#include <maya/MMatrix.h>
#include <maya/MIntArray.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItMeshVertex.h>
#include "utilities/logging.h"
#include "utilities/tools.h"
#include "utilities/attrTools.h"

#include "shadingtools/shadingUtils.h"
#include "shadingtools/material.h"
#include "../mtin_common/mtin_mayaObject.h"
#include "mayaScene.h"
#include "world.h"

static Logging logger;

void IndigoRenderer::defineMesh(std::shared_ptr<mtin_MayaObject>  obj)
{
	MObject meshObject = obj->mobject;
	MStatus stat = MStatus::kSuccess;
	MFnMesh meshFn(meshObject, &stat);
	CHECK_MSTATUS(stat);

	MItMeshPolygon faceIt(meshObject, &stat);
	CHECK_MSTATUS(stat);
	MItMeshVertex vertexIt(obj->dagPath, MObject::kNullObj, &stat);
	CHECK_MSTATUS(stat);

	MPointArray points;
	meshFn.getPoints(points);
	MFloatVectorArray normals, vertexNormals;
	meshFn.getNormals( normals, MSpace::kWorld );

	MVectorArray vtxNormals;

	for( vertexIt.reset(); !vertexIt.isDone(); vertexIt.next())
	{
		MVector n;
		MStatus s = vertexIt.getNormal(n, MSpace::kWorld);
		if( !s )
		{
			logger.debug(MString("Error: ") + s.errorString());
			continue;
		}
		vertexNormals.append(n);
	}

	MFloatArray uArray, vArray;
	meshFn.getUVs(uArray, vArray);

	logger.debug(MString("Translating mesh object ") + meshFn.name().asChar());
	MString meshFullName = makeGoodString(meshFn.fullPathName());

	Indigo::SceneNodeMeshRef mesh_node(new Indigo::SceneNodeMesh());
	mesh_node->setName(meshFullName.asChar());
	mesh_node->mesh = Indigo::MeshRef(new Indigo::Mesh());

	for( uint vtxId = 0; vtxId < points.length(); vtxId++)
	{
		mesh_node->mesh->vert_positions.push_back(Indigo::Vec3f(points[vtxId].x,points[vtxId].y,points[vtxId].z));
	}
	
	for( uint nId = 0; nId < vertexNormals.length(); nId++)
	{
		mesh_node->mesh->vert_normals.push_back(Indigo::Vec3f(vertexNormals[nId].x,vertexNormals[nId].y,vertexNormals[nId].z));
	}

	// at the moment only one uv map
	mesh_node->mesh->num_uv_mappings = 1;

	mesh_node->normal_smoothing = true;

	for( uint tId = 0; tId < uArray.length(); tId++)
	{
		mesh_node->mesh->uv_pairs.push_back(Indigo::Vec2f(uArray[tId], vArray[tId]));
	}   

	MPointArray triPoints;
	MIntArray triVtxIds;
	MIntArray faceVtxIds;
	MIntArray faceNormalIds;


	for(faceIt.reset(); !faceIt.isDone(); faceIt.next())
	{
		int faceId = faceIt.index();
		int numTris;
		faceIt.numTriangles(numTris);
		faceIt.getVertices(faceVtxIds);
		MIntArray faceUVIndices;

		faceNormalIds.clear();
		for( uint vtxId = 0; vtxId < faceVtxIds.length(); vtxId++)
		{
			faceNormalIds.append(faceIt.normalIndex(vtxId));
			int uvIndex;
			faceIt.getUVIndex(vtxId, uvIndex);
			faceUVIndices.append(uvIndex);
		}

		for( int triId = 0; triId < numTris; triId++)
		{
			int faceRelIds[3];
			faceIt.getTriangle(triId, triPoints, triVtxIds);

			for( uint triVtxId = 0; triVtxId < 3; triVtxId++)
			{
				for(uint faceVtxId = 0; faceVtxId < faceVtxIds.length(); faceVtxId++)
				{
					if( faceVtxIds[faceVtxId] == triVtxIds[triVtxId])
					{
						faceRelIds[triVtxId] = faceVtxId;
					}
				}
			}
			
			uint vtxId0 = faceVtxIds[faceRelIds[0]];
			uint vtxId1 = faceVtxIds[faceRelIds[1]];
			uint vtxId2 = faceVtxIds[faceRelIds[2]];
			uint normalId0 = faceNormalIds[faceRelIds[0]];
			uint normalId1 = faceNormalIds[faceRelIds[1]];
			uint normalId2 = faceNormalIds[faceRelIds[2]];
			uint uvId0 = faceUVIndices[faceRelIds[0]];
			uint uvId1 = faceUVIndices[faceRelIds[1]];
			uint uvId2 = faceUVIndices[faceRelIds[2]];


			Indigo::Triangle t;
			t.tri_mat_index = 0;
			
			t.vertex_indices[0] = vtxId0;
			t.vertex_indices[1] = vtxId1;
			t.vertex_indices[2] = vtxId2;
			t.uv_indices[0] = uvId0;
			t.uv_indices[1] = uvId1;
			t.uv_indices[2] = uvId2;

			mesh_node->mesh->triangles.push_back(t);
		}		
	}
	mesh_node->mesh->endOfModel();

	bool useSubdiv = false;
	getBool("mtin_mesh_subdivUse", meshFn, useSubdiv);
	if( useSubdiv )
	{
		getInt("mtin_mesh_subdivMaxSubdiv", meshFn, mesh_node->max_num_subdivisions);
		getBool("mtin_mesh_subdivSmooth", meshFn, mesh_node->subdivision_smoothing);
		getDouble(MString("mtin_mesh_subdivCurvatureThreshold"), meshFn, mesh_node->subdivide_curvature_threshold);
		getDouble(MString("mtin_mesh_subdivErrorThreshold"), meshFn, mesh_node->displacement_error_threshold);
		getBool("mtin_mesh_subdivViewDependent", meshFn, mesh_node->view_dependent_subdivision);
		getDouble(MString("mtin_mesh_subdivPixelThreshold"), meshFn, mesh_node->subdivide_pixel_threshold);
	}
	
	sceneRootRef->addChildNode(mesh_node);

	obj->meshRef = mesh_node;

	//createIndigoMaterial(obj);

	this->defineShadingNodes(obj);

}


		////==================== Create light geometry =========================
		//Indigo::SceneNodeMeshRef mesh_node(new Indigo::SceneNodeMesh());
		//mesh_node->mesh = Indigo::MeshRef(new Indigo::Mesh());
		//mesh_node->mesh->num_uv_mappings = 0;
		//
		//// Make a single quad
		//Indigo::Quad q;
		//
		//// Set the material index to the first material of the object.
		//q.mat_index = 0;

		//for(int i = 0; i < 4; ++i)
		//{
		//	q.vertex_indices[i] = i;
		//	q.uv_indices[i] = 0;
		//}

		//// Add it to mesh's quads
		//mesh_node->mesh->quads.push_back(q);

		//// Define the vertices
		//mesh_node->mesh->vert_positions.push_back(Indigo::Vec3f(0, 0, 2));
		//mesh_node->mesh->vert_positions.push_back(Indigo::Vec3f(0, 1, 2));
		//mesh_node->mesh->vert_positions.push_back(Indigo::Vec3f(1, 1, 2));
		//mesh_node->mesh->vert_positions.push_back(Indigo::Vec3f(1, 0, 2));

		//mesh_node->mesh->endOfModel(); // Build the mesh
		//
		//sceneRootRef->addChildNode(mesh_node);


		////==================== Create an emitting material =========================
		//Indigo::SceneNodeMaterialRef mat(new Indigo::SceneNodeMaterial());

		//Reference<Indigo::DiffuseMaterial> diffuse = new Indigo::DiffuseMaterial();
		//diffuse->random_triangle_colours = false;
		//diffuse->layer = 0;
		//diffuse->base_emission = new Indigo::ConstantWavelengthDependentParam(new Indigo::UniformSpectrum(1.0e10));
		//diffuse->emission = new Indigo::ConstantWavelengthDependentParam(new Indigo::UniformSpectrum(1.0));
		//mat->material = diffuse;

		//sceneRootRef->addChildNode(mat);


		////==================== Create the light object =========================
		//Indigo::SceneNodeModelRef model(new Indigo::SceneNodeModel());
		//model->setName("Light Object");
		//model->setGeometry(mesh_node);
		//model->keyframes.push_back(Indigo::KeyFrame());
		//model->rotation = new Indigo::MatrixRotation();
		//model->setMaterials(Indigo::Vector<Indigo::SceneNodeMaterialRef>(1, mat));

		//sceneRootRef->addChildNode(model);

void  IndigoRenderer::addGeometry(std::shared_ptr<mtin_MayaObject> obj)
{		
	Indigo::SceneNodeMeshRef meshRef = obj->meshRef;
	Indigo::SceneNodeMaterialRef matRef = obj->matRef;

	if( (obj->isInstanced() || obj->isInstancerObject ) && (obj->origObject != 0))
	{
		std::shared_ptr<mtin_MayaObject> origObj(std::static_pointer_cast<mtin_MayaObject>(obj->origObject));
		meshRef = origObj->meshRef;
		matRef = origObj->matRef;
	}

	if( meshRef.isNull())
	{
		logger.debug(MString("MeshRef for object ") + obj->shortName + " is null");
		return;
	}
			
	Indigo::SceneNodeModelRef model(new Indigo::SceneNodeModel());
	model->setName((obj->fullNiceName + "_model").asChar());
	model->setGeometry(meshRef);

 //   MPoint pos, scale, rot;
 //   MMatrix m = obj->transformMatrices[0];
 //   getMatrixComponents(m, pos, rot, scale);
 //   MTransformationMatrix tm(m);
	//MMatrix im;
	//im.setToIdentity();
 //   Indigo::MatrixRotation matRot(im[0][0],im[1][0],im[2][0], im[0][1],im[1][1],im[2][1] ,im[0][2],im[1][2],im[2][2]);
 //   Indigo::KeyFrame posKf(0.0, Indigo::Vec3d(pos.x, pos.y, pos.z), Indigo::AxisAngle().identity());
	//double x, y, z, w;
	//tm.getRotationQuaternion(x, y, z, w, MSpace::kWorld);
	//Indigo::AxisAngle axis(Indigo::Vec3d(x, y, z), w);	
	
    //Indigo::KeyFrame posKf(0.0, Indigo::Vec3d(pos.x, pos.y, pos.z), axis);
    //model->keyframes.push_back(posKf);
    //model->rotation = new Indigo::MatrixRotation(matRot);

	createTransform(model, obj);
	logger.debug(MString("Assinging material: ") + matRef->getName().dataPtr());

	
	for( uint i = 0; i < obj->iesProfilePaths.size(); i++)
	{
		Indigo::String iesPath(obj->iesProfilePaths[i].asChar());
		Indigo::IESProfileRef iesRef(new Indigo::IESProfile(iesPath, matRef));
		model->ies_profiles.push_back(iesRef);
	}

	model->setMaterials(Indigo::Vector<Indigo::SceneNodeMaterialRef>(1, matRef));		
	sceneRootRef->addChildNode(model); // Add node to scene graph.
}

void IndigoRenderer::defineGeometry()
{
	std::shared_ptr<MayaScene> mayaScene = MayaTo::getWorldPtr()->worldScenePtr;
	std::shared_ptr<RenderGlobals> renderGlobals = MayaTo::getWorldPtr()->worldRenderGlobalsPtr;

	for (auto mobj : mayaScene->objectList)
	{
		std::shared_ptr<mtin_MayaObject> obj(std::static_pointer_cast<mtin_MayaObject>(mobj));
		if( !obj->mobject.hasFn(MFn::kMesh))
			continue;

		if( !obj->visible )
			continue;
		
		getObjectShadingGroups(obj->dagPath, obj->perFaceAssignments, obj->shadingGroups, true);

		if( obj->instanceNumber == 0 )
			this->defineMesh(obj);

		this->addGeometry(obj);
	}

	for (auto mobj : mayaScene->instancerNodeElements)
	{
		std::shared_ptr<mtin_MayaObject> obj(std::static_pointer_cast<mtin_MayaObject>(mobj));
		if (!obj->mobject.hasFn(MFn::kMesh))
			continue;
		this->addGeometry(obj);
	}
}


