#ifndef mtlu_GLOBALS_H
#define mtlu_GLOBALS_H

#include "mayarendernodes/renderGlobalsNode.h"

const MString MayaToLuxGlobalsName = "luxGlobals";

class MayaToLuxGlobals : public MayaRenderGlobalsNode
{
public:
						MayaToLuxGlobals();
	virtual				~MayaToLuxGlobals(); 

	static  void*		creator();
	static  MStatus		initialize();

	static	MTypeId		id;

private:
//	------------- automatically created attributes start ----------- // 
static    MObject renderer;
static    MObject hsConfigFile;
static    MObject hsOclPlatformIndex;
static    MObject hsOclGpuUse;
static    MObject hsOclWGroupSize;
static    MObject sampler;
static    MObject noiseaware;
static    MObject pixelSampler;
static    MObject numSamples;
static    MObject maxconsecrejects;
static    MObject largemutationprob;
static    MObject mutationrange;
static    MObject usevariance;
static    MObject usecooldown;
static    MObject initSamples;
static    MObject chainLength;
static    MObject mutationRange;
static    MObject usePhysicalSky;
static    MObject physicalSun;
static    MObject physicalSunConnection;
static    MObject sunGain;
static    MObject turbidity;
static    MObject skySamples;
static    MObject sunRelSize;
static    MObject premultiplyAlpha;
static    MObject haltspp;
static    MObject halttime;
static    MObject uiupdateinterval;
static    MObject pixelfilter;
static    MObject filterWidth;
static    MObject filterHeight;
static    MObject filterAlpha;
static    MObject B;
static    MObject C;
static    MObject mSupersample;
static    MObject sincTau;
static    MObject accelerator;
static    MObject kdIntersectcost;
static    MObject kdTraversalcost;
static    MObject kdEmptybonus;
static    MObject kdMaxprims;
static    MObject kdMaxdepth;
static    MObject maxprimsperleaf;
static    MObject fullsweepthreshold;
static    MObject skipfactor;
static    MObject refineimmediately;
static    MObject treetype;
static    MObject costsamples;
static    MObject surfaceIntegrator;
static    MObject lightStrategy;
static    MObject shadowraycount;
static    MObject eyedepth;
static    MObject lightdepth;
static    MObject eyerrthreshold;
static    MObject lightrrthreshold;
static    MObject pathMaxdepth;
static    MObject rrstrategy;
static    MObject rrcontinueprob;
static    MObject includeenvironment;
static    MObject directlightsampling;
static    MObject dlightMaxdepth;
static    MObject phRenderingmode;
static    MObject phCausticphotons;
static    MObject phIndirectphotons;
static    MObject phDirectphotons;
static    MObject phRadiancephotons;
static    MObject phNphotonsused;
static    MObject phMaxphotondist;
static    MObject phMaxdepth;
static    MObject phMaxphotondepth;
static    MObject phFinalgather;
static    MObject phFinalgathersamples;
static    MObject phGatherangle;
static    MObject phRrstrategy;
static    MObject phRrcontinueprob;
static    MObject phDistancethreshold;
static    MObject phPhotonmapsfile;
static    MObject renderingmode;
static    MObject strategy;
static    MObject directsampleall;
static    MObject directsamples;
static    MObject indirectsampleall;
static    MObject indirectsamples;
static    MObject diffusereflectdepth;
static    MObject diffusereflectsamples;
static    MObject diffuserefractdepth;
static    MObject diffuserefractsamples;
static    MObject directdiffuse;
static    MObject indirectdiffuse;
static    MObject glossyreflectdepth;
static    MObject glossyreflectsamples;
static    MObject glossyrefractdepth;
static    MObject glossyrefractsamples;
static    MObject directglossy;
static    MObject indirectglossy;
static    MObject specularreflectdepth;
static    MObject specularrefractdepth;
static    MObject diffusereflectreject;
static    MObject diffuserefractreject;
static    MObject diffusereflectreject_threshold;
static    MObject diffuserefractreject_threshold;
static    MObject glossyreflectreject;
static    MObject glossyrefractreject;
static    MObject glossyreflectreject_threshold;
static    MObject glossyrefractreject_threshold;
static    MObject maxeyedepth;
static    MObject maxphotondepth;
static    MObject photonperpass;
static    MObject startk;
static    MObject alpha;
static    MObject glossythreshold;
static    MObject lookupaccel;
static    MObject sppmpixelsampler;
static    MObject photonsampler;
static    MObject sppmincludeenvironment;
static    MObject parallelhashgridspare;
static    MObject startradius;
static    MObject sppmdirectlightsampling;
static    MObject useproba;
static    MObject wavelengthstratification;
static    MObject debug;
static    MObject storeglossy;
//	------------- automatically created attributes end ----------- // 
};

#endif