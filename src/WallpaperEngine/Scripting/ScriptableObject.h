#pragma once
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptableObject : virtual public CObject {
public:
    struct PropertyEntry {
	std::string key;
	DynamicValue& value;
    };

    ScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~ScriptableObject () = default;

    DynamicValue& getProperty (const std::string& name);

    const std::map<std::string, PropertyEntry>& getProperties () const;

    // Queues a script for every property currently registered under m_properties. Must be called
    // exactly once, after an object's FULL constructor chain (base ScriptableObject ctor plus
    // every derived class's own ctor body) has finished registering its properties -- see
    // CScene::dispatchObjectType(), the only call site. Deliberately separate from
    // registerProperty() itself: a base class commonly registers a generic placeholder (e.g.
    // ScriptableObject's own "visible" from object.groupVisible) that a derived class then
    // replaces with its own more specific value (e.g. CImage's "visible" from image.visible) --
    // queuing inline per-registerProperty() call would bind the script to whichever registration
    // happened to run first, not to the one that's actually read at render time.
    void finalizeProperties ();

protected:
    // Last registration for a given name wins. Derived classes always construct after their base
    // (C++ guarantees base ctors run first), so a subclass's own more-specific field naturally
    // replaces a base class's generic placeholder registered under the same name -- see
    // finalizeProperties() for why the actual script-queuing is deferred rather than done here.
    void registerProperty (const std::string& name, DynamicValue& value);

private:
    std::map<std::string, PropertyEntry> m_properties;
};
}