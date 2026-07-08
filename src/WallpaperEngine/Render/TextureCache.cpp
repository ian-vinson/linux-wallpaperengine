#include "TextureCache.h"

#include "AlbumTexture.h"
#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;

TextureCache::TextureCache (RenderContext& context) : Helpers::ContextAware (context) {
    // these textures are special cases, so make sure they're created only upon request
    this->m_currentThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_currentThumbnail->getTextureID (0), -1, "$mediaThumbnail");
#endif

    this->m_previousThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_previousThumbnail->getTextureID (0), -1, "$mediaPreviousThumbnail");
#endif

    // load the latest texture (if available)
    this->m_currentThumbnail->load ();

    // add these to the cache and return the right one
    this->store ("$mediaThumbnail", this->m_currentThumbnail);
    this->store ("$mediaPreviousThumbnail", this->m_previousThumbnail);

    this->m_mediaCallback = this->getContext ().getMediaSource ().addAlbumArtListener (
	[this] (const Media::MediaSource::MediaInfo& data) {
	    if (this->m_currentThumbnail->isReady ()) {
		// copy over pixel data and setup the new texture with the new data
		this->m_previousThumbnail->copyContents (*this->m_currentThumbnail);
	    }

	    // load the next image
	    this->m_currentThumbnail->load ();
	}
    );
}

TextureCache::~TextureCache () { this->m_mediaCallback (); }

std::shared_ptr<const TextureProvider> TextureCache::resolve (const std::string& filename) {
    if (const auto found = this->m_textureCache.find (filename); found != this->m_textureCache.end ()) {
	return found->second;
    }

    // search for the texture in all the different containers just in case
    for (const auto& project : this->getContext ().getApp ().getBackgrounds () | std::views::values) {
	try {
	    const auto contents = project->assetLocator->texture (filename);
	    auto stream = BinaryReader (contents);

	    // Create metadata loader lambda that captures the assetLocator
	    // so we need to construct the full path here
	    auto metadataLoader = [&project] (const std::string& metaFilename) -> std::string {
		std::filesystem::path fullPath = std::filesystem::path ("materials") / metaFilename;
		return project->assetLocator->readString (fullPath);
	    };

	    auto parsedTexture = TextureParser::parse (stream, filename, metadataLoader);
	    auto texture = std::make_shared<CTexture> (this->getContext (), std::move (parsedTexture));

#if !NDEBUG
	    glObjectLabel (GL_TEXTURE, texture->getTextureID (0), -1, filename.c_str ());
#endif

	    this->store (filename, texture);

	    return texture;
	} catch (AssetLoadException&) {
	    // ignored, this happens if we're looking at the wrong background
	}
    }

    throw AssetLoadException ("Cannot find file", filename, std::error_code ());
}

std::shared_ptr<const TextureProvider> TextureCache::resolveUserTexture (const std::string& filename) {
    try {
        return this->resolve (filename);
    } catch (AssetLoadException&) {
        // the wallpaper's shader references a user texture slot (launcher icon, custom
        // image, etc) that the user hasn't configured yet -- this is expected, not an error
        sLog.debug ("User texture slot \"", filename, "\" is not configured, using a transparent placeholder");

        return this->resolveTransparentPlaceholder ();
    }
}

std::shared_ptr<const TextureProvider> TextureCache::resolveTransparentPlaceholder () {
    static const std::string placeholderName = "$transparentUserTexturePlaceholder";

    if (const auto found = this->m_textureCache.find (placeholderName); found != this->m_textureCache.end ()) {
        return found->second;
    }

    auto header = std::make_unique<Texture> ();
    header->format = TextureFormat_ARGB8888;
    header->freeImageFormat = FIF_UNKNOWN;
    header->textureWidth = header->width = 1;
    header->textureHeight = header->height = 1;
    header->imageCount = 1;

    auto mipmap = std::make_shared<Mipmap> ();
    mipmap->width = 1;
    mipmap->height = 1;
    mipmap->uncompressedSize = 4;
    mipmap->uncompressedData = std::unique_ptr<char[]> (new char[4] {0, 0, 0, 0});

    header->images.emplace (0, MipmapList {mipmap});

    auto texture = std::make_shared<CTexture> (this->getContext (), std::move (header));

    this->store (placeholderName, texture);

    return texture;
}

void TextureCache::store (const std::string& name, std::shared_ptr<const TextureProvider> texture) {
    this->m_textureCache.insert_or_assign (name, texture);
}

void TextureCache::updateAll () const {
    for (const auto& texture : this->m_textureCache | std::views::values) {
	texture->update ();
    }
}
