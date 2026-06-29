#include "BrowserClient.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserClient::BrowserClient (CefRefPtr<CefRenderHandler> ptr) : m_renderHandler (std::move (ptr)) { }

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler () { return m_renderHandler; }

CefRefPtr<CefLoadHandler> BrowserClient::GetLoadHandler () { return this; }

void BrowserClient::OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
    if (frame->IsMain ()) {
        m_pageLoaded = true;
    }
}