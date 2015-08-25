#include "mtap_mayaRenderer.h"
#include "utilities/logging.h"
#include "utilities/attrTools.h"
#include "../appleseed/appleseedGeometry.h"

#include "foundation/core/appleseed.h"
#include "renderer/api/bsdf.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/material.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/image/image.h"
#include "foundation/image/tile.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/utility/containers/specializedarrays.h"
#include "foundation/utility/log/consolelogtarget.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/searchpaths.h"

#include "renderer/global/globallogger.h"
#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/texture.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/edf.h"

#include "renderer/modeling/shadergroup/shadergroup.h"
#include "../appleseed/appleseedUtils.h"

#include <thread>
#include <vector>
#include <maya/MGlobal.h>
#include <maya/MFnDependencyNode.h>

#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>


#if MAYA_API_VERSION >= 201600

#define kNumChannels 4

#define GETASM() project->get_scene()->assemblies().get_by_name("sceneAssembly")->assemblies().get_by_name("world")

mtap_MayaRenderer::mtap_MayaRenderer()
{
	log_target = std::auto_ptr<asf::ILogTarget>(asf::create_console_log_target(stdout));
	asr::global_logger().add_target(log_target.get());
	
	project = asr::ProjectFactory::create("mayaRendererProject");
	project->add_default_configurations();
	project->configurations()
		.get_by_name("final")->get_parameters()
		.insert_path("uniform_pixel_renderer.samples", "32");

	defineScene(project.get());
	defineMasterAssembly(project.get());
	defineDefaultMaterial(project.get());

	MString envMapAttrName = "C:/Program Files/Autodesk/Maya2016/presets/Assets/IBL/Exterior1_Color.exr";

	asr::ParamArray params;
	params.insert("filename", envMapAttrName.asChar());   
	params.insert("color_space", "linear_rgb");

	asf::SearchPaths searchPaths;
	asf::auto_release_ptr<asr::Texture> textureElement(
		asr::DiskTexture2dFactory().create(
		"envTex",
		params,
		searchPaths)); 

	project->get_scene()->textures().insert(textureElement);

	asr::ParamArray tInstParams;
	tInstParams.insert("addressing_mode", "clamp");
	tInstParams.insert("filtering_mode", "bilinear");

	MString textureInstanceName = "envTex_texInst";
	asf::auto_release_ptr<asr::TextureInstance> tinst = asr::TextureInstanceFactory().create(
		textureInstanceName.asChar(),
		tInstParams,
		"envTex");
	project->get_scene()->texture_instances().insert(tinst);


	asf::auto_release_ptr<asr::EnvironmentEDF> environmentEDF = asr::LatLongMapEnvironmentEDFFactory().create(
		"sky_edf",
		asr::ParamArray()
		.insert("radiance", textureInstanceName.asChar())
		.insert("radiance_multiplier", 5.0)
		.insert("horizontal_shift", 0.0)
		.insert("vertical_shift", 0.0)
		);

	asr::EnvironmentEDF *sky = project->get_scene()->environment_edfs().get_by_name("sky_edf");
	if (sky != nullptr)
		project->get_scene()->environment_edfs().remove(sky);
	project->get_scene()->environment_edfs().insert(environmentEDF);
	
	asr::EnvironmentShader *shader = project->get_scene()->environment_shaders().get_by_name("sky_shader");
	if (shader != nullptr)
		project->get_scene()->environment_shaders().remove(shader);

	project->get_scene()->environment_shaders().insert(
		asr::EDFEnvironmentShaderFactory().create(
		"sky_shader",
		asr::ParamArray()
		.insert("environment_edf", "sky_edf")));

	project->get_scene()->set_environment(
		asr::EnvironmentFactory::create(
		"sky",
		asr::ParamArray()
		.insert("environment_edf", "sky_edf")
		.insert("environment_shader", "sky_shader")));

	width = height = 64;
	MString res = MString("") + width + " " + height;
	MString tileSize = "16 16";

	project->set_frame(
		asr::FrameFactory::create(
		"beauty",
		asr::ParamArray()
		.insert("resolution", res.asChar())
		.insert("tile_size", tileSize.asChar())
		.insert("color_space", "linear_rgb")));

	renderBuffer = (float*)malloc(width*height*kNumChannels*sizeof(float));
}

bool mtap_MayaRenderer::isRunningAsync()
{
	Logging::debug("isRunningAsync");
	return true;
}
void* mtap_MayaRenderer::creator()
{
	return new mtap_MayaRenderer();
}

