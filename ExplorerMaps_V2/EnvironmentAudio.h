#pragma once

#include "miniaudio.h"

struct EnvironmentFrame;
class EnvironmentSystem;

// Funcionalidad: audio de ambiente dia/noche y sonido de rayos.
// Implementacion completa en EnvironmentAudio.cpp.
class EnvironmentAudio {
public:
    bool Init(ma_engine* engine);
    void StartAmbient();
    void StopAmbient();
    void SetCityAmbienceMuted(bool muted);
    void Update(const EnvironmentFrame& frame, EnvironmentSystem& environment);
    void Shutdown();

private:
    ma_sound cityDay{};
    ma_sound cityNight{};
    ma_sound thunder{};
    ma_sound rain{};
    bool initialized = false;
    bool ambientStarted = false;
    bool cityAmbienceMuted = false;
};
