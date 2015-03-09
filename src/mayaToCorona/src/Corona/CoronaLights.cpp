#include "Corona.h"
#include "CoronaLights.h"
#include "CoronaSky.h"
#include "CoronaShaders.h"
#include "CoronaUtils.h"

#include <maya/MObjectArray.h>

#include "renderGlobals.h"
#include "utilities/logging.h"
#include "utilities/tools.h"
#include "utilities/attrTools.h"
#include "../mtco_common/mtco_mayaScene.h"
#include "../mtco_common/mtco_mayaObject.h"

static Logging logger;

bool CoronaRenderer::isSunLight(std::shared_ptr<MayaObject> obj)
{
	// a sun light has a transform connection to the coronaGlobals.sunLightConnection plug
	MObject coronaGlobals = objectFromName(MString("coronaGlobals"));
	MObjectArray nodeList;
	MStatus stat;
	getConnectedInNodes(MString("sunLightConnection"), coronaGlobals, nodeList);
	if( nodeList.length() > 0)
	{
		MObject sunObj = nodeList[0];
		if(sunObj.hasFn(MFn::kTransform))
		{
			MFnDagNode sunDagNode(sunObj);
			MObject sunDagObj =	sunDagNode.child(0, &stat);
			if( sunDagObj == obj->mobject)
				return true;
		}
	}
	return false;
}

