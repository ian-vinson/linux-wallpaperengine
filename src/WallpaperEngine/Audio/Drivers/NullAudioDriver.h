#pragma once

#include "AudioDriver.h"

namespace WallpaperEngine::Audio::Drivers {
/**
 * No-op audio driver used when no loaded background actually needs real sound playback
 * (e.g. video-only wallpapers, or scenes with no sound object). Avoids opening an idle
 * SDL audio device / backend connection that would otherwise sit unused for the whole
 * session.
 */
class NullAudioDriver final : public AudioDriver {
public:
    NullAudioDriver (
        Application::ApplicationContext& applicationContext, Detectors::AudioPlayingDetector& detector,
        Recorders::PlaybackRecorder& recorder
    );

    /** @inheritdoc */
    int addStream (AudioStream* stream) override;
    /** @inheritdoc */
    void removeStream (int streamId) override;

    /** @inheritdoc */
    [[nodiscard]] AVSampleFormat getFormat () const override;
    /** @inheritdoc */
    [[nodiscard]] int getSampleRate () const override;
    /** @inheritdoc */
    [[nodiscard]] int getChannels () const override;
};
} // namespace WallpaperEngine::Audio::Drivers