void mtap_MayaRenderer::render()
{
	ProgressParams progressParams;
	
	mrenderer->render();

	MString tstFile = "C:/daten/3dprojects/mayaToAppleseed/renderData/HypershadeRender.exr";
	project->get_frame()->write_main_image(tstFile.asChar());
	asr::ProjectFileWriter::write(project.ref(), "C:/daten/3dprojects/mayaToAppleseed/renderData/mayaRenderer.xml");
	Logging::debug("renderthread done.");

}

static void startRenderThread(mtap_MayaRenderer* renderPtr)
{
	renderPtr->render();
}

MStatus mtap_MayaRenderer::startAsync(const JobParams& params)
{
	Logging::debug("startAsync:");
	Logging::debug(MString("\tJobDescr: ") + params.description);
	this->tileCallbackFac.reset(new TileCallbackFactory(this));
	asr::ProjectFileWriter::write(project.ref(), "C:/daten/3dprojects/mayaToAppleseed/renderData/mayaRenderer.xml");

	mrenderer = std::auto_ptr<asr::MasterRenderer>( new asr::MasterRenderer(
		this->project.ref(),
		this->project->configurations().get_by_name("final")->get_inherited_parameters(),
		&controller,
		this->tileCallbackFac.get()));

	renderThread = std::thread(startRenderThread, this);
	Logging::debug(MString("started async"));
	//render();
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::stopAsync()
{
	Logging::debug("stopAsync");
	//controller.status = asr::IRendererController::PauseRendering;
	controller.status = asr::IRendererController::AbortRendering;
	return MS::kSuccess;
}

MStatus mtap_MayaRenderer::beginSceneUpdate()
{
	Logging::debug("beginSceneUpdate");
	controller.status = asr::IRendererController::AbortRendering;
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::translateMesh(const MUuid& id, const MObject& node)
{
	MObject mobject = node;
	MFnDependencyNode depFn(mobject);
	MString meshName = depFn.name();
	MString meshIdName = meshName + "_" + id;
	MString meshInstName = meshIdName + "_instance";
	Logging::debug(MString("translateMesh ") + meshIdName);

	asr::Object *obj = GETASM()->objects().get_by_name(meshIdName.asChar());
	if (obj != nullptr)
	{
		Logging::debug(MString("Mesh object ") + meshName + " is already defined, removing...");
		GETASM()->objects().remove(obj);
	}
	asf::auto_release_ptr<asr::MeshObject> mesh = MTAP_GEOMETRY::createMesh(mobject);
	mesh->set_name(meshIdName.asChar());
	GETASM()->objects().insert(asf::auto_release_ptr<asr::Object>(mesh));

	IdNameStruct idName;
	idName.id = id;
	idName.name = meshIdName;
	idName.mobject = node;
	objectArray.push_back(idName);
	lastShapeId = id;
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::translateLightSource(const MUuid& id, const MObject& node)
{
	MFnDependencyNode depFn(node);
	MString lightName = depFn.name();
	MString lightInstName = lightName + "_instance";
	MString lightIdName = lightName + "_" + id;

	Logging::debug(MString("translateLightSource: ") + depFn.name() + " from type: " + node.apiTypeStr());
	if (node.hasFn(MFn::kAreaLight))
	{		
		asr::MeshObject *meshPtr = (asr::MeshObject *)GETASM()->objects().get_by_name(lightIdName.asChar());
		if (meshPtr != nullptr)
			GETASM()->objects().remove(meshPtr);

		asf::auto_release_ptr<asr::MeshObject> plane = MTAP_GEOMETRY::defineStandardPlane();
		plane->set_name(lightIdName.asChar());
		GETASM()->objects().insert(asf::auto_release_ptr<asr::Object>(plane));

		meshPtr = (asr::MeshObject *)GETASM()->objects().get_by_name(lightIdName.asChar());

		MString areaLightMaterialName = lightIdName + "_material";
		MString physicalSurfaceName = lightIdName + "_physical_surface_shader";
		MString areaLightColorName = lightIdName + "_color";
		MString edfName = lightIdName + "_edf";
		asr::ParamArray edfParams;
		MColor color = getColorAttr("color", depFn);
		defineColor(project.get(), areaLightColorName.asChar(), color, getFloatAttr("intensity", depFn, 1.0f) * 2);
		edfParams.insert("radiance", areaLightColorName.asChar());

		asr::EDF *edfPtr = GETASM()->edfs().get_by_name(edfName.asChar());
		if (edfPtr != nullptr)
			GETASM()->edfs().remove(edfPtr);

		asf::auto_release_ptr<asr::EDF> edf = asr::DiffuseEDFFactory().create(edfName.asChar(), edfParams);
		GETASM()->edfs().insert(edf);

		asr::SurfaceShader *ss = GETASM()->surface_shaders().get_by_name(physicalSurfaceName.asChar());
		if (ss != nullptr)
			GETASM()->surface_shaders().remove(ss);

		GETASM()->surface_shaders().insert(
			asr::PhysicalSurfaceShaderFactory().create(
			physicalSurfaceName.asChar(),
			asr::ParamArray()));

		asr::Material *ma = GETASM()->materials().get_by_name(areaLightMaterialName.asChar());
		if (ma != nullptr)
			GETASM()->materials().remove(ma);

		GETASM()->materials().insert(
			asr::GenericMaterialFactory().create(
			areaLightMaterialName.asChar(),
			asr::ParamArray()
			.insert("surface_shader", physicalSurfaceName.asChar())
			.insert("edf", edfName.asChar())));

		IdNameStruct idName;
		idName.id = id;
		idName.name = lightIdName;
		idName.mobject = node;
		objectArray.push_back(idName);
		lastShapeId = id;

	}
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::translateCamera(const MUuid& id, const MObject& node)
{
	Logging::debug("translateCamera");
	
	asr::Camera *camera = project->get_scene()->get_camera();
	MFnDependencyNode depFn(node);
	MString camName = depFn.name();
	Logging::debug(MString("Creating camera shape: ") + depFn.name());
	
	float imageAspect = (float)width / (float)height;
	asr::ParamArray camParams;
	float horizontalFilmAperture = getFloatAttr("horizontalFilmAperture", depFn, 24.892f);
	float verticalFilmAperture = getFloatAttr("verticalFilmAperture", depFn, 18.669f);
	float focalLength = getFloatAttr("focalLength", depFn, 35.0f);
	float focusDistance = getFloatAttr("focusDistance", depFn, 10.0f);
	float fStop = getFloatAttr("fStop", depFn, 1000.0f);

	// maya aperture is given in inces so convert to cm and convert to meters
	horizontalFilmAperture = horizontalFilmAperture * 2.54f * 0.01f;
	verticalFilmAperture = verticalFilmAperture * 2.54f * 0.01f;
	verticalFilmAperture = horizontalFilmAperture / imageAspect;
	MString filmBack = MString("") + horizontalFilmAperture + " " + verticalFilmAperture;
	MString focalLen = MString("") + focalLength * 0.001f;

	camParams.insert("film_dimensions", filmBack.asChar());
	camParams.insert("focal_length", focalLen.asChar());
	camParams.insert("focal_distance", (MString("") + focusDistance).asChar());
	camParams.insert("f_stop", (MString("") + fStop).asChar());

	asf::auto_release_ptr<asr::Camera> appleCam = asr::PinholeCameraFactory().create(
		camName.asChar(),
		camParams);
	project->get_scene()->set_camera(appleCam);

	IdNameStruct idName;
	idName.id = id;
	idName.name = camName + "_" + id;
	idName.mobject = node;
	objectArray.push_back(idName);
	lastShapeId = id;
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::translateEnvironment(const MUuid& id, EnvironmentType type)
{
	Logging::debug("translateEnvironment");
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::translateTransform(const MUuid& id, const MUuid& childId, const MMatrix& matrix)
{
	Logging::debug("translateTransform");
	MObject shapeNode;
	IdNameStruct idNameObj;
	for (auto idobj : objectArray)
	{
		if (idobj.id == lastShapeId)
		{
			Logging::debug(MString("Found id object for transform: ") + idobj.name);
			shapeNode = idobj.mobject;
			idNameObj = idobj;
		}
	}
	MString elementInstName = idNameObj.name + "_instance";
	MString elementName = idNameObj.name;
	MMatrix mayaMatrix = matrix;
	asf::Matrix4d appleMatrix;
	MMatrixToAMatrix(mayaMatrix, appleMatrix);

	if (idNameObj.mobject.hasFn(MFn::kMesh))
	{
		asr::ObjectInstance *objInst = GETASM()->object_instances().get_by_name(elementInstName.asChar());
		if (objInst != nullptr)
		{
			Logging::debug(MString("Removing already existing inst object: ") + elementInstName);
			GETASM()->object_instances().remove(objInst);
		}
		GETASM()->object_instances().get_by_name(elementInstName.asChar());
		GETASM()->object_instances().insert(
			asr::ObjectInstanceFactory::create(
			elementInstName.asChar(),
			asr::ParamArray(),
			elementName.asChar(),
			asf::Transformd::from_local_to_parent(appleMatrix),
			asf::StringDictionary()
			.insert("slot0", "default")));
	}

	if (idNameObj.mobject.hasFn(MFn::kCamera))
	{
		asr::Camera *cam = project->get_scene()->get_camera();
		cam->transform_sequence().set_transform(0, asf::Transformd::from_local_to_parent(appleMatrix));
	}
	if (idNameObj.mobject.hasFn(MFn::kAreaLight))
	{
		MTransformationMatrix tm;
		double rotate90Deg[3] = { -M_PI_2, 0, 0 };
		tm.setRotation(rotate90Deg, MTransformationMatrix::kXYZ);
		MMatrix lightMatrix = tm.asMatrix() * mayaMatrix;
		MMatrixToAMatrix(lightMatrix, appleMatrix);

		asr::ObjectInstance *objInst = GETASM()->object_instances().get_by_name(elementInstName.asChar());
		if (objInst != nullptr)
		{
			Logging::debug(MString("Removing already existing inst object: ") + elementInstName);
			GETASM()->object_instances().remove(objInst);
		}
		Logging::debug(MString("area light transform: ") + idNameObj.name);
		MString areaLightMaterialName = elementName + "_material";
		GETASM()->object_instances().insert(
			asr::ObjectInstanceFactory::create(
			elementInstName.asChar(),
			asr::ParamArray(),
			elementName.asChar(),
			asf::Transformd::from_local_to_parent(appleMatrix),
			asf::StringDictionary()
			.insert("slot0", areaLightMaterialName.asChar())
			));
	}
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::translateShader(const MUuid& id, const MObject& node)
{
	Logging::debug(MString("translateShader: "));
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::setProperty(const MUuid& id, const MString& name, bool value)
{
	Logging::debug(MString("setProperty bool: ") + name + " " + value);
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::setProperty(const MUuid& id, const MString& name, int value)
{
	Logging::debug(MString("setProperty int: ") + name + " " + value);
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::setProperty(const MUuid& id, const MString& name, float value)
{
	Logging::debug(MString("setProperty float: ") + name + " " + value);
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::setProperty(const MUuid& id, const MString& name, const MString& value)
{
	Logging::debug(MString("setProperty string: ") + name + " " + value);
	return MStatus::kSuccess;
};

MStatus mtap_MayaRenderer::setShader(const MUuid& id, const MUuid& shaderId)
{
	Logging::debug("setShader");
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::setResolution(unsigned int w, unsigned int h)
{
	Logging::debug(MString("setResolution to") + w + " " + h);
	this->width = w;
	this->height = h;
	// Update resolution buffer
	this->renderBuffer = (float*)realloc(this->renderBuffer, w*h*kNumChannels*sizeof(float));

	MString res = MString("") + width + " " + height;
	MString tileSize = "16 16";

	asr::Camera *cam = project->get_scene()->get_camera();
	MString camName = "";
	if (cam != nullptr)
		camName = project->get_scene()->get_camera()->get_name();

	project->set_frame(
		asr::FrameFactory::create(
		"beauty",
		asr::ParamArray()
		.insert("camera", camName.asChar())
		.insert("resolution", res.asChar())
		.insert("tile_size", tileSize.asChar())
		.insert("color_space", "linear_rgb")));

	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::endSceneUpdate()
{
	Logging::debug("endSceneUpdate");
	// wait for thread end if it is running
	renderThread.join();
	renderThread = std::thread(startRenderThread, this);
	return MStatus::kSuccess;
};
MStatus mtap_MayaRenderer::destroyScene()
{
	Logging::debug("destroyScene");
	controller.status = asr::IRendererController::AbortRendering;
	renderThread.join();
	ProgressParams progressParams;
	progressParams.progress = -1.0f;
	progress(progressParams);
	return MStatus::kSuccess;
};

bool mtap_MayaRenderer::isSafeToUnload()
{
	Logging::debug("isSafeToUnload");
	return true;
};

void TileCallback::post_render_tile(const asr::Frame* frame, const size_t tile_x, const size_t tile_y)
{
	//boost::lock_guard<boost::mutex> guard(m_mutex);
	Logging::debug("TileCallback::post_render_tile");

	asf::Image img = frame->image();
	const asf::CanvasProperties& frame_props = img.properties();
	const asf::Tile& tile = frame->image().tile(tile_x, tile_y);

	asf::Tile float_tile_storage(
		frame_props.m_tile_width,
		frame_props.m_tile_height,
		frame_props.m_channel_count,
		asf::PixelFormatFloat);

	asf::Tile uint8_tile_storage(
		frame_props.m_tile_width,
		frame_props.m_tile_height,
		frame_props.m_channel_count,
		asf::PixelFormatUInt8);

	asf::Tile fp_rgb_tile(
		tile,
		asf::PixelFormatFloat,
		float_tile_storage.get_storage());

	frame->transform_to_output_color_space(fp_rgb_tile);

	static const size_t ShuffleTable[] = { 0, 1, 2, 3 };
	const asf::Tile uint8_rgb_tile(
		fp_rgb_tile,
		asf::PixelFormatUInt8,
		uint8_tile_storage.get_storage());

	size_t tw = tile.get_width();
	size_t th = tile.get_height();
	size_t numPixels = tw * th;
	//std::shared_ptr<RV_PIXEL> pixelsPtr(new RV_PIXEL[numPixels]);
	//RV_PIXEL *pixels = pixelsPtr.get();
	//for (size_t yy = 0; yy < th; yy++)
	//{
	//	for (size_t xx = 0; xx < tw; xx++)
	//	{
	//		size_t pixelId = yy * tw + xx;
	//		size_t yy1 = th - yy - 1;
	//		asf::uint8 *source = uint8_rgb_tile.pixel(xx, yy1);
	//		pixels[pixelId].r = (float)source[0];
	//		pixels[pixelId].g = (float)source[1];
	//		pixels[pixelId].b = (float)source[2];
	//		pixels[pixelId].a = (float)source[3];
	//	}
	//}

	//size_t x = tile_x * frame_props.m_tile_width;
	//size_t y = tile_y * frame_props.m_tile_height;
	//size_t x1 = x + tw - 1;
	//size_t y1 = y + th;
	//size_t miny = img.properties().m_canvas_height - y1;
	//size_t maxy = img.properties().m_canvas_height - y - 1;

}

void TileCallback::post_render(const asr::Frame* frame)
{
	Logging::debug("TileCallback::post_render");

	////The image is R32G32B32A32_Float format
	//refreshParams.bottom = 0;
	//refreshParams.top = height - 1;
	//refreshParams.bytesPerChannel = sizeof(float);
	//refreshParams.channels = kNumChannels;
	//refreshParams.left = 0;
	//refreshParams.right = width - 1;
	//refreshParams.width = width;
	//refreshParams.height = height;

	//refreshParams.data = renderBuffer;
	//refresh(refreshParams);

	////Logging::debug("renderthread sleeping 5000:");
	////std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	//progressParams.progress = 1.0f;
	//progress(progressParams);

	//asf::Image img = frame->image();
	//const asf::CanvasProperties& frame_props = img.properties();
	//size_t numPixels = frame_props.m_canvas_width * frame_props.m_canvas_height;

	//std::shared_ptr<RV_PIXEL> pixelsPtr(new RV_PIXEL[numPixels]);
	//RV_PIXEL *pixels = pixelsPtr.get();

	//for (int x = 0; x < numPixels; x++)
	//{
	//	pixels[x].r = 255.0f;
	//	pixels[x].g = .0f;
	//	pixels[x].b = .0f;
	//	pixels[x].a = .0f;
	//}

	////copyASImageToMayaImage(pixels, frame);

	//for (int tile_x = 0; tile_x < frame_props.m_tile_count_x; tile_x++)
	//{
	//	for (int tile_y = 0; tile_y < frame_props.m_tile_count_y; tile_y++)
	//	{
	//		const asf::Tile& tile = frame->image().tile(tile_x, tile_y);

	//		asf::Tile float_tile_storage(
	//			tile.get_width(),
	//			tile.get_height(),
	//			frame_props.m_channel_count,
	//			asf::PixelFormatFloat);

	//		asf::Tile uint8_tile_storage(
	//			tile.get_width(),
	//			tile.get_height(),
	//			frame_props.m_channel_count,
	//			asf::PixelFormatUInt8);

	//		asf::Tile fp_rgb_tile(
	//			tile,
	//			asf::PixelFormatFloat,
	//			float_tile_storage.get_storage());

	//		frame->transform_to_output_color_space(fp_rgb_tile);

	//		static const size_t ShuffleTable[] = { 0, 1, 2, 3 };
	//		asf::Tile uint8_rgb_tile(
	//			fp_rgb_tile,
	//			asf::PixelFormatUInt8,
	//			uint8_tile_storage.get_storage());

	//		copyTileToImage(pixels, uint8_rgb_tile, tile_x, tile_y, frame);
	//	}
	//}

}

#endif
