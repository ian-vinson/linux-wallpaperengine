#include "ScriptableObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <ranges>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;

ScriptableObject::ScriptableObject (Wallpapers::CScene& scene, const Object& object) : CObject (scene, object) {
    // register common dynamic values
    this->registerProperty ("origin", *object.origin->value);
    this->registerProperty ("scale", *object.groupScale->value);
    this->registerProperty ("angles", *object.groupAngles->value);
    this->registerProperty ("visible", *object.groupVisible->value);
}

DynamicValue& ScriptableObject::getProperty (const std::string& name) {
    const auto it = this->m_properties.find (name);

    if (it == this->m_properties.end ()) {
	sLog.exception ("Property '" + name + "' not found on object '" + this->getObject ().name + "'");
    }

    return it->second.value;
}

const std::map<std::string, ScriptableObject::PropertyEntry>& ScriptableObject::getProperties () const {
    return this->m_properties;
}

void ScriptableObject::registerProperty (const std::string& name, DynamicValue& value) {
    // PropertyEntry::value is a reference, so it can't be assigned in place -- erase the old
    // entry (if any) before emplacing the new one, rather than trying to overwrite it.
    this->m_properties.erase (name);
    this->m_properties.emplace (
	name, PropertyEntry { .key = name + "_" + std::to_string (this->getId ()), .value = value }
    );
}

void ScriptableObject::finalizeProperties () {
    for (const auto& entry : this->m_properties | std::views::values) {
	this->getScene ().getScriptEngine ().queueScript (entry.key, entry.value, *this);
    }
}
