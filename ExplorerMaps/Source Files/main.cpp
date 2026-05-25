#define _CRT_SECURE_NO_WARNINGS

//------- Ignore this ----------
#include<algorithm>
#include<filesystem>
#include<iomanip>
#include<sstream>
#include<string>
#include<vector>
#include<stb/stb_easy_font.h>
namespace fs = std::filesystem;
//------------------------------

#ifdef _WIN32
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

#include "Model.h"
#include "Skybox.h"

// --- Ventana / escena ----------------------------------
const unsigned int width = 1920;
const unsigned int height = 1080;
const float targetSceneRadius = 1800.0f;
const float cameraFov = 55.0f;
const float cameraNearPlane = 0.05f;
const float cameraFarPlane = 6000.0f;
const float celestialOrbitRadius = 2800.0f;
const float maxSunHeight = 2000.0f;

// --- Caminar -------------------------------------------
const float walkEyeHeight = 6.0f;
const float walkProbeRadius = 8.0f;
const float walkMaxStepUp = 12.0f;
const float walkMaxDropDown = 45.0f;
const float walkMaxSlopeDegrees = 68.0f;
const float walkSpeed = 30.0f;

// --- Opciones ------------------------------------------
const bool showCoordinatesInWindowTitle = true;
const bool useFastRenderMode = false;
const glm::vec3 blockedZoneMin = glm::vec3(240.0f, -260.0f, 360.0f);
const glm::vec3 blockedZoneMax = glm::vec3(310.0f, -150.0f, 435.0f);

// CICLO MUY RAPIDO PARA VIDEO
const float dayNightSpeed = 0.20f;

const float SUN_SIZE = 90.0f;
const float MOON_SIZE = 65.0f;

enum class EnvironmentMode
{
    Auto,
    Day,
    Night
};

struct EnvironmentMenuState
{
    bool open = false;
    int selection = 0;
    bool tabWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool enterWasDown = false;
    bool escapeWasDown = false;
    bool gamepadOptionsWasDown = false;
    bool gamepadUpWasDown = false;
    bool gamepadDownWasDown = false;
    bool gamepadAcceptWasDown = false;
    bool gamepadCancelWasDown = false;
    bool mouseLeftWasDown = false;
};

constexpr int kEnvironmentMenuItemCount = 4;
constexpr int kEnvironmentMenuExitIndex = 3;

bool IsInsideRect(double px, double py, float x, float y, float w, float h)
{
    return px >= x && px <= (x + w) && py >= y && py <= (y + h);
}

int GetEnvironmentMenuRowAt(double mouseX, double mouseY)
{
    const float panelX = 580.0f;
    const float panelY = 245.0f;
    const float rowX = panelX + 76.0f;
    const float rowW = 608.0f;

    for (int i = 0; i < kEnvironmentMenuItemCount; ++i)
    {
        const float rowY = panelY + 172.0f + i * 78.0f;
        if (IsInsideRect(mouseX, mouseY, rowX, rowY, rowW, 58.0f))
            return i;
    }

    return -1;
}

const char* EnvironmentModeName(EnvironmentMode mode)
{
    switch (mode)
    {
    case EnvironmentMode::Day: return "DIA";
    case EnvironmentMode::Night: return "NOCHE";
    default: return "AUTO";
    }
}

EnvironmentMode EnvironmentModeFromSelection(int selection)
{
    switch (selection)
    {
    case 0: return EnvironmentMode::Day;
    case 1: return EnvironmentMode::Night;
    case 2: return EnvironmentMode::Auto;
    default: return EnvironmentMode::Auto;
    }
}

int EnvironmentSelectionFromMode(EnvironmentMode mode)
{
    switch (mode)
    {
    case EnvironmentMode::Day: return 0;
    case EnvironmentMode::Night: return 1;
    default: return 2;
    }
}

EnvironmentMode ResolveSkyboxMode(EnvironmentMode environmentMode, bool isDay)
{
    if (environmentMode == EnvironmentMode::Day)
        return EnvironmentMode::Day;

    if (environmentMode == EnvironmentMode::Night)
        return EnvironmentMode::Night;

    return isDay ? EnvironmentMode::Day : EnvironmentMode::Night;
}

