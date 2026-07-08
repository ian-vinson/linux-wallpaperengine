// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"
#include "WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.h"

#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include <sstream>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

using namespace WallpaperEngine::WebBrowser;
using namespace WallpaperEngine::WebBrowser::CEF;

CWeb::CWeb (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, WebBrowserContext& browserContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const float& offsetX,
    const float& offsetY
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode, offsetX, offsetY),
    m_browserContext (browserContext) {
    // Seed m_width/m_height from the output before creating framebuffers and the CEF browser.
    // Without this, CreateBrowserSync triggers GetViewRect and receives 16×17 — the wrong size.
    const int outputWidth = context.getOutput ().getFullWidth ();
    const int outputHeight = context.getOutput ().getFullHeight ();
    if (outputWidth > 0) this->m_width = outputWidth;
    if (outputHeight > 0) this->m_height = outputHeight;

    // setup framebuffers
    this->setupFramebuffers ();

    CefWindowInfo window_info;
    window_info.SetAsWindowless (0);

    this->m_renderHandler = new WebBrowser::CEF::RenderHandler (this);

    CefBrowserSettings browserSettings;
    // documentaion says that 60 fps is maximum value
    browserSettings.windowless_frame_rate = std::max (60, context.getApp ().getContext ().settings.render.maximumFPS);

    this->m_client = new WebBrowser::CEF::BrowserClient (m_renderHandler);
    // use the custom scheme for the wallpaper's files
    const std::string htmlURL = WPSchemeHandlerFactory::generateSchemeName (this->getWeb ().project.workshopId)
	+ "://root/" + this->getWeb ().filename;
    this->m_browser
	= CefBrowserHost::CreateBrowserSync (window_info, this->m_client, htmlURL, browserSettings, nullptr, nullptr);
}

void CWeb::setSize (const int width, const int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0) {
	return;
    }

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (
	GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );

    // Notify cef that it was resized(maybe it's not even needed)
    this->m_browser->GetHost ()->WasResized ();
}

static std::string jsEscapeString (const std::string& s) {
    std::string out;
    out.reserve (s.size ());
    for (char c : s) {
	if (c == '"') out += "\\\"";
	else if (c == '\\') out += "\\\\";
	else if (c == '\n') out += "\\n";
	else if (c == '\r') out += "\\r";
	else out += c;
    }
    return out;
}

void CWeb::injectProperties () {
    using namespace WallpaperEngine::Data::Model;
    const auto& props = this->getWeb ().project.properties;

    // Install the audio listener shim so wallpaperRegisterAudioListener(cb) captures
    // the callback even if supportsAudioProcessing is false — avoids ReferenceErrors.
    {
	CefRefPtr<CefFrame> frame = this->m_browser->GetMainFrame ();
	if (frame) {
	    const std::string audioShim
		= "(function(){"
		  "window.__muralAudioCallback=null;"
		  "window.wallpaperRegisterAudioListener=function(cb){"
		  "window.__muralAudioCallback=cb;};"
		  "})();";
	    frame->ExecuteJavaScript (audioShim, frame->GetURL (), 0);
	}
    }

    std::ostringstream js;
    js << "(function() {\n"
       << "  if (!window.wallpaperPropertyListener) return;\n"
       << "  if (!window.wallpaperPropertyListener.applyUserProperties) return;\n"
       << "  window.wallpaperPropertyListener.applyUserProperties({\n";

    bool first = true;
    for (const auto& [key, prop] : props) {
	// PropertyText is a display-label with no JS-visible value — skip it
	if (dynamic_cast<const PropertyText*> (prop.get ())) {
	    continue;
	}

	std::string valueStr;
	if (auto* color = dynamic_cast<const PropertyColor*> (prop.get ())) {
	    // WE convention: "r g b" space-separated floats in 0-1 range
	    const auto& v = color->getVec4 ();
	    std::ostringstream ss;
	    ss << v.r << " " << v.g << " " << v.b;
	    valueStr = ss.str ();
	} else {
	    valueStr = prop->toString ();
	}

	if (!first) js << ",\n";
	first = false;
	js << "    \"" << jsEscapeString (key) << "\": {\"value\": \"" << jsEscapeString (valueStr) << "\"}";
    }

    js << "\n  });\n"
       << "})();\n";

    CefRefPtr<CefFrame> frame = this->m_browser->GetMainFrame ();
    if (frame) {
	frame->ExecuteJavaScript (js.str (), frame->GetURL (), 0);
    }

    this->m_propertiesInjected = true;
}

