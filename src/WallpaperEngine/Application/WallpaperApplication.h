#pragma once

#include <atomic>
#include <chrono>
#include <random>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"

#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Media/MediaSource.h"

#include <set>

namespace WallpaperEngine::Application {

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 */
class WallpaperApplication {
public:
    explicit WallpaperApplication (ApplicationContext& context);

    /**
     * Prepares the application for rendering.
     */
    void setup ();
    /**
     * Renders a frame of the application.
     */
    void render ();
    /**
     * Cleans up all the resources used by the application.
     */
    static void cleanup ();
    /**
     * Shows the application until it's closed
     */
    void show ();
    /**
     * Handles a OS signal sent to this PID
     *
     * @param signal
     */
    void signal (int signal);
    /**
     * @return Maps screens to loaded backgrounds
     */
    [[nodiscard]] const std::map<std::string, ProjectUniquePtr>& getBackgrounds () const;
    /**
     * @return The current application context
     */
    [[nodiscard]] ApplicationContext& getContext () const;
    /**
     * Renders a frame
     */
    void update (Render::Drivers::Output::OutputViewport* viewport);
    /**
     * Gets the output
     */
    [[nodiscard]] const WallpaperEngine::Render::Drivers::Output::Output& getOutput () const;
    /**
     * Sets the destination framebuffer for rendering. If not called, the default framebuffer will be used.
     */
    void setDestinationFramebuffer (GLuint framebuffer);

    /**
     * Gets the currently set destination framebuffer for rendering. If not set, returns 0 (the default framebuffer).
     */
    [[nodiscard]] GLuint getDestinationFramebuffer () const;

private:
    /**
     * Sets up an asset locator for the given background
     *
     * @param bg
     */
    AssetLocatorUniquePtr setupAssetLocator (const std::string& bg) const;
    /**
     * Initializes subsystems required for application operation
     */
    void initializeSubsystems ();

    /**
     * Loads projects based off the settings
     */
    void loadBackgrounds ();
    /**
     * Loads the given project
     *
     * @param bg
     * @return
     */
    [[nodiscard]] ProjectUniquePtr loadBackground (const std::string& bg);
    /**
     * Prepares all background's values and updates their properties if required
     */
    void setupProperties ();
    /**
     * Updates the properties for the given background based on the current context
     *
     * @param project
     */
    void setupPropertiesForProject (const Project& project);
    /**
     * Checks whether a SIGUSR1-triggered property reload is pending and, if so, re-reads
     * --properties-file, applies any changed values, and dispatches applyUserProperties()
     * to the affected scripts. No-op if no reload is pending.
     */
    void checkPropertyReload ();
    /**
     * Translates an m_backgrounds key into the key RenderContext::getWallpapers() actually uses.
     *
     * For a regular --screen-root background these are the same string. For a --screen-span group,
     * m_backgrounds keys the loaded Project as "span:<firstScreen>", but getWallpapers() registers
     * the group's single shared CWallpaper under each real screen name individually (never under the
     * "span:" key) — so this resolves the m_backgrounds key back to that first real screen name.
     * All screens in a span group point at the identical CWallpaper instance, so any one of them is a
     * valid lookup key.
     *
     * @param backgroundKey a key from m_backgrounds
     * @return the corresponding key to look up in RenderContext::getWallpapers()
     */
    [[nodiscard]] std::string resolveWallpaperLookupKey (const std::string& backgroundKey) const;
    /**
     * Prepares CEF browser to be used
     */
    void setupBrowser ();
    /**
     * Prepares desktop environment-related things (like render, window, fullscreen detector, etc)
     */
    void setupOutput ();
    /**
     * Prepares all audio-related things (like detector, output, etc)
     */
    void setupAudio ();
    /**
     * Prepares the render-context of all the backgrounds so they can be displayed on the screen
     */
    void prepareOutputs ();
    /**
     * Prepares output debugging for all opengl errors
     */
    void setupOpenGLDebugging ();
    /**
     * Takes an screenshot of the background and saves it to the specified path
     *
     * @param filename
     */
    void takeScreenshot (const std::filesystem::path& filename) const;

