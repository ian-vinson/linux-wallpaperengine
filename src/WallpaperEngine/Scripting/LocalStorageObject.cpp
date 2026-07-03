#include "LocalStorageObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace WallpaperEngine::Scripting;
using WallpaperEngine::Data::Utils::ScopeGuard;

static LocalStorageObject* ls_opaque (JSValueConst this_val) {
    JSClassID classId;
    return static_cast<LocalStorageObject*> (JS_GetAnyOpaque (this_val, &classId));
}

JSValue ls_get_item (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_NULL;
    auto* ls = ls_opaque (this_val);
    const char* key = JS_ToCString (ctx, argv[0]);
    if (!key)
        return JS_NULL;
    ScopeGuard guard ([ctx, key] { JS_FreeCString (ctx, key); });
    const std::string* value = ls->findItem (key);
    return value ? JS_NewString (ctx, value->c_str ()) : JS_NULL;
}

JSValue ls_set_item (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2)
        return JS_UNDEFINED;
    auto* ls = ls_opaque (this_val);
    const char* key = JS_ToCString (ctx, argv[0]);
    const char* value = JS_ToCString (ctx, argv[1]);
    if (!key || !value) {
        if (key)
            JS_FreeCString (ctx, key);
        if (value)
            JS_FreeCString (ctx, value);
        return JS_UNDEFINED;
    }
    ScopeGuard gk ([ctx, key] { JS_FreeCString (ctx, key); });
    ScopeGuard gv ([ctx, value] { JS_FreeCString (ctx, value); });
    ls->setItem (key, value);
    return JS_UNDEFINED;
}

JSValue ls_remove_item (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_UNDEFINED;
    auto* ls = ls_opaque (this_val);
    const char* key = JS_ToCString (ctx, argv[0]);
    if (!key)
        return JS_UNDEFINED;
    ScopeGuard guard ([ctx, key] { JS_FreeCString (ctx, key); });
    ls->removeItem (key);
    return JS_UNDEFINED;
}

// The real Wallpaper Engine `localStorage` API — verified against ~300 workshop scripts, none of
// which call getItem/setItem/removeItem (the standard Web Storage names implemented above; kept
// since they're harmless). get()/set()/remove() are what real scripts use, and set() commonly
// stores whole objects/arrays (e.g. `localStorage.set(POSITION_KEY, {x, y})`), not just strings —
// so values round-trip through JSON rather than being coerced to a bare string.
//
// Real scripts also pass an optional trailing "location" argument (`localStorage.LOCATION_SCREEN`
// / the literal string `'LOCATION_GLOBAL'`), presumably scoping storage per-monitor vs.
// per-wallpaper-instance when the same wallpaper runs on multiple screens at once. lwe's storage
// backend is a single per-wallpaper JSON file with no per-screen concept, so that argument is
// accepted (to avoid breaking the call) but has no effect — every screen running the same
// wallpaper shares one store.
JSValue ls_get (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    auto* ls = ls_opaque (this_val);
    const char* key = JS_ToCString (ctx, argv[0]);

    if (!key) {
	return JS_UNDEFINED;
    }

    ScopeGuard guard ([ctx, key] { JS_FreeCString (ctx, key); });

    const std::string* value = ls->findItem (key);

    if (!value) {
	return JS_UNDEFINED;
    }

    JSValue parsed = JS_ParseJSON (ctx, value->c_str (), value->size (), "<localStorage>");

    if (JS_IsException (parsed)) {
	// Not (or no longer) valid JSON — clear the exception and hand back the raw string
	// rather than propagating a parse error into an unrelated script.
	JS_FreeValue (ctx, JS_GetException (ctx));
	return JS_NewString (ctx, value->c_str ());
    }

    return parsed;
}

JSValue ls_set (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2) {
	return JS_UNDEFINED;
    }

    auto* ls = ls_opaque (this_val);
    const char* key = JS_ToCString (ctx, argv[0]);

    if (!key) {
	return JS_UNDEFINED;
    }

    ScopeGuard guard ([ctx, key] { JS_FreeCString (ctx, key); });

    JSValue json = JS_JSONStringify (ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);

    if (JS_IsException (json)) {
	JS_FreeValue (ctx, JS_GetException (ctx));
	return JS_UNDEFINED;
    }

    const char* jsonStr = JS_ToCString (ctx, json);

    if (jsonStr) {
	ls->setItem (key, jsonStr);
	JS_FreeCString (ctx, jsonStr);
    }

    JS_FreeValue (ctx, json);

    return JS_UNDEFINED;
}

JSValue ls_clear (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    ls_opaque (this_val)->clear ();
    return JS_UNDEFINED;
}

JSValue ls_get_length (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_NewInt32 (ctx, ls_opaque (this_val)->length ());
}

