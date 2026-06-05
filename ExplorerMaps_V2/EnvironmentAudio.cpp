#include "EnvironmentAudio.h"

#include <algorithm>

#include "EnvironmentSystem.h"

bool EnvironmentAudio::Init(ma_engine* engine) {
    // Aqui se cargan los audios de ambiente y rayos.
    if (ma_sound_init_from_file(engine, "Sonidos/City.mp3", 0, nullptr, nullptr, &cityDay) != MA_SUCCESS) {
        return false;
    }
    if (ma_sound_init_from_file(engine, "Sonidos/City_nigth.mp3", 0, nullptr, nullptr, &cityNight) != MA_SUCCESS) {
        ma_sound_uninit(&cityDay);
        return false;
    }
    if (ma_sound_init_from_file(engine, "Sonidos/Rayos.mp3", 0, nullptr, nullptr, &thunder) != MA_SUCCESS) {
        ma_sound_uninit(&cityNight);
        ma_sound_uninit(&cityDay);
        return false;
    }

    ma_sound_set_looping(&cityDay, MA_TRUE);
    ma_sound_set_looping(&cityNight, MA_TRUE);
    ma_sound_set_looping(&thunder, MA_FALSE);
    ma_sound_set_volume(&cityDay, 0.0f);
    ma_sound_set_volume(&cityNight, 0.0f);
    ma_sound_set_volume(&thunder, 0.85f);
    initialized = true;
    return true;
}

void EnvironmentAudio::StartAmbient() {
    if (!initialized || ambientStarted) {
        return;
    }

    ma_sound_start(&cityDay);
    ma_sound_start(&cityNight);
    ambientStarted = true;
}

void EnvironmentAudio::StopAmbient() {
    if (!initialized) {
        return;
    }

    ma_sound_stop(&cityDay);
    ma_sound_stop(&cityNight);
    ambientStarted = false;
}

void EnvironmentAudio::Update(const EnvironmentFrame& frame, EnvironmentSystem& environment) {
    if (!initialized) {
        return;
    }

    // Aqui se hace crossfade entre ciudad de dia y ciudad de noche.
    const float nightVolume = std::clamp(frame.blendFactor, 0.0f, 1.0f) * 0.42f;
    const float dayVolume = (1.0f - std::clamp(frame.blendFactor, 0.0f, 1.0f)) * 0.38f;
    ma_sound_set_volume(&cityDay, dayVolume);
    ma_sound_set_volume(&cityNight, nightVolume);

    if (environment.ConsumeLightningEvent()) {
        ma_sound_seek_to_pcm_frame(&thunder, 0);
        ma_sound_start(&thunder);
    }
}

void EnvironmentAudio::Shutdown() {
    if (!initialized) {
        return;
    }

    ma_sound_uninit(&cityDay);
    ma_sound_uninit(&cityNight);
    ma_sound_uninit(&thunder);
    initialized = false;
    ambientStarted = false;
}
