#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sstream>
#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/Render/RenderContext.h"
#include "WallpaperEngine/Testing/Render/TestingOpenGLDriver.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Testing::Render;

namespace {
class NullMediaSource final : public Media::MediaSource {
public:
    using MediaSource::MediaSource;

protected:
    void performUpdate () override { }
};

std::size_t countOccurrences (const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;

    while ((pos = haystack.find (needle, pos)) != std::string::npos) {
	count++;
	pos += needle.size ();
    }

    return count;
}
} // namespace

TEST_CASE ("RenderContext::render logs once when a viewport has no mapped wallpaper") {
    // a minimal on-disk "video" project -- the simplest wallpaper type to parse, since
    // WallpaperParser::parseVideo() only stores the filename string without opening it
    const std::string fixture =
	(std::filesystem::path (__FILE__).parent_path () / "Fixtures/MinimalVideoWallpaper").string ();

    char emptyArg[] = "";
    char bgFlag[] = "--bg";
    std::vector<char> fixtureArg (fixture.begin (), fixture.end ());
    fixtureArg.push_back ('\0');
    char* argv[] = { emptyArg, bgFlag, fixtureArg.data () };

    ApplicationContext context (3, argv);
    context.loadSettingsFromArgv ();
    WallpaperApplication app (context);
    TestingOpenGLDriver driver (context, app);
    NullMediaSource mediaSource (std::chrono::milliseconds (1000));
    RenderContext renderContext (driver, app, mediaSource);

    // capture error output for the rest of this test binary's life -- Log has no
    // removeError(), so the stream must outlive every future sLog.error() call
    static auto* capture = new std::ostringstream ();
    sLog.addError (capture);

    const auto& viewports = driver.getOutput ().getViewports ();
    REQUIRE (viewports.find ("default") != viewports.end ());

    auto* viewport = viewports.at ("default");

    // no wallpaper was ever registered for "default" via setWallpaper(), so this must
    // hit the missing-mapping branch -- render it twice to confirm the log fires once, not per-frame
    renderContext.render (viewport);
    renderContext.render (viewport);

    const std::string output = capture->str ();

    REQUIRE (output.find ("no wallpaper mapped") != std::string::npos);
    REQUIRE (output.find ("default") != std::string::npos);
    REQUIRE (countOccurrences (output, "no wallpaper mapped") == 1);
}