JSValue ls_key (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_NULL;
    auto* ls = ls_opaque (this_val);
    int n = 0;
    JS_ToInt32 (ctx, &n, argv[0]);
    if (n < 0 || n >= ls->length ())
        return JS_NULL;
    const std::string* k = ls->keyAt (n);
    return k ? JS_NewString (ctx, k->c_str ()) : JS_NULL;
}

LocalStorageObject::LocalStorageObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_classId (0) {
    // Storage path: ~/.local/share/lwe/storage/<workshopId>.json
    std::filesystem::path base;
    const char* home = std::getenv ("HOME");
    base = (home && home[0] != '\0')
               ? std::filesystem::path (home) / ".local" / "share" / "lwe" / "storage"
               : std::filesystem::temp_directory_path () / "lwe" / "storage";

    std::error_code ec;
    std::filesystem::create_directories (base, ec);

    const auto& project = scene.getScene ().project;
    std::string storageId = project.workshopId.empty () ? scene.getScene ().filename : project.workshopId;
    for (char& c : storageId) {
        if (c == '/' || c == '\\')
            c = '_';
    }
    if (storageId.empty ())
        storageId = "default";

    this->m_storagePath = base / (storageId + ".json");
    this->load ();

    this->m_definition = { .class_name = "ILocalStorage" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);
    JS_DupValue (this->m_engine.getContext (), this->m_instance);
    JS_SetOpaque (this->m_instance, this);

    auto* ctx = this->m_engine.getContext ();
    JS_DefinePropertyValueStr (ctx, this->m_instance, "getItem",
                               JS_NewCFunction (ctx, ls_get_item, "getItem", 1), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "setItem",
                               JS_NewCFunction (ctx, ls_set_item, "setItem", 2), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "removeItem",
                               JS_NewCFunction (ctx, ls_remove_item, "removeItem", 1), JS_PROP_ENUMERABLE);
    // Real WE API — see the comment on ls_get()/ls_set() above.
    JS_DefinePropertyValueStr (ctx, this->m_instance, "get", JS_NewCFunction (ctx, ls_get, "get", 1),
                               JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "set", JS_NewCFunction (ctx, ls_set, "set", 2),
                               JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "remove",
                               JS_NewCFunction (ctx, ls_remove_item, "remove", 1), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "LOCATION_SCREEN",
                               JS_NewString (ctx, "LOCATION_SCREEN"), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "LOCATION_GLOBAL",
                               JS_NewString (ctx, "LOCATION_GLOBAL"), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "clear",
                               JS_NewCFunction (ctx, ls_clear, "clear", 0), JS_PROP_ENUMERABLE);
    JS_DefinePropertyValueStr (ctx, this->m_instance, "key",
                               JS_NewCFunction (ctx, ls_key, "key", 1), JS_PROP_ENUMERABLE);
    JS_DefinePropertyGetSet (ctx, this->m_instance,
                             JS_NewAtom (ctx, "length"),
                             JS_NewCFunction (ctx, ls_get_length, "get", 0),
                             JS_UNDEFINED, JS_PROP_ENUMERABLE);
}

LocalStorageObject::~LocalStorageObject () { JS_FreeValue (this->m_engine.getContext (), this->m_instance); }

const std::string* LocalStorageObject::findItem (const std::string& key) const {
    auto it = this->m_data.find (key);
    return it != this->m_data.end () ? &it->second : nullptr;
}

std::string LocalStorageObject::getItem (const std::string& key) const {
    const std::string* v = this->findItem (key);
    return v ? *v : std::string {};
}

void LocalStorageObject::setItem (const std::string& key, const std::string& value) {
    this->m_data[key] = value;
    this->save ();
}

void LocalStorageObject::removeItem (const std::string& key) {
    this->m_data.erase (key);
    this->save ();
}

void LocalStorageObject::clear () {
    this->m_data.clear ();
    this->save ();
}

int LocalStorageObject::length () const { return static_cast<int> (this->m_data.size ()); }

const std::string* LocalStorageObject::keyAt (int n) const {
    if (n < 0 || n >= static_cast<int> (this->m_data.size ()))
        return nullptr;
    auto it = this->m_data.begin ();
    std::advance (it, n);
    return &it->first;
}

void LocalStorageObject::load () {
    std::ifstream f (this->m_storagePath);
    if (!f.is_open ())
        return;
    try {
        nlohmann::json j;
        f >> j;
        for (auto& [k, v] : j.items ()) {
            if (v.is_string ())
                this->m_data[k] = v.get<std::string> ();
        }
    } catch (...) {
        // corrupt file — start fresh
    }
}

void LocalStorageObject::save () const {
    std::ofstream f (this->m_storagePath, std::ios::trunc);
    if (!f.is_open ())
        return;
    nlohmann::json j = this->m_data;
    f << j.dump (2);
}
