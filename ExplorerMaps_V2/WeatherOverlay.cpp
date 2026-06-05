#include "WeatherOverlay.h"

#include <algorithm>
#include <cmath>

#include "EnvironmentSystem.h"
#include "imgui/imgui.h"

namespace {
    float Fract(float value) {
        return value - std::floor(value);
    }

    float Hash(float seed) {
        return Fract(std::sin(seed * 12.9898f + 78.233f) * 43758.5453f);
    }
}

namespace WeatherOverlay {
    void Draw(const EnvironmentFrame& frame, float currentFrame, float screenWidth, float screenHeight) {
        const float rain = std::clamp(frame.rainIntensity, 0.0f, 1.0f);
        const float lightning = std::clamp(frame.lightningAmount, 0.0f, 1.0f);
        if (rain <= 0.01f && lightning <= 0.01f) {
            return;
        }

        // Aqui se dibuja lluvia ligera y flashes sin crear geometria pesada.
        ImDrawList* drawList = ImGui::GetOverlayDrawList();
        const ImGuiIO& io = ImGui::GetIO();
        const float width = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : screenWidth;
        const float height = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : screenHeight;

        if (lightning > 0.01f) {
            const int alpha = static_cast<int>(std::clamp(lightning * 120.0f, 0.0f, 120.0f));
            drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(width, height), IM_COL32(190, 210, 255, alpha));
        }

        const int streakCount = static_cast<int>(70.0f + rain * 120.0f);
        const ImU32 color = IM_COL32(150, 175, 210, static_cast<int>(65.0f + rain * 95.0f));
        const float fall = currentFrame * (280.0f + rain * 360.0f);
        for (int i = 0; i < streakCount; ++i) {
            const float seed = static_cast<float>(i) + 1.0f;
            const float x = Hash(seed * 1.37f) * width;
            const float baseY = Hash(seed * 2.11f) * height;
            const float y = std::fmod(baseY + fall * (0.45f + Hash(seed) * 0.65f), height + 80.0f) - 40.0f;
            const float length = 14.0f + Hash(seed * 3.3f) * 18.0f;
            drawList->AddLine(ImVec2(x, y), ImVec2(x - length * 0.28f, y + length), color, 1.0f);
        }
    }
}
