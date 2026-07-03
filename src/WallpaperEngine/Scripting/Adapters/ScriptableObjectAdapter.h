#pragma once

#include "ObjectAdapter.h"

namespace WallpaperEngine::Scripting::Adapters {
class ScriptableObjectAdapter : public ObjectAdapter {
public:
    explicit ScriptableObjectAdapter (ScriptEngine& engine, std::string name);

    JSValue instantiate (ScriptableObject& object) override;
    JSValue instantiate (Data::Model::DynamicValue& value) override;

    /**
     * Unwraps an ILayer JSValue (as returned by instantiate()) back into its underlying
     * ScriptableObject, or nullptr if the value is not a valid ILayer instance.
     */
    static ScriptableObject* extract (JSValueConst value);

private:
    JSClassExoticMethods m_exoticMethods;
    std::string m_name;
};
}