#pragma once

struct ImFont;

class LoadingScreen {
public:
    void Initialize();
    void Draw(float progress, float currentTime);
    void DrawReturnToMenu(float progress);

private:
    ImFont* titleFont = nullptr;
    ImFont* bodyFont = nullptr;
    float displayedProgress = 0.0f;
};