void CWeb::setPropertyValue (const std::string& key, const std::string& value) {
    std::ostringstream js;
    js << "(function() {\n"
       << "  if (!window.wallpaperPropertyListener) return;\n"
       << "  if (!window.wallpaperPropertyListener.applyUserProperties) return;\n"
       << "  window.wallpaperPropertyListener.applyUserProperties({\n"
       << "    \"" << jsEscapeString (key) << "\": {\"value\": \"" << jsEscapeString (value) << "\"}\n"
       << "  });\n"
       << "})();\n";

    CefRefPtr<CefFrame> frame = this->m_browser->GetMainFrame ();
    if (frame) {
	frame->ExecuteJavaScript (js.str (), frame->GetURL (), 0);
    }
}

void CWeb::injectAudio () {
    // Throttle: only inject every other frame to avoid building a 128-float string at 60 fps
    if ((this->m_audioFrameCount++ & 1) != 0) {
	return;
    }

    CefRefPtr<CefFrame> frame = this->m_browser->GetMainFrame ();
    if (!frame) {
	return;
    }

    const auto& recorder = this->getAudioContext ().getRecorder ();

    // Build Float32Array literal with 128 elements: [0..63] left, [64..127] right.
    // lwe does not separate left/right channels — mirror audio64 for both, matching
    // how CPass assigns g_AudioSpectrum64Left == g_AudioSpectrum64Right.
    std::ostringstream js;
    js << "(function(){"
       << "if(!window.__muralAudioCallback)return;"
       << "var a=new Float32Array([";

    for (int i = 0; i < 64; ++i) {
	if (i > 0) js << ',';
	js << recorder.audio64[i];
    }
    for (int i = 0; i < 64; ++i) {
	js << ',' << recorder.audio64[i];
    }

    js << "]);window.__muralAudioCallback(a);})();";

    frame->ExecuteJavaScript (js.str (), frame->GetURL (), 0);
}

void CWeb::renderFrame (const glm::ivec4& viewport) {
    // ensure the viewport matches the window size, and resize if needed
    if (viewport.z != this->getWidth () || viewport.w != this->getHeight ()) {
	this->setSize (viewport.z, viewport.w);
    }

    // inject user properties once, after the page has finished loading
    if (!this->m_propertiesInjected && this->m_client->isPageLoaded ()) {
	this->injectProperties ();
    }

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    // Cef processes all messages, including OnPaint, which renders frame
    // If there is no OnPaint in message loop, we will not update(render) frame
    //  This means some frames will not have OnPaint call in cef messageLoop
    //  Because of that glClear will result in flickering on higher fps
    //  Do not use glClear until some method to control rendering with cef is supported
    // We might actually try to use cef to execute javascript, and not using off-screen rendering at all
    // But for now let it be like this
    //  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CefDoMessageLoopWork ();

    if (this->getWeb ().project.supportsAudioProcessing) {
	this->injectAudio ();
    }
}

void CWeb::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    auto& input = this->getContext ().getInputContext ().getMouseInput ();

    const glm::dvec2 position = input.position ();
    const auto leftClick = input.leftClick ();
    const auto rightClick = input.rightClick ();

    CefMouseEvent evt;
    // Set mouse current position. Maybe clamps are not needed
    evt.x = std::clamp (static_cast<int> (position.x - viewport.x), 0, viewport.z);
    // Convert from OpenGL coordinates (Y=0 at bottom) to CEF coordinates (Y=0 at top)
    evt.y = viewport.w - std::clamp (static_cast<int> (position.y - viewport.y), 0, viewport.w);
    // Send mouse position to cef
    this->m_browser->GetHost ()->SendMouseMoveEvent (evt, false);

    // TODO: ANY OTHER MOUSE EVENTS TO SEND?
    if (leftClick != this->m_leftClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_LEFT,
	    leftClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    if (rightClick != this->m_rightClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_RIGHT,
	    rightClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    this->m_leftClick = leftClick;
    this->m_rightClick = rightClick;
}

CWeb::~CWeb () {
    CefDoMessageLoopWork ();
    this->m_browser->GetHost ()->CloseBrowser (true);

    // m_renderHandler is owned by BrowserClient's CefRefPtr<CefRenderHandler> (established via
    // CEF's own AddRef in BrowserClient's constructor) — it must not be freed manually here, or
    // BrowserClient's later Release() call on its own (now-dangling) reference use-after-frees it.
}