void CoronaRenderer::defineLights()
{
	MFnDependencyNode rGlNode(getRenderGlobalsNode());
	// first get the globals node and serach for a directional light connection
	MObject coronaGlobals = getRenderGlobalsNode();
	MObjectArray nodeList;
	MStatus stat;

	if( renderGlobals->useSunLightConnection )
	{
		getConnectedInNodes(MString("sunLightConnection"), coronaGlobals, nodeList);
		if( nodeList.length() > 0)
		{
			MObject sunObj = nodeList[0];
			if(sunObj.hasFn(MFn::kTransform))
			{
				// we suppose what's connected here is a dir light transform
				MVector lightDir(0, 0, 1); // default dir light dir
				MFnDagNode sunDagNode(sunObj);
				lightDir *= sunDagNode.transformationMatrix();
				lightDir *= renderGlobals->globalConversionMatrix;
				lightDir.normalize();
		
				MObject sunDagObj =	sunDagNode.child(0, &stat);
				MColor sunColor(1);
				float colorMultiplier = 1.0f;
				if(sunDagObj.hasFn(MFn::kDirectionalLight))
				{
					MFnDependencyNode sunNode(sunDagObj);
					getColor("color", sunNode, sunColor);
					colorMultiplier = getFloatAttr("mtco_sun_multiplier", sunNode, 1.0f);
				}else{
					Logging::warning(MString("Sun connection is not a directional light - using transform only."));
				}
				const float intensityFactor = (1.f - cos(Corona::SUN_PROJECTED_HALF_ANGLE)) / (1.f - cos(getFloatAttr("sunSizeMulti", rGlNode, 1.0f) * Corona::SUN_PROJECTED_HALF_ANGLE));
				sunColor *= colorMultiplier * intensityFactor * 1.0;// 2000000;
				Corona::Sun sun;

				Corona::ColorOrMap bgCoMap = this->context.scene->getBackground();
				SkyMap *sky = dynamic_cast<SkyMap *>(bgCoMap.getMap());

				Corona::Rgb avgColor(1, 1, 1);
				if (sky != NULL)
				{
					avgColor = sky->sc();
				}

				Corona::Rgb sColor(sunColor.r, sunColor.g, sunColor.b);
				sun.color = sColor * avgColor;
				sun.active = true;
				sun.dirTo = Corona::Dir(lightDir.x, lightDir.y, lightDir.z).getNormalized();
				//sun.color = Corona::Rgb(sunColor.r,sunColor.g,sunColor.b);
				sun.visibleDirect = true;
				sun.visibleReflect = true;
				sun.visibleRefract = true;
				sun.sizeMultiplier = getFloatAttr("sunSizeMulti", rGlNode, 1.0);
				this->context.scene->getSun() = sun;

			}
		}
	}

	for( size_t lightId = 0; lightId < this->mtco_scene->lightList.size();  lightId++)
	{
		std::shared_ptr<MayaObject> obj =  (std::shared_ptr<MayaObject> )this->mtco_scene->lightList[lightId];
		if(!obj->visible)
			continue;

		if( this->isSunLight(obj))
			continue;
		
		MFnDependencyNode depFn(obj->mobject);

		if( obj->mobject.hasFn(MFn::kPointLight))
		{
			MColor col;
			getColor("color", depFn, col);
			float intensity = 1.0f;
			getFloat("intensity", depFn, intensity);
			int decay = 0;
			getEnum(MString("decayRate"), depFn, decay);
			MMatrix m = obj->transformMatrices[0] * renderGlobals->globalConversionMatrix;
			Corona::Pos LP(m[3][0],m[3][1],m[3][2]);
			PointLight *pl = new PointLight;
			pl->LP = LP;
			pl->distFactor = 1.0/renderGlobals->scaleFactor;
			pl->lightColor = Corona::Rgb(col.r, col.g, col.b);
			pl->lightIntensity = intensity;
			getEnum(MString("decayRate"), depFn, pl->decayType);
			pl->lightRadius = getFloatAttr("lightRadius", depFn, 0.0) * renderGlobals->scaleFactor;
			pl->doShadows = getBoolAttr("useRayTraceShadows", depFn, true);
			col = getColorAttr("shadowColor", depFn);
			pl->shadowColor = Corona::Rgb(col.r, col.g, col.b);
			for (size_t loId = 0; loId < obj->excludedObjects.size(); loId++)
			{ 
				pl->excludeList.nodes.push(obj->excludedObjects[loId]);
			}
			this->context.scene->addLightShader(pl);
		}
		if( obj->mobject.hasFn(MFn::kSpotLight))
		{
			MVector lightDir(0, 0, -1);
			MColor col;
			getColor("color", depFn, col);
			float intensity = 1.0f;
			getFloat("intensity", depFn, intensity);
			MMatrix m = obj->transformMatrices[0] * renderGlobals->globalConversionMatrix;
			lightDir *= obj->transformMatrices[0] * renderGlobals->globalConversionMatrix;
			lightDir.normalize();
			Corona::Pos LP(m[3][0],m[3][1],m[3][2]);
			SpotLight *sl = new SpotLight;
			sl->LP = LP;
			sl->lightColor = Corona::Rgb(col.r, col.g, col.b);
			sl->lightIntensity = intensity;
			sl->LD = Corona::Dir(lightDir.x, lightDir.y, lightDir.z);
			sl->angle = 45.0f;
			sl->distFactor = 1.0/renderGlobals->scaleFactor;
			getEnum(MString("decayRate"), depFn, sl->decayType);
			getFloat("coneAngle", depFn, sl->angle);
			getFloat("penumbraAngle", depFn, sl->penumbraAngle);
			getFloat("dropoff", depFn, sl->dropoff);
			sl->lightRadius = getFloatAttr("lightRadius", depFn, 0.0) * renderGlobals->scaleFactor;
			sl->doShadows = getBoolAttr("useRayTraceShadows", depFn, true);
			col = getColorAttr("shadowColor", depFn);
			sl->shadowColor = Corona::Rgb(col.r, col.g, col.b);
			for (size_t loId = 0; loId < obj->excludedObjects.size(); loId++)
			{
				sl->excludeList.nodes.push(obj->excludedObjects[loId]);
			}
			Corona::AffineTm tm;
			setTransformationMatrix(sl->lightWorldInverseMatrix, m);
			ShadingNetwork network(obj->mobject);
			sl->lightColorMap = defineAttribute(MString("color"), depFn, network);
			this->context.scene->addLightShader(sl);
		}
		if( obj->mobject.hasFn(MFn::kDirectionalLight))
		{
			MVector lightDir(0, 0, -1);
			MVector lightDirTangent(1, 0, 0);
			MVector lightDirBiTangent(0, 1, 0);
			MColor col;
			getColor("color", depFn, col);
			float intensity = 1.0f;
			getFloat("intensity", depFn, intensity);
			MMatrix m = obj->transformMatrices[0] * renderGlobals->globalConversionMatrix;
			lightDir *= m;
			lightDirTangent *= m;
			lightDirBiTangent *= m;
			lightDir.normalize();

			Corona::Pos LP(m[3][0],m[3][1],m[3][2]);
			DirectionalLight *dl = new DirectionalLight;
			dl->LP = LP;
			dl->lightColor = Corona::Rgb(col.r, col.g, col.b);
			dl->lightIntensity = intensity;
			dl->LD = Corona::Dir(lightDir.x, lightDir.y, lightDir.z);
			dl->LT = Corona::Dir(lightDirTangent.x, lightDirTangent.y, lightDirTangent.z);
			dl->LBT = Corona::Dir(lightDirBiTangent.x, lightDirBiTangent.y, lightDirBiTangent.z);
			dl->lightAngle = getFloatAttr("lightAngle", depFn, 0.0);
			dl->doShadows = getBoolAttr("useRayTraceShadows", depFn, true);
			col = getColorAttr("shadowColor", depFn);
			dl->shadowColor = Corona::Rgb(col.r, col.g, col.b);
			for (size_t loId = 0; loId < obj->excludedObjects.size(); loId++)
			{
				dl->excludeList.nodes.push(obj->excludedObjects[loId]);
			}

			this->context.scene->addLightShader(dl);
		}
		if( obj->mobject.hasFn(MFn::kAreaLight))
		{
			MMatrix m = obj->transformMatrices[0] * renderGlobals->globalConversionMatrix;
			obj->geom = defineStdPlane();
			Corona::AnimatedAffineTm atm;
			this->setAnimatedTransformationMatrix(atm, obj);
			obj->instance = obj->geom->addInstance(atm, NULL, NULL);
			if (getBoolAttr("mtco_envPortal", depFn, false))
			{
				Corona::EnviroPortalMtlData data;
				Corona::SharedPtr<Corona::IMaterial> mat = data.createMaterial();
				Corona::IMaterialSet ms = Corona::IMaterialSet(mat);
				obj->instance->addMaterial(ms);
			}
			else{
				Corona::NativeMtlData data;
				MColor lightColor = getColorAttr("color", depFn);
				float intensity = getFloatAttr("intensity", depFn, 1.0f);
				lightColor *= intensity;
				data.emission.color = Corona::ColorOrMap(Corona::Rgb(lightColor.r, lightColor.g, lightColor.b));
				for (size_t loId = 0; loId < obj->excludedObjects.size(); loId++)
				{
					data.emission.excluded.nodes.push(obj->excludedObjects[loId]);
				}				

				Corona::SharedPtr<Corona::IMaterial> mat = data.createMaterial();
				Corona::IMaterialSet ms = Corona::IMaterialSet(mat);
				bool visible = getBoolAttr("mtco_areaVisible", depFn, true);
				ms.visibility.direct = visible;
				ms.visibility.reflect = visible;
				ms.visibility.refract = visible;
				obj->instance->addMaterial(ms);
			}
		}
	}
}
