#include "JsonPrinter.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Data::Dumpers;
using namespace WallpaperEngine::Data::Model;

namespace {
template <typename Map> nlohmann::json indexedMapToJson (const Map& map) {
    nlohmann::json result = nlohmann::json::object ();

    for (const auto& [index, value] : map) {
	result[std::to_string (index)] = value;
    }

    return result;
}
} // namespace

nlohmann::json JsonPrinter::printWallpaper (const Wallpaper& wallpaper) {
    if (wallpaper.is<Video> ()) {
	const auto video = wallpaper.as<Video> ();
	return { { "type", "video" }, { "filename", video->filename } };
    }

    if (wallpaper.is<Web> ()) {
	const auto web = wallpaper.as<Web> ();
	return { { "type", "web" }, { "filename", web->filename } };
    }

    if (wallpaper.is<Scene> ()) {
	const auto scene = wallpaper.as<Scene> ();
	nlohmann::json objects = nlohmann::json::array ();

	for (const auto& object : scene->objects) {
	    objects.push_back (this->printObject (*object));
	}

	return { { "type", "scene" }, { "objects", objects } };
    }

    return { { "type", "unknown" } };
}

nlohmann::json JsonPrinter::printObject (const Object& object) {
    nlohmann::json result = { { "id", object.id },
			      { "name", object.name },
			      { "dependencies", object.dependencies },
			      { "parent", object.parent.has_value () ? nlohmann::json (object.parent.value ())
									: nlohmann::json (nullptr) } };

    if (object.is<Image> ()) {
	result["type"] = "image";
	result["image"] = this->printImage (*object.as<Image> ());
    } else if (object.is<Sound> ()) {
	result["type"] = "sound";
	result["sound"] = this->printSound (*object.as<Sound> ());
    } else if (object.is<Text> ()) {
	result["type"] = "text";
	result["text"] = this->printText (*object.as<Text> ());
    } else if (object.is<Particle> ()) {
	result["type"] = "particle";
	result["particle"] = this->printParticle (*object.as<Particle> ());
    } else if (object.is<CameraObject> ()) {
	result["type"] = "camera";
	result["camera"] = this->printCameraObject (*object.as<CameraObject> ());
    } else {
	result["type"] = "unknown";
    }

    return result;
}

nlohmann::json JsonPrinter::printImage (const Image& image) {
    nlohmann::json effects = nlohmann::json::array ();

    for (const auto& effect : image.effects) {
	effects.push_back (this->printImageEffect (*effect));
    }

    return { { "model", this->printModel (*image.model) }, { "effects", effects } };
}

nlohmann::json JsonPrinter::printSound (const Sound& sound) { return { { "files", sound.sounds } }; }

nlohmann::json JsonPrinter::printText (const Text& text) {
    return { { "font", text.font }, { "text", text.text->value->toString () } };
}

nlohmann::json JsonPrinter::printParticle (const Particle& particle) {
    return { { "particleFile", particle.particleFile },
	     { "animationMode", particle.animationMode },
	     { "maxCount", particle.maxCount } };
}

nlohmann::json JsonPrinter::printCameraObject (const CameraObject& camera) {
    return { { "cameraName", camera.cameraName } };
}

nlohmann::json JsonPrinter::printModel (const ModelStruct& model) {
    nlohmann::json result = { { "filename", model.filename },      { "autosize", model.autosize },
			      { "passthrough", model.passthrough }, { "fullscreen", model.fullscreen },
			      { "nopadding", model.nopadding },     { "solidlayer", model.solidlayer } };

    if (model.width.has_value ()) {
	result["width"] = model.width.value ();
    }

    if (model.height.has_value ()) {
	result["height"] = model.height.value ();
    }

    result["material"] = this->printMaterial (*model.material);

    return result;
}

nlohmann::json JsonPrinter::printImageEffect (const ImageEffect& imageEffect) {
    nlohmann::json passOverrides = nlohmann::json::array ();

    for (const auto& override : imageEffect.passOverrides) {
	passOverrides.push_back (this->printImageEffectPassOverride (*override));
    }

    return { { "id", imageEffect.id },
	     { "defaultVisible", imageEffect.visible->value->getBool () },
	     { "effect", this->printEffect (*imageEffect.effect) },
	     { "passOverrides", passOverrides } };
}

nlohmann::json JsonPrinter::printImageEffectPassOverride (const ImageEffectPassOverride& imageEffectPass) {
    nlohmann::json constants = nlohmann::json::object ();

    for (const auto& [name, value] : imageEffectPass.constants) {
	constants[name] = value->value->toString ();
    }

    return { { "id", imageEffectPass.id },
	     { "textures", indexedMapToJson (imageEffectPass.textures) },
	     { "combos", imageEffectPass.combos },
	     { "constants", constants } };
}

nlohmann::json JsonPrinter::printFBO (const FBO& fbo) {
    return { { "name", fbo.name }, { "format", fbo.format }, { "scale", fbo.scale }, { "unique", fbo.unique } };
}

nlohmann::json JsonPrinter::printMaterial (const Material& material) {
    nlohmann::json passes = nlohmann::json::array ();

    for (const auto& pass : material.passes) {
	passes.push_back (this->printMaterialPass (*pass));
    }

    return { { "filename", material.filename }, { "passes", passes } };
}

nlohmann::json JsonPrinter::printMaterialPass (const MaterialPass& materialPass) {
    return { { "shader", materialPass.shader },
	     { "depthTest", static_cast<int> (materialPass.depthtest) },
	     { "depthWrite", static_cast<int> (materialPass.depthwrite) },
	     { "blending", static_cast<int> (materialPass.blending) },
	     { "cullMode", static_cast<int> (materialPass.cullmode) },
	     { "textures", indexedMapToJson (materialPass.textures) },
	     { "combos", materialPass.combos } };
}

nlohmann::json JsonPrinter::printEffect (const Effect& effect) {
    nlohmann::json passes = nlohmann::json::array ();

    for (const auto& pass : effect.passes) {
	passes.push_back (this->printEffectPass (*pass));
    }

    nlohmann::json fbos = nlohmann::json::array ();

    for (const auto& fbo : effect.fbos) {
	fbos.push_back (this->printFBO (*fbo));
    }

    return { { "name", effect.name },   { "description", effect.description },
	     { "group", effect.group }, { "preview", effect.preview },
	     { "dependencies", effect.dependencies }, { "passes", passes },
	     { "fbos", fbos } };
}

nlohmann::json JsonPrinter::printEffectPass (const EffectPass& effectPass) {
    nlohmann::json result = nlohmann::json::object ();

    if (effectPass.command.has_value ()) {
	result["command"] = *effectPass.command == Command_Copy ? "copy" : "swap";
    }

    if (effectPass.target.has_value ()) {
	result["target"] = effectPass.target.value ();
    }

    if (effectPass.source.has_value ()) {
	result["source"] = effectPass.source.value ();
    }

    result["binds"] = indexedMapToJson (effectPass.binds);

    if (effectPass.material.has_value ()) {
	result["material"] = this->printMaterial (**effectPass.material);
    }

    return result;
}
