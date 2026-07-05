#include <SDL.h>
#include <ranges>

#include "CSound.h"

#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene& scene, const Sound& sound) :
    CObject (scene, sound), ScriptableObject (scene, sound), m_sound (sound) {
    this->registerProperty ("volume", *sound.volume->value);

    if (this->getContext ().getApp ().getContext ().settings.audio.enabled) {
	this->load ();
	this->m_state = PlaybackState::Playing;
    }
}

CSound::~CSound () { this->stop (); }

void CSound::load () {
    for (const auto& cur : this->m_sound.sounds) {
	auto stream
	    = new Audio::AudioStream (this->getScene ().getAudioContext (), this->getAssetLocator ().read (cur));

	stream->setRepeat (this->m_sound.playbackmode.has_value () && this->m_sound.playbackmode == "loop");
	stream->setVolume (this->m_sound.volume->value->getFloat ());

	// add the stream to the context so it can be played
	this->m_audioStreams.insert_or_assign (this->getScene ().getAudioContext ().addStream (stream), stream);
    }
}

void CSound::render () {
    // re-apply the (possibly script-changed) volume property to every managed stream every
    // frame — mirrors how other ScriptableObject-derived objects re-read transform properties
    // fresh each frame rather than caching them once.
    const float currentVolume = this->m_sound.volume->value->getFloat ();

    for (const auto& stream : this->m_audioStreams | std::views::values) {
	stream->setVolume (currentVolume);
    }
}

bool CSound::isPlaying () const { return this->m_state == PlaybackState::Playing; }

void CSound::play () {
    if (this->m_state == PlaybackState::Playing) {
	return;
    }

    // a script's play() call must not override the user's global audio-disabled preference
    if (!this->getContext ().getApp ().getContext ().settings.audio.enabled) {
	return;
    }

    if (this->m_state == PlaybackState::Paused) {
	// resume every stream exactly where it left off — no rebuild needed, pause() never
	// touched decode/queue state
	for (const auto& stream : this->m_audioStreams | std::views::values) {
	    stream->resume ();
	}
    } else {
	// AudioStream has no restart path once stopped (m_initialized never flips back to
	// true) — rebuild fresh instances via the same load() the constructor uses
	this->load ();
    }

    this->m_state = PlaybackState::Playing;
}

void CSound::stop () {
    if (this->m_state == PlaybackState::Stopped) {
	return;
    }

    // free all the sound buffers and streams
    for (const auto& [id, stream] : this->m_audioStreams) {
	this->getScene ().getAudioContext ().removeStream (id);
	stream->stop ();
	delete stream;
    }

    this->m_audioStreams.clear ();
    this->m_state = PlaybackState::Stopped;
}

void CSound::pause () {
    // pausing only makes sense while actually playing — a no-op while stopped (nothing to
    // pause) or already paused (idempotent)
    if (this->m_state != PlaybackState::Playing) {
	return;
    }

    for (const auto& stream : this->m_audioStreams | std::views::values) {
	stream->pause ();
    }

    this->m_state = PlaybackState::Paused;
}
