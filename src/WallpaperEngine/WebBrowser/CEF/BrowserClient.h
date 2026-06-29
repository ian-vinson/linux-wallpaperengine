#pragma once

#include "include/cef_client.h"
#include "include/cef_load_handler.h"

namespace WallpaperEngine::WebBrowser::CEF {
// *************************************************************************
//! \brief Provide access to browser-instance-specific callbacks. A single
//! CefClient instance can be shared among any number of browsers.
// *************************************************************************
class BrowserClient : public CefClient, public CefLoadHandler {
public:
    explicit BrowserClient (CefRefPtr<CefRenderHandler> ptr);

    [[nodiscard]] CefRefPtr<CefRenderHandler> GetRenderHandler () override;
    [[nodiscard]] CefRefPtr<CefLoadHandler> GetLoadHandler () override;

    void OnLoadEnd (CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;

    [[nodiscard]] bool isPageLoaded () const { return m_pageLoaded; }

    CefRefPtr<CefRenderHandler> m_renderHandler = nullptr;

private:
    bool m_pageLoaded = false;

    IMPLEMENT_REFCOUNTING (BrowserClient);
};
} // namespace WallpaperEngine::WebBrowser::CEF