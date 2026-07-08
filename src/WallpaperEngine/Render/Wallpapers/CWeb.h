#pragma once

// Matrices manipulation for OpenGL
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/WebBrowser/CEF/BrowserClient.h"
#include "WallpaperEngine/WebBrowser/CEF/RenderHandler.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

namespace WallpaperEngine::WebBrowser::CEF {
class RenderHandler;
}

namespace WallpaperEngine::Render::Wallpapers {
class CWeb : public CWallpaper {
public:
    CWeb (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	WallpaperEngine::WebBrowser::WebBrowserContext& browserContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const float& offsetX = 0.0f,
	const float& offsetY = 0.0f, const float& contrast = 1.0f, const float& saturation = 1.0f,
	const glm::vec3& borderColour = { 0.0f, 0.0f, 0.0f }
    );
    ~CWeb () override;
    [[nodiscard]] int getWidth () const override { return this->m_width; }

    [[nodiscard]] int getHeight () const override { return this->m_height; }

    void setSize (int width, int height);
    void setPropertyValue (const std::string& key, const std::string& value);

    // Whether CEF has finished loading the main frame (CefLoadHandler::OnLoadEnd fired). CEF's
    // page load is async and can take real wall-clock time (network fetches, subprocess
    // spin-up) that has no fixed relationship to this app's own render-loop frame count — callers
    // that need to know "has this web wallpaper actually produced real content yet" (e.g. the
    // --screenshot flow) should check this rather than assuming a frame-count delay is enough.
    [[nodiscard]] bool isPageLoaded () const { return this->m_client != nullptr && this->m_client->isPageLoaded (); }

protected:
    void renderFrame (const glm::ivec4& viewport) override;
    void updateMouse (const glm::ivec4& viewport);
    void injectProperties ();
    void injectAudio ();
    const Web& getWeb () const { return *this->getWallpaperData ().as<Web> (); }

    friend class CWallpaper;

private:
    WallpaperEngine::WebBrowser::WebBrowserContext& m_browserContext;
    CefRefPtr<CefBrowser> m_browser = nullptr;
    CefRefPtr<WallpaperEngine::WebBrowser::CEF::BrowserClient> m_client = nullptr;
    WallpaperEngine::WebBrowser::CEF::RenderHandler* m_renderHandler = nullptr;

    int m_width = 16;
    int m_height = 16;

    WallpaperEngine::Input::MouseClickStatus m_leftClick = Input::Released;
    WallpaperEngine::Input::MouseClickStatus m_rightClick = Input::Released;

    glm::vec2 m_mousePosition = {};
    glm::vec2 m_mousePositionLast = {};
    bool m_propertiesInjected = false;
    int m_audioFrameCount = 0;
};
}
