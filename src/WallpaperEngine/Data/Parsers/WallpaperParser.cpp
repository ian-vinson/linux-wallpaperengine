#include "WallpaperParser.h"

#include "ObjectParser.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/FileSystem/Container.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Parsers;

WallpaperUniquePtr WallpaperParser::parse (const JSON& file, Project& project) {
    switch (project.type) {
	case Project::Type_Scene:
	    return parseScene (file, project);
	case Project::Type_Video:
	    return parseVideo (file, project);
	case Project::Type_Web:
	    return parseWeb (file, project);
	default:
	    sLog.exception ("Unexpected project type value found... This is likely a bug");
    }
}

SceneUniquePtr WallpaperParser::parseScene (const JSON& file, Project& project) {
    const auto scene = WallpaperEngine::Data::JSON::parseLenient (project.assetLocator->readString (file));
    const auto camera = scene.require ("camera", "Scenes must have a camera section");
    const auto general = scene.require ("general", "Scenes must have a general section");
    const auto projection = general.optional ("orthogonalprojection");
    const auto objects = scene.require ("objects", "Scenes must have an objects section");
    const auto& properties = project.properties;

    // TODO: FIND IF THESE DEFAULTS ARE SENSIBLE OR NOT AND PERFORM PROPER VALIDATION WHEN CAMERA PREVIEW AND CAMERA
    // PARALLAX ARE PRESENT

    return std::make_unique <Scene> (
        WallpaperData {
            .filename = "",
            .project = project
        }, SceneData {
            .colors = {
                .ambient  = general.user ("ambientcolor", properties, glm::vec3 (0.0f)),
                .skylight = general.user ("skylightcolor", properties, glm::vec3 (0.0f)),
                .clear = general.user ("clearcolor", properties, glm::vec3 (1.0f)),
            },
            .camera = {
                .fade = general.user ("camerafade", properties, false),
                .preview = general.optional ("camerapreview", false),
                .bloom = {
                    .enabled = general.user ("bloom", properties, false),
                    .strength = general.user ("bloomstrength", properties, 0.0f),
                    .threshold = general.user ("bloomthreshold", properties, 0.0f),
                },
                .parallax = {
                    .enabled = general.user ("cameraparallax", properties, false),
                    .amount = general.user ("cameraparallaxamount", properties, 1.0f),
                    .delay = general.user ("cameraparallaxdelay", properties, 0.0f),
                    .mouseInfluence = general.user ("cameraparallaxmouseinfluence", properties, 1.0f),
                },
                .shake = {
                    .enabled = general.user ("camerashake", properties, false),
                    .amplitude = general.user ("camerashakeamplitude", properties, 0.0f),
                    .roughness = general.user ("camerashakeroughness", properties, 0.0f),
                    .speed = general.user ("camerashakespeed", properties, 0.0f),
                },
                .configuration = {
                    .center = camera.require <glm::vec3> ("center", "Camera must have a center position"),
                    .eye = camera.require <glm::vec3> ("eye", "Camera must have an eye position"),
                    .up = camera.require <glm::vec3> ("up", "Camera must have an up position"),
                },
                .projection = {
                    .width  = (projection.has_value () && !projection->optional ("auto", false)) ? projection->require <int> ("width",  "Projection must have a width") : 0,
                    .height = (projection.has_value () && !projection->optional ("auto", false)) ? projection->require <int> ("height", "Projection must have a height") : 0,
                    .isAuto = projection.has_value () && projection->optional ("auto", false),
                    .hasOrthogonal = projection.has_value (),
                    .perspectiveOverrideFov = general.optional ("perspectiveoverridefov", 0.0f),
                    .nearz = general.user ("nearz", properties, 0.0f),
                    .farz = general.user ("farz", properties, 10000.0f),
                    .fov = general.user ("fov", properties, 50.0f)
                }
            },
            .objects = parseObjects (objects, project),
        }
    );
}

VideoUniquePtr WallpaperParser::parseVideo (const JSON& file, Project& project) {
    return std::make_unique<Video> (WallpaperData { .filename = file, .project = project });
}

WebUniquePtr WallpaperParser::parseWeb (const JSON& file, Project& project) {
    return std::make_unique<Web> (WallpaperData {
	.filename = file,
	.project = project,
    });
}

ObjectList WallpaperParser::parseObjects (const JSON& objects, const Project& project) {
    ObjectList result = {};

    for (const auto& cur : objects) {
	try {
	    auto obj = ObjectParser::parse (cur, project);
	    if (obj != nullptr) {
		result.emplace_back (std::move (obj));
	    }
	} catch (const std::exception& e) {
	    sLog.error ("Failed to parse object, skipping: ", e.what ());
	}
    }

    return result;
}