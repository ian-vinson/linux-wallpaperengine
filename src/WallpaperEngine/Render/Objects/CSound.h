#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Scripting;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

class CSound final : virtual public CObject, public ScriptableObject {
public:
    CSound (Wallpapers::CScene& scene, const Sound& sound);
    ~CSound () override;

    void render () override;

    // ISoundLayer scripting surface (thisLayer.isPlaying()/play()/stop()/pause(), see
    // ScriptableObjectAdapter.cpp; `volume` is exposed as a plain registered property instead,
    // re-applied to every managed stream each frame in render()).
    [[nodiscard]] bool isPlaying () const;
    void play ();
    void stop ();
    void pause ();

protected:
    void load ();

private:
    // Distinct from AudioStream's own isInitialized()/isPaused(): AudioStream has no notion of
    // "stopped vs. never started", and pausing must not be confused with stopping.
    enum class PlaybackState { Stopped, Playing, Paused };

    std::map<int, Audio::AudioStream*> m_audioStreams = {};

    const Sound& m_sound;
    PlaybackState m_state = PlaybackState::Stopped;
};
} // namespace WallpaperEngine::Render::Objects
