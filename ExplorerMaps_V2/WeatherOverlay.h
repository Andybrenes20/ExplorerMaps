#pragma once

struct EnvironmentFrame;

// Funcionalidad: lluvia visible y flash de rayos sobre la pantalla.
// Implementacion completa en WeatherOverlay.cpp.
namespace WeatherOverlay {
    void Draw(const EnvironmentFrame& frame, float currentFrame, float screenWidth, float screenHeight);
}