bool HandleEnvironmentMenu(GLFWwindow* window, EnvironmentMenuState& menu, EnvironmentMode& mode, bool& shouldExit)
{
    const bool tabDown = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    const bool upDown = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    const bool downDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    const bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const bool mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool gamepadOptionsDown = false;
    bool gamepadUpDown = false;
    bool gamepadDownDown = false;
    bool gamepadAcceptDown = false;
    bool gamepadCancelDown = false;
    bool modeChanged = false;

    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
    {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        {
            const float stickDeadzone = 0.55f;
            gamepadOptionsDown = state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
            gamepadUpDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -stickDeadzone;
            gamepadDownDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > stickDeadzone;
            gamepadAcceptDown = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            gamepadCancelDown = state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        }
    }

    if ((tabDown && !menu.tabWasDown) || (gamepadOptionsDown && !menu.gamepadOptionsWasDown))
    {
        menu.open = !menu.open;
        menu.selection = EnvironmentSelectionFromMode(mode);
    }

    if (menu.open)
    {
        if ((upDown && !menu.upWasDown) || (gamepadUpDown && !menu.gamepadUpWasDown))
            menu.selection = (menu.selection + kEnvironmentMenuItemCount - 1) % kEnvironmentMenuItemCount;

        if ((downDown && !menu.downWasDown) || (gamepadDownDown && !menu.gamepadDownWasDown))
            menu.selection = (menu.selection + 1) % kEnvironmentMenuItemCount;

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        const int hoveredRow = GetEnvironmentMenuRowAt(mouseX, mouseY);
        if (hoveredRow >= 0)
            menu.selection = hoveredRow;

        if ((enterDown && !menu.enterWasDown) || (gamepadAcceptDown && !menu.gamepadAcceptWasDown))
        {
            if (menu.selection == kEnvironmentMenuExitIndex)
            {
                shouldExit = true;
                menu.open = false;
            }
            else
            {
                mode = EnvironmentModeFromSelection(menu.selection);
                menu.open = false;
                modeChanged = true;
            }
        }

        if (hoveredRow >= 0 && mouseLeftDown && !menu.mouseLeftWasDown)
        {
            if (hoveredRow == kEnvironmentMenuExitIndex)
            {
                shouldExit = true;
                menu.open = false;
            }
            else
            {
                menu.selection = hoveredRow;
                mode = EnvironmentModeFromSelection(hoveredRow);
                menu.open = false;
                modeChanged = true;
            }
        }

        if ((escapeDown && !menu.escapeWasDown) || (gamepadCancelDown && !menu.gamepadCancelWasDown))
            menu.open = false;
    }

    menu.tabWasDown = tabDown;
    menu.upWasDown = upDown;
    menu.downWasDown = downDown;
    menu.enterWasDown = enterDown;
    menu.escapeWasDown = escapeDown;
    menu.gamepadOptionsWasDown = gamepadOptionsDown;
    menu.gamepadUpWasDown = gamepadUpDown;
    menu.gamepadDownWasDown = gamepadDownDown;
    menu.gamepadAcceptWasDown = gamepadAcceptDown;
    menu.gamepadCancelWasDown = gamepadCancelDown;
    menu.mouseLeftWasDown = mouseLeftDown;

    return modeChanged;
}

void ApplyEnvironmentMode(EnvironmentMode mode, float& sunAngleRad, float& sunHeight)
{
    if (mode == EnvironmentMode::Day)
    {
        sunAngleRad = 1.5708f;
        sunHeight = 1.0f;
    }
    else if (mode == EnvironmentMode::Night)
    {
        sunAngleRad = 4.71239f;
        sunHeight = -1.0f;
    }
}

std::string BuildEnvironmentTitle(const EnvironmentMenuState& menu, EnvironmentMode mode, const glm::vec3& normPos)
{
    std::ostringstream title;
    title << std::fixed << std::setprecision(3);

    if (menu.open)
    {
        const char* option0 = menu.selection == 0 ? "> DIA" : "  DIA";
        const char* option1 = menu.selection == 1 ? "> NOCHE" : "  NOCHE";
        const char* option2 = menu.selection == 2 ? "> AUTO" : "  AUTO";
        const char* option3 = menu.selection == 3 ? "> SALIR" : "  SALIR";
        title << "MENU AMBIENTE | " << option0 << " | " << option1 << " | " << option2 << " | " << option3
              << " | Flechas/W/S: mover | Enter: aplicar | Tab/Esc: cerrar";
    }
    else
    {
        title << "Ambiente: " << EnvironmentModeName(mode)
              << " | Tab: menu | X:" << normPos.x << " Y:" << normPos.y << " Z:" << normPos.z;
    }

    return title.str();
}

