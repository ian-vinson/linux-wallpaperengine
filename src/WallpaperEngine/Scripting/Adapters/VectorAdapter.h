#pragma once

#include "ObjectAdapter.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Scripting::Adapters {
template <int components> class VectorAdapter : public ObjectAdapter {
public:
    explicit VectorAdapter (ScriptEngine& engine);
    ~VectorAdapter () override;

    int length () { return components; }
    JSValue instantiate (Data::Model::DynamicValue& value) override;
    JSValue instantiate (ScriptableObject& object) override;
    /**
     * @return A new, anonymous JSValue representing the vector
     */
    JSValue instantiate (Data::Model::DynamicValue& source, bool temporal);
    JSValue instantiate ();

    void free (uint32_t vectorId);

    /**
     * Releases this adapter's own reference to its prototype object. Must be called before
     * JS_FreeContext()/JS_FreeRuntime() — the runtime's own teardown GC pass still needs to
     * finalize any surviving vector instances (calling back into free() above), so the adapter
     * itself must stay alive (not destructed) until after that runs; only the JS-side reference
     * is released here.
     */
    void releaseBeforeRuntimeShutdown ();

private:
    JSValue m_prototype;
    uint32_t m_instanceId;
    std::string m_name;
    JSClassExoticMethods m_exoticMethods;
    std::map<uint32_t, Data::Model::DynamicValueUniquePtr> m_values;
};

extern template class VectorAdapter<2>;
extern template class VectorAdapter<3>;
extern template class VectorAdapter<4>;
}