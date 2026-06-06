#include "EnvironmentAudio.h"

#include <algorithm>

#include "EnvironmentSystem.h"

bool EnvironmentAudio::Init(ma_engine* engine) {
    if (ma_sound_init_from_file(engine, "Sonidos/Lluvia.mp3", 0, nullptr, nullptr, &rain) != MA_SUCCESS) {
        return false;
    }
    if (ma_sound_init_from_file(engine, "Sonidos/Truenos.mp3", 0, nullptr, nullptr, &thunder) != MA_SUCCESS) {
        ma_sound_uninit(&rain);
        return false;
    }

    ma_sound_set_looping(&rain, MA_TRUE);
    ma_sound_set_looping(&thunder, MA_FALSE);
    ma_sound_set_volume(&rain, 0.0f);
    ma_sound_set_volume(&thunder, 0.42f);
    initialized = true;
    return true;
}

void EnvironmentAudio::StartAmbient() {
    if (!initialized || ambientStarted) {
        return;
    }

    ma_sound_start(&rain);
    ambientStarted = true;
}

void EnvironmentAudio::StopAmbient() {
    if (!initialized) {
        return;
    }

    ma_sound_stop(&rain);
    ambientStarted = false;
}

void EnvironmentAudio::Update(const EnvironmentFrame& frame, EnvironmentSystem& environment) {
    if (!initialized) {
        return;
    }

    if (!ambientStarted) {
        StartAmbient();
    }

    const float rainAmount = std::clamp(frame.rainIntensity, 0.0f, 1.0f);
    const float rainVolume = rainAmount * rainAmount * 0.34f;
    ma_sound_set_volume(&rain, rainVolume);

    if (environment.ConsumeLightningEvent() && !ma_sound_is_playing(&thunder)) {
        ma_sound_seek_to_pcm_frame(&thunder, 0);
        ma_sound_start(&thunder);
    }
}

void EnvironmentAudio::Shutdown() {
    if (!initialized) {
        return;
    }

    ma_sound_uninit(&rain);
    ma_sound_uninit(&thunder);
    initialized = false;
    ambientStarted = false;
}