struct OverlayVertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

void AddOverlayRect(std::vector<OverlayVertex>& vertices, float x, float y, float w, float h, const glm::vec4& color)
{
    const float x0 = (x / width) * 2.0f - 1.0f;
    const float y0 = 1.0f - (y / height) * 2.0f;
    const float x1 = ((x + w) / width) * 2.0f - 1.0f;
    const float y1 = 1.0f - ((y + h) / height) * 2.0f;

    vertices.push_back({ x0, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y1, color.r, color.g, color.b, color.a });
    vertices.push_back({ x0, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y1, color.r, color.g, color.b, color.a });
    vertices.push_back({ x0, y1, color.r, color.g, color.b, color.a });
}

void AddOverlayText(std::vector<OverlayVertex>& vertices, const std::string& text, float x, float y, float scale, const glm::vec4& color)
{
    if (text.empty())
        return;

    struct EasyFontVertex
    {
        float x;
        float y;
        float z;
        unsigned char color[4];
    };

    const float fontScale = scale * 2.8f;
    std::vector<char> printableText(text.begin(), text.end());
    printableText.push_back('\0');

    std::vector<char> textBuffer((text.size() + 1) * 512, 0);
    const int quadCount = stb_easy_font_print(0.0f, 0.0f, printableText.data(), nullptr, textBuffer.data(), static_cast<int>(textBuffer.size()));
    const auto* glyphVertices = reinterpret_cast<const EasyFontVertex*>(textBuffer.data());

    for (int i = 0; i < quadCount; ++i)
    {
        const EasyFontVertex& v0 = glyphVertices[i * 4 + 0];
        const EasyFontVertex& v1 = glyphVertices[i * 4 + 1];
        const EasyFontVertex& v2 = glyphVertices[i * 4 + 2];

        AddOverlayRect(
            vertices,
            x + v0.x * fontScale,
            y + v0.y * fontScale,
            (v1.x - v0.x) * fontScale,
            (v2.y - v0.y) * fontScale,
            color
        );
    }
}

