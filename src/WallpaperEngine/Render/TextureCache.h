#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "TextureProvider.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/RenderContext.h"

using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render {
class AlbumTexture;
namespace Helpers {
    class ContextAware;
}

class RenderContext;

class TextureCache final : Helpers::ContextAware {
public:
    explicit TextureCache (RenderContext& context);
    ~TextureCache () override;

    /**
     * Checks if the given texture was already loaded and returns it
     * If the texture was not loaded yet, it tries to load it from the container
     *
     * @param filename
     * @return
     */
    std::shared_ptr<const TextureProvider> resolve (const std::string& filename);

    /**
     * Resolves a user-configurable texture slot (launcher icons, custom images, etc).
     * Unlike resolve(), an unconfigured slot is not an error: it falls back to a
     * cached 1x1 transparent texture instead of throwing, since the wallpaper must
     * still render sensibly before the user has set a value for the slot.
     *
     * @param filename
     * @return
     */
    std::shared_ptr<const TextureProvider> resolveUserTexture (const std::string& filename);

    /**
     * Registers a texture in the cache
     *
     * @param name
     * @param texture
     */
    void store (const std::string& name, std::shared_ptr<const TextureProvider> texture);

private:
    /**
     * @return A cached 1x1 fully transparent texture used as a placeholder for
     * unconfigured user texture slots
     */
    std::shared_ptr<const TextureProvider> resolveTransparentPlaceholder ();


    /** The previous album thumbnail texture */
    std::shared_ptr<const AlbumTexture> m_previousThumbnail = nullptr;
    /** The current album thumbnail texture */
    std::shared_ptr<const AlbumTexture> m_currentThumbnail = nullptr;
    /** Cached textures */
    std::map<std::string, std::shared_ptr<const TextureProvider>> m_textureCache = {};
    /** The callback to de-register media events */
    std::function<void ()> m_mediaCallback;
};
} // namespace WallpaperEngine::Render