    /**
     * Whether every currently-active CWeb wallpaper has finished loading its main frame
     * (CefLoadHandler::OnLoadEnd). Always true if there are no web wallpapers active. CEF's page
     * load is async and can take real wall-clock time with no fixed relationship to this app's
     * own render-loop frame count, so --screenshot must not assume a frame-count delay alone
     * means a web wallpaper's content actually exists yet — see m_nextFrameScreenshot's use.
     */
    [[nodiscard]] bool allWebWallpapersLoaded () const;

    struct ActivePlaylist {
	ApplicationContext::PlaylistDefinition definition;
	std::vector<std::size_t> order;
	std::size_t orderIndex = 0;
	std::chrono::steady_clock::time_point nextSwitch;
	std::chrono::steady_clock::time_point lastUpdate;
	std::set<std::size_t> failedIndices;
    };

    void initializePlaylists ();
    void updatePlaylists ();
    void advancePlaylist (
	const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now
    );
    bool selectNextCandidate (ActivePlaylist& playlist, std::size_t& outOrderIndex);
    bool preflightWallpaper (const std::string& path);
    std::vector<std::size_t> buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition);
    void ensureBrowserForProject (const Project& project);
    /**
     * Upgrades the audio driver from the no-op driver to a real SDL-backed one the first time a
     * loaded background actually contains a sound object (e.g. a playlist rotating in a scene
     * wallpaper with sound after starting on a video-only/silent one). Never downgrades, to avoid
     * tearing down live streams.
     *
     * @param project
     */
    void ensureAudioForProject (const Project& project);
    bool makeAnyViewportCurrent () const;

    /** The application context that contains the current app settings */
    ApplicationContext& m_context;
    /** Maps screens to backgrounds */
    std::map<std::string, ProjectUniquePtr> m_backgrounds {};
    std::map<std::string, ActivePlaylist> m_activePlaylists {};

    std::unique_ptr<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> m_audioDetector = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::AudioContext> m_audioContext = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::AudioDriver> m_audioDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> m_audioRecorder = nullptr;
    std::unique_ptr<WallpaperEngine::Render::RenderContext> m_renderContext = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::VideoDriver> m_videoDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> m_fullScreenDetector = nullptr;
    std::unique_ptr<WallpaperEngine::WebBrowser::WebBrowserContext> m_browserContext = nullptr;
    std::unique_ptr<WallpaperEngine::Media::MediaSource> m_mediaSource = nullptr;
    std::mt19937 m_playlistRng { std::random_device {}() };
    std::atomic<bool> m_reloadPropertiesRequested = false;
    bool m_isPaused = false;
    bool m_screenShotTaken = false;
    uint32_t m_nextFrameScreenshot = 0;
    // Hard cap on how many extra frames --screenshot will wait for web wallpapers to finish
    // loading beyond m_nextFrameScreenshot, so a wallpaper whose page never fires OnLoadEnd
    // (broken content, network never resolving, etc.) can't stall the screenshot forever.
    static constexpr uint32_t MAX_WEB_LOAD_WAIT_FRAMES = 600;
    // CefLoadHandler::OnLoadEnd only means the HTML/JS/CSS document itself finished loading —
    // many real pages then do further async work afterward (fetching a 3D model, waiting on a
    // WebGL/canvas init) before anything is actually visible. Give the page this many additional
    // frames after OnLoadEnd fires to let that settle before capturing.
    static constexpr uint32_t WEB_SETTLE_FRAMES = 90;
    // 0 = not yet observed loaded for the current screenshot request; set the first time
    // allWebWallpapersLoaded() returns true, so WEB_SETTLE_FRAMES is measured from actual load
    // completion rather than from process start.
    uint32_t m_webWallpapersLoadedAtFrame = 0;
    std::chrono::steady_clock::time_point m_pauseStart {};
    GLuint m_destinationFramebuffer = 0;
};
} // namespace WallpaperEngine::Application