void DrawEnvironmentMenu(const EnvironmentMenuState& menu, EnvironmentMode mode, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    std::vector<OverlayVertex> vertices;

    const glm::vec4 panelColor(0.015f, 0.018f, 0.026f, 0.88f);
    const glm::vec4 panelEdge(0.95f, 0.76f, 0.28f, 0.95f);
    const glm::vec4 rowColor(0.055f, 0.065f, 0.085f, 0.88f);
    const glm::vec4 rowSelected(0.16f, 0.28f, 0.40f, 0.96f);
    const glm::vec4 textMain(0.92f, 0.90f, 0.82f, 1.0f);
    const glm::vec4 textMuted(0.52f, 0.60f, 0.70f, 1.0f);
    const glm::vec4 accent(0.98f, 0.78f, 0.24f, 1.0f);

    if (menu.open)
    {
        const float panelX = 580.0f;
        const float panelY = 245.0f;
        const float panelW = 760.0f;
        const float panelH = 560.0f;

        AddOverlayRect(vertices, panelX + 10.0f, panelY + 12.0f, panelW, panelH, glm::vec4(0.0f, 0.0f, 0.0f, 0.28f));
        AddOverlayRect(vertices, panelX, panelY, panelW, panelH, panelColor);
        AddOverlayRect(vertices, panelX, panelY, panelW, 5.0f, panelEdge);
        AddOverlayRect(vertices, panelX, panelY + panelH - 5.0f, panelW, 5.0f, glm::vec4(0.08f, 0.10f, 0.14f, 0.90f));

        AddOverlayText(vertices, "AMBIENTE", panelX + 76.0f, panelY + 58.0f, 1.7f, textMain);
        AddOverlayText(vertices, "SELECCIONA SKYBOX", panelX + 76.0f, panelY + 112.0f, 0.92f, textMuted);

        const char* labels[kEnvironmentMenuItemCount] = { "DIA", "NOCHE", "AUTO", "SALIR" };
        for (int i = 0; i < kEnvironmentMenuItemCount; ++i)
        {
            const float rowX = panelX + 76.0f;
            const float rowY = panelY + 172.0f + i * 78.0f;
            const bool selected = menu.selection == i;
            const bool isExit = i == kEnvironmentMenuExitIndex;
            const bool active = !isExit && EnvironmentSelectionFromMode(mode) == i;

            const glm::vec4 currentRowColor = isExit
                ? (selected ? glm::vec4(0.32f, 0.18f, 0.18f, 0.96f) : glm::vec4(0.14f, 0.07f, 0.07f, 0.88f))
                : (selected ? rowSelected : rowColor);
            const glm::vec4 currentStripeColor = isExit
                ? (selected ? glm::vec4(1.0f, 0.48f, 0.38f, 1.0f) : glm::vec4(0.34f, 0.16f, 0.16f, 1.0f))
                : (selected ? accent : glm::vec4(0.18f, 0.22f, 0.28f, 1.0f));

            AddOverlayRect(vertices, rowX, rowY, 608.0f, 58.0f, currentRowColor);
            AddOverlayRect(vertices, rowX, rowY, 5.0f, 58.0f, currentStripeColor);

            if (selected)
                AddOverlayText(vertices, ">", rowX + 28.0f, rowY + 17.0f, 1.4f, isExit ? glm::vec4(1.0f, 0.48f, 0.38f, 1.0f) : accent);

            AddOverlayText(vertices, labels[i], rowX + 86.0f, rowY + 17.0f, 1.35f, isExit
                ? (selected ? glm::vec4(1.0f, 0.90f, 0.86f, 1.0f) : glm::vec4(0.88f, 0.70f, 0.70f, 1.0f))
                : (selected ? glm::vec4(1.0f, 0.96f, 0.82f, 1.0f) : glm::vec4(0.74f, 0.80f, 0.88f, 1.0f)));

            if (active)
                AddOverlayText(vertices, "ACTIVO", rowX + 410.0f, rowY + 21.0f, 0.80f, selected ? accent : textMuted);
        }

        AddOverlayRect(vertices, panelX + 76.0f, panelY + 500.0f, 608.0f, 1.5f, glm::vec4(0.22f, 0.25f, 0.31f, 0.9f));
        AddOverlayText(vertices, "W/S PAD MOVER", panelX + 92.0f, panelY + 518.0f, 0.58f, textMuted);
        AddOverlayText(vertices, "CLICK APLICAR", panelX + 300.0f, panelY + 518.0f, 0.56f, textMuted);
        AddOverlayText(vertices, "OPTIONS/B CERRAR", panelX + 500.0f, panelY + 518.0f, 0.52f, textMuted);
    }
    else
    {
        AddOverlayRect(vertices, 32.0f, 28.0f, 460.0f, 46.0f, glm::vec4(0.015f, 0.018f, 0.026f, 0.70f));
        AddOverlayRect(vertices, 32.0f, 28.0f, 4.0f, 46.0f, accent);
        AddOverlayText(vertices, std::string("AMBIENTE: ") + EnvironmentModeName(mode), 52.0f, 43.0f, 0.86f, textMain);
        AddOverlayText(vertices, "TAB", 412.0f, 43.0f, 0.72f, accent);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    overlayShader.Activate();
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(OverlayVertex), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
std::vector<std::string> GetSkyboxFacePaths(EnvironmentMode mode)
{
    const char* folder = mode == EnvironmentMode::Night
        ? "Texturas/Skybox_Nigth"
        : "Texturas/Skybox_Day";

    return {
        std::string(folder) + "/px.png",
        std::string(folder) + "/nx.png",
        std::string(folder) + "/py.png",
        std::string(folder) + "/ny.png",
        std::string(folder) + "/pz.png",
        std::string(folder) + "/nz.png"
    };
}
void createSphere(GLuint& VAO, GLuint& VBO, GLuint& EBO, int sectors, float radius) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float x, y, z, xy;
    float nx, ny, nz;
    float sectorStep = 2 * 3.14159f / sectors;
    float stackStep = 3.14159f / sectors;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= sectors; ++i) {
        stackAngle = 3.14159f / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            nx = x / radius;
            ny = y / radius;
            nz = z / radius;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    for (int i = 0; i < sectors; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (sectors - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Proyecto", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    gladLoadGL();
    glViewport(0, 0, width, height);

    Shader shaderProgram("Shaders/default.vert", "Shaders/default.frag");
    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader sphereShader("Shaders/sphere.vert", "Shaders/sphere.frag");
    Shader overlayShader("Shaders/menu.vert", "Shaders/menu.frag");
    Texture sunTexture("Texturas/sun.jpg", "diffuse", 0);
    Texture moonTexture("Texturas/moon.jpg", "diffuse", 0);
    sphereShader.Activate();
    sunTexture.texUnit(sphereShader, "sphereTexture", 0);

    GLuint sunVAO, sunVBO, sunEBO;
    GLuint moonVAO, moonVBO, moonEBO;
    GLuint overlayVAO, overlayVBO;
    glGenVertexArrays(1, &overlayVAO);
    glGenBuffers(1, &overlayVBO);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    createSphere(sunVAO, sunVBO, sunEBO, 48, SUN_SIZE);
    createSphere(moonVAO, moonVBO, moonEBO, 36, MOON_SIZE);

    Model model("modelos/city.glb");
    Skybox skybox(
        GetSkyboxFacePaths(EnvironmentMode::Day),
        "Shaders/skybox_cubemap.vert",
        "Shaders/skybox_cubemap.frag"
    );

    glm::vec3 modelCenter = model.GetCenter();
    float modelRadius = model.GetRadius();
    if (modelRadius < 1.0f) modelRadius = 1.0f;

    const float normalizationScale = targetSceneRadius / modelRadius;
    const glm::mat4 modelTransform =
        glm::scale(glm::mat4(1.0f), glm::vec3(normalizationScale)) *
        glm::translate(glm::mat4(1.0f), -modelCenter);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Camera camera(width, height, glm::vec3(112.0f, -179.0f, 600.0f));
    camera.Orientation = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
    camera.speed = walkSpeed;
    camera.flyMode = false;

    float lastFrame = 0.0f;
    float lastTitleUpdate = 0.0f;
    glm::vec3 lastTitlePosition = camera.Position;

    glm::vec3 snappedStart;
    if (model.TrySnapToWalkableSurface(
        camera.Position, modelTransform,
        walkProbeRadius, walkEyeHeight,
        walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
        snappedStart))
    {
        camera.Position = snappedStart;
        lastTitlePosition = camera.Position;
    }

    float timeOfDayAngle = 0.0f;
    float sunHeight = 0.0f;
    float dayFactor = 0.0f;
    float nightFactor = 0.0f;

    glm::vec3 sunPos;
    glm::vec3 moonPos;
    bool isDay = true;

    EnvironmentMode environmentMode = EnvironmentMode::Auto;
    EnvironmentMode activeSkyboxMode = EnvironmentMode::Day;
    EnvironmentMenuState environmentMenu;
    bool menuCursorVisible = false;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !environmentMenu.open)
        {
            glfwSetWindowShouldClose(window, true);
        }

        bool shouldExit = false;
        HandleEnvironmentMenu(window, environmentMenu, environmentMode, shouldExit);
        if (shouldExit)
        {
            glfwSetWindowShouldClose(window, true);
        }

        if (environmentMenu.open && !menuCursorVisible)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            menuCursorVisible = true;
            camera.firstClick = true;
        }
        else if (!environmentMenu.open && menuCursorVisible)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            menuCursorVisible = false;
            camera.firstClick = true;
        }

        timeOfDayAngle = currentFrame * dayNightSpeed;
        float sunAngleRad = timeOfDayAngle;

        sunHeight = std::sin(sunAngleRad);
        ApplyEnvironmentMode(environmentMode, sunAngleRad, sunHeight);
        isDay = sunHeight > 0.0f;

        const EnvironmentMode requestedSkyboxMode = ResolveSkyboxMode(environmentMode, isDay);
        if (requestedSkyboxMode != activeSkyboxMode)
        {
            skybox.SetFaces(GetSkyboxFacePaths(requestedSkyboxMode));
            activeSkyboxMode = requestedSkyboxMode;
        }

        dayFactor = glm::clamp(sunHeight + 0.2f, 0.05f, 1.0f);
        nightFactor = glm::clamp(-sunHeight + 0.1f, 0.0f, 1.0f);

        float sunHorizontal = std::cos(sunAngleRad);
        float sunVertical = sunHeight;

        if (sunVertical > 0.0f) {
            sunPos = glm::vec3(
                sunHorizontal * celestialOrbitRadius,
                sunVertical * maxSunHeight,
                std::sin(sunAngleRad) * celestialOrbitRadius * 0.5f
            );
        }
        else {
            sunPos = glm::vec3(
                sunHorizontal * celestialOrbitRadius,
                -500.0f,
                std::sin(sunAngleRad) * celestialOrbitRadius * 0.5f
            );
        }

        float moonAngleRad = sunAngleRad + 3.14159f;
        float moonHorizontal = std::cos(moonAngleRad);
        float moonVertical = std::sin(moonAngleRad);

        if (moonVertical > 0.0f) {
            moonPos = glm::vec3(
                moonHorizontal * celestialOrbitRadius * 0.8f,
                moonVertical * maxSunHeight * 0.6f,
                std::sin(moonAngleRad) * celestialOrbitRadius * 0.4f
            );
        }
        else {
            moonPos = glm::vec3(
                moonHorizontal * celestialOrbitRadius * 0.8f,
                -300.0f,
                std::sin(moonAngleRad) * celestialOrbitRadius * 0.4f
            );
        }

        glm::vec3 mainLightPos;
        glm::vec3 mainLightColor;
        float mainLightIntensity;
        glm::vec3 ambientColor;
        float diffuseIntensity;
        float specularIntensity;

        if (isDay) {
            mainLightPos = sunPos;
            float lightPower = glm::clamp(sunHeight * 1.2f, 0.1f, 1.0f);

            if (sunHeight > 0.7f) {
                mainLightColor = glm::vec3(1.0f, 0.98f, 0.92f);
                mainLightIntensity = 1.2f * lightPower;
                ambientColor = glm::vec3(0.35f, 0.38f, 0.42f);
                diffuseIntensity = 1.0f;
                specularIntensity = 0.6f;
            }
            else if (sunHeight > 0.4f) {
                float t = (sunHeight - 0.4f) / 0.3f;
                mainLightColor = glm::mix(
                    glm::vec3(1.0f, 0.75f, 0.55f),
                    glm::vec3(1.0f, 0.98f, 0.92f),
                    t
                );
                mainLightIntensity = 0.9f * lightPower;
                ambientColor = glm::vec3(0.28f, 0.30f, 0.32f);
                diffuseIntensity = 0.8f;
                specularIntensity = 0.5f;
            }
            else if (sunHeight > 0.1f) {
                float t = (sunHeight - 0.1f) / 0.3f;
                mainLightColor = glm::mix(
                    glm::vec3(1.0f, 0.45f, 0.25f),
                    glm::vec3(1.0f, 0.75f, 0.55f),
                    t
                );
                mainLightIntensity = 0.5f * lightPower;
                ambientColor = glm::vec3(0.18f, 0.16f, 0.20f);
                diffuseIntensity = 0.5f;
                specularIntensity = 0.3f;
            }
            else {
                mainLightColor = glm::vec3(0.9f, 0.45f, 0.30f);
                mainLightIntensity = 0.25f * lightPower;
                ambientColor = glm::vec3(0.08f, 0.07f, 0.10f);
                diffuseIntensity = 0.25f;
                specularIntensity = 0.15f;
            }
        }
        else {
            // NOCHE - Luna ilumina un poco la ciudad (20-35% de intensidad)
            mainLightPos = moonPos;
            float moonHeight = std::abs(moonVertical);
            // Intensidad de la luna: entre 20% y 35% para que se vea la ciudad
            float moonLightPower = glm::clamp(moonHeight * 0.4f, 0.20f, 0.35f);

            mainLightColor = glm::vec3(0.65f, 0.70f, 0.90f);
            mainLightIntensity = moonLightPower;

            // Ambiente suficiente para ver la ciudad
            ambientColor = glm::vec3(0.08f, 0.10f, 0.15f);
            diffuseIntensity = 0.35f;
            specularIntensity = 0.15f;

            // Noche mas oscura pero siempre visible
            if (sunHeight < -0.6f) {
                ambientColor = glm::vec3(0.05f, 0.06f, 0.08f);
                mainLightIntensity = 0.20f;
                diffuseIntensity = 0.25f;
            }
        }

        glm::vec4 lightColor = glm::vec4(mainLightColor * mainLightIntensity, 1.0f);

        glm::vec3 skyColor;
        if (isDay) {
            if (sunHeight > 0.6f) {
                skyColor = glm::vec3(0.45f, 0.75f, 1.0f);
            }
            else if (sunHeight > 0.2f) {
                float t = (sunHeight - 0.2f) / 0.4f;
                skyColor = glm::mix(
                    glm::vec3(0.85f, 0.55f, 0.45f),
                    glm::vec3(0.45f, 0.75f, 1.0f),
                    t
                );
            }
            else if (sunHeight > 0.0f) {
                float t = sunHeight / 0.2f;
                skyColor = glm::mix(
                    glm::vec3(0.08f, 0.06f, 0.15f),
                    glm::vec3(0.85f, 0.55f, 0.45f),
                    t
                );
            }
            else {
                skyColor = glm::vec3(0.04f, 0.03f, 0.08f);
            }
        }
        else {
            // Cielo nocturno visible (no completamente negro)
            skyColor = glm::vec3(0.03f, 0.03f, 0.06f);
            float moonHeight = std::abs(moonVertical);
            skyColor += mainLightColor * (0.03f + moonHeight * 0.03f);
        }

        glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::vec3 prevPos = camera.Position;
        if (!environmentMenu.open)
            camera.Inputs(window, deltaTime);

        if (!camera.flyMode)
        {
            glm::vec3 snapped;
            if (model.TrySnapToWalkableSurface(
                camera.Position, modelTransform,
                walkProbeRadius, walkEyeHeight,
                walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
                snapped))
                camera.Position = snapped;
            else if (model.TrySnapToWalkableSurface(
                prevPos, modelTransform,
                walkProbeRadius, walkEyeHeight,
                walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
                snapped))
                camera.Position = snapped;
            else
                camera.Position = prevPos;

            const bool blocked =
                camera.Position.x >= blockedZoneMin.x && camera.Position.x <= blockedZoneMax.x &&
                camera.Position.y >= blockedZoneMin.y && camera.Position.y <= blockedZoneMax.y &&
                camera.Position.z >= blockedZoneMin.z && camera.Position.z <= blockedZoneMax.z;
            if (blocked) camera.Position = prevPos;
        }

        camera.updateMatrix(cameraFov, cameraNearPlane, cameraFarPlane);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(cameraFov), (float)width / height, cameraNearPlane, cameraFarPlane);

        shaderProgram.Activate();
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "isDay"), isDay ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "diffuseIntensity"), diffuseIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "specularIntensity"), specularIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);

        model.Draw(shaderProgram, camera, modelTransform);

        if (sunPos.y > -200.0f && isDay) {
            sphereShader.Activate();
            sunTexture.Bind();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 1.0f);
            glm::mat4 sunModel = glm::translate(glm::mat4(1.0f), sunPos);
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(sunModel));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3f(glGetUniformLocation(sphereShader.ID, "color"), 1.0f, 0.85f, 0.45f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 1.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);

            glBindVertexArray(sunVAO);
            glDrawElements(GL_TRIANGLES, 48 * 48 * 6, GL_UNSIGNED_INT, 0);
            sunTexture.Unbind();
        }

        if (moonPos.y > -200.0f && !isDay) {
            sphereShader.Activate();
            moonTexture.Bind();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 1.0f);
            glm::mat4 moonModel =
                glm::translate(glm::mat4(1.0f), moonPos) *
                glm::inverse(glm::mat4(glm::mat3(view)));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(moonModel));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3f(glGetUniformLocation(sphereShader.ID, "color"), 0.75f, 0.80f, 0.95f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);

            glBindVertexArray(moonVAO);
            glDrawElements(GL_TRIANGLES, 36 * 36 * 6, GL_UNSIGNED_INT, 0);
            moonTexture.Unbind();
        }

        if (!useFastRenderMode)
            skybox.Draw(camera, cameraFov, cameraNearPlane, cameraFarPlane);

        DrawEnvironmentMenu(environmentMenu, environmentMode, overlayShader, overlayVAO, overlayVBO);

        if (showCoordinatesInWindowTitle && currentFrame - lastTitleUpdate >= 0.1f)
        {
            const glm::vec3 normPos = glm::clamp(camera.Position / targetSceneRadius, glm::vec3(-1.0f), glm::vec3(1.0f));

            const std::string title = BuildEnvironmentTitle(environmentMenu, environmentMode, normPos);
            glfwSetWindowTitle(window, title.c_str());
            lastTitleUpdate = currentFrame;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteBuffers(1, &sunEBO);
    glDeleteVertexArrays(1, &moonVAO);
    glDeleteBuffers(1, &moonVBO);
    glDeleteBuffers(1, &moonEBO);
    sunTexture.Delete();
    moonTexture.Delete();
    glDeleteVertexArrays(1, &overlayVAO);
    glDeleteBuffers(1, &overlayVBO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
