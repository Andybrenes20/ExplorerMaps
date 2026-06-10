#include <iostream>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include <cstdio>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- INCLUDES EXTERNOS ---
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "PhysicsWorld.h"
#include "Optimization.h"
#include "PlayerMovementAnimation.h"
#include "InteractionReticle.h"
#include "GameplayInput.h"
#include "EnvironmentSystem.h"
#include "EnvironmentAudio.h"
#include "WeatherOverlay.h"
#include "VehicleController.h"
#include "TrafficSystem.h"
#include "MainMenu.h"
#include "Localization.h"
#include "LoadingScreen.h"
#include "Shader.h"

// --- CONFIGURACIÓN GLOBAL ---
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const float PLAYER_HEIGHT = 1.75f;
const glm::vec3 PLAYER_SPAWN = glm::vec3(304.1f, 37.8f, 143.5f);
const float PLAYER_SPAWN_YAW = -90.0f;
const float PLAYER_SPAWN_PITCH = 0.0f;
const unsigned int SHADOW_MAP_SIZE = 1024;

// --- VARIABLES DE BARRA DE PROGRESO (HILOS ATÓMICOS COMBINADOS) ---
std::atomic<float> currentLoadingProgress{ 0.0f };
bool loadingStarted = false;

// --- VARIABLES GLOBALES DE AUDIO ---
ma_engine audioEngine;
ma_sound sfxPasos;
ma_sound sfxCorrer;
bool isMovingAudio = false;
bool isRunningAudio = false;
bool isFlying = false;
bool flyToggleWasPressed = false;
bool pauseToggleWasPressed = false;

// --- MÁQUINA DE ESTADOS DE JUEGO ---
enum GameState { STATE_MENU, STATE_LOADING, STATE_RUNNING, STATE_PAUSE, STATE_RETURNING_MENU };
GameState currentState = STATE_MENU;
float returnToMenuStartedAt = 0.0f;

// --- SISTEMA DE LUCES URBANAS ---
glm::vec3 farolesPos[4];

// --- VARIABLES DE CÁMARA (FPS) ---
glm::vec3 cameraPos = glm::vec3(0.0f, 10.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- VARIABLES INTERACCIÓN FICHAS PROCEDURALES ---
enum MonumentType { MONUMENT_NONE, MONUMENT_RELOJ, MONUMENT_VOLCAN };
bool showFicha = false;
bool e_pressed_last_frame = false;
MonumentType activeMonument = MONUMENT_NONE;

// --- DECLARACIÓN DE FUNCIONES ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void processInput(GLFWwindow* window, PhysicsWorld& physics, const GameplayGamepadInput& gamepad, bool vehicleDriving);
bool processFlightInput(GLFWwindow* window);
void UpdateWindowTitle(GLFWwindow* window, float currentFrame);
void UploadCityEnvironment(Shader& shader, const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame);
void DrawDynamicSky(Shader& shader, unsigned int skyboxVAO, const EnvironmentFrame& frame, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float currentFrame);
glm::mat4 RenderDirectionalShadow(Shader& shader, unsigned int framebuffer, const EnvironmentFrame& frame, const glm::vec3& cameraPosition, const glm::vec3& cameraForward, const PhysicsWorld& physics, const std::vector<Optimization::MeshBounds>& meshBounds, float sceneRadius);

// Controles geográficos de tu lógica sin texturas viejas
bool IsPlayerInBuildingZone(glm::vec3 playerPos);
bool IsPlayerInVolcanoZone(glm::vec3 playerPos);
void DrawProgrammedMonumentFicha(MonumentType type);

int main() {
    // 1. INICIALIZAR GLFW Y CREAR VENTANA
    EnvironmentAudio environmentAudio;
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Motor Grafico - ExplorerMaps", NULL, NULL);
    if (window == NULL) {
        std::cout << "Fallo al crear la ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Fallo al inicializar GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // 2. INICIALIZAR MOTOR DE AUDIO
    if (ma_engine_init(NULL, &audioEngine) != MA_SUCCESS) {
        std::cout << "Error fatal: No se pudo inicializar miniaudio." << std::endl;
        return -1;
    }
    if (!environmentAudio.Init(&audioEngine)) {
        std::cout << "Advertencia: No se pudo inicializar el audio de ambiente." << std::endl;
    }

    ma_sound_init_from_file(&audioEngine, "Sonidos/Caminar.mp3", 0, NULL, NULL, &sfxPasos);
    ma_sound_set_looping(&sfxPasos, MA_TRUE);
    ma_sound_set_volume(&sfxPasos, 0.8f);

    ma_sound_init_from_file(&audioEngine, "Sonidos/Correr.mp3", 0, NULL, NULL, &sfxCorrer);
    ma_sound_set_looping(&sfxCorrer, MA_TRUE);
    ma_sound_set_volume(&sfxCorrer, 0.9f);

    // 3. INICIALIZAR DEAR IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    MainMenu mainMenu;
    LoadingScreen loadingScreen;
    if (!mainMenu.Initialize()) {
        std::cout << "Advertencia: No se pudieron cargar los fondos del menu." << std::endl;
    }
    loadingScreen.Initialize();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Instanciar mundos físicos, Tráfico de compañeros y Shaders
    unsigned int skyboxVAO, skyboxVBO;

    PhysicsWorld physics;
    std::vector<Optimization::MeshBounds> cityMeshBounds;
    Optimization::SceneBounds citySceneBounds;
    PlayerMovementAnimation movementAnimation;
    VehicleController vehicle;
    TrafficSystem traffic;
    traffic.Initialize();
    if (!vehicle.InitAudio(&audioEngine)) {
        std::cout << "Advertencia: No se pudieron cargar todos los sonidos del vehiculo." << std::endl;
    }
    EnvironmentSystem environmentSystem;
    Shader cityShader("city.vert", "city.frag");
    Shader skyboxShader("skybox.vert", "skybox.frag");
    Shader shadowShader("shadow_depth.vert", "shadow_depth.frag");

    unsigned int shadowFramebuffer = 0;
    unsigned int shadowTexture = 0;
    glGenFramebuffers(1, &shadowFramebuffer);
    glGenTextures(1, &shadowTexture);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float shadowBorder[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, shadowBorder);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glm::mat4 shadowLightSpaceMatrix(1.0f);
    float nextShadowUpdate = 0.0f;

    // Configuración de optimización del Pull
    Optimization::RenderSettings cityRenderSettings;
    cityRenderSettings.maxRenderDistance = 100000.0f;
    cityRenderSettings.lodNearDistanceMultiplier = 0.08f;
    cityRenderSettings.lodMidDistanceMultiplier = 0.42f;
    cityRenderSettings.lodTinyMeshScreenRatio = 0.00055f;
    cityRenderSettings.lodSmallMeshScreenRatio = 0.00135f;
    cityRenderSettings.viewportCullPadding = 0.02f;

    static std::future<bool> loadFuture;
    bool worldReady = false;
    float loadingScreenStartedAt = 0.0f;
    float loadingReadyAt = -1.0f;
    loadFuture = std::async(std::launch::async, &PhysicsWorld::LoadCollisionData, &physics, "modelos/city.glb");
    loadingStarted = true;

    // CONFIGURACIÓN DE GEOMETRÍA DEL SKYBOX
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    const auto finalizeWorld = [&]() {
        if (worldReady || !loadFuture.valid() || loadFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }

        const bool loaded = loadFuture.get();
        if (!loaded || physics.cityCollider.vertices.empty()) {
            loadingStarted = false;
            return;
        }

        currentLoadingProgress = 0.84f;
        cityMeshBounds = Optimization::BuildMeshBounds(physics.visualMeshes);
        citySceneBounds = Optimization::BuildSceneBounds(cityMeshBounds);
        currentLoadingProgress = 0.88f;
        physics.UploadToGPU();
        currentLoadingProgress = 0.96f;
        if (vehicle.Load("modelos/nissan.glb")) {
            vehicle.UploadToGPU();
        }

        cameraPos = PLAYER_SPAWN;
        const glm::vec3 lampCenter(PLAYER_SPAWN.x, PLAYER_SPAWN.y + 2.0f, PLAYER_SPAWN.z);
        farolesPos[0] = lampCenter + glm::vec3(15.0f, 0.0f, 15.0f);
        farolesPos[1] = lampCenter + glm::vec3(-15.0f, 0.0f, -15.0f);
        farolesPos[2] = lampCenter + glm::vec3(15.0f, 0.0f, -15.0f);
        farolesPos[3] = lampCenter + glm::vec3(15.0f, 0.0f, 15.0f);
        yaw = PLAYER_SPAWN_YAW;
        pitch = PLAYER_SPAWN_PITCH;
        cameraFront = glm::normalize(glm::vec3(
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
        firstMouse = true;
        isFlying = false;
        worldReady = true;
        currentLoadingProgress = 1.0f;
        loadingReadyAt = static_cast<float>(glfwGetTime());
        };

    // 4. BUCLE PRINCIPAL DE RENDERIZADO
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::clamp(currentFrame - lastFrame, 0.0f, 0.05f);
        lastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        switch (currentState) {

        case STATE_MENU: {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            finalizeWorld();

            const MainMenuAction menuAction = mainMenu.Draw(currentFrame);
            if (menuAction == MainMenuAction::Start) {
                if (worldReady) {
                    currentLoadingProgress = 1.0f;
                }
                loadingScreenStartedAt = currentFrame;
                loadingReadyAt = worldReady ? currentFrame : -1.0f;
                currentState = STATE_LOADING;
            }
            else if (menuAction == MainMenuAction::Quit) {
                glfwSetWindowShouldClose(window, true);
            }
            break;
        }

        case STATE_LOADING: {
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            loadingScreen.Draw(currentLoadingProgress.load(), currentFrame);

            if (worldReady) {
                if (loadingReadyAt < 0.0f) {
                    loadingReadyAt = static_cast<float>(glfwGetTime());
                }
                const float entryReadyAt = loadingScreenStartedAt > loadingReadyAt ? loadingScreenStartedAt : loadingReadyAt;
                if (currentFrame - entryReadyAt >= 0.12f) {
                    cameraPos = PLAYER_SPAWN;
                    yaw = PLAYER_SPAWN_YAW;
                    pitch = PLAYER_SPAWN_PITCH;
                    cameraFront = glm::normalize(glm::vec3(
                        std::cos(glm::radians(yaw)),
                        0.0f,
                        std::sin(glm::radians(yaw))));
                    firstMouse = true;
                    isFlying = false;
                    environmentAudio.StartAmbient();
                    vehicle.SetAudioPaused(false);
                    currentState = STATE_RUNNING;
                }
                break;
            }

            if (!loadingStarted) {
                loadFuture = std::async(std::launch::async, &PhysicsWorld::LoadCollisionData, &physics, "modelos/city.glb");
                loadingStarted = true;
            }

            finalizeWorld();
            break;
        }

        case STATE_RUNNING: {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            const GameplayGamepadInput gamepad = GameplayInput::ReadGamepad();

            const EnvironmentFrame environmentFrame = environmentSystem.Update(window, gamepad, deltaTime, currentFrame);
            environmentAudio.Update(environmentFrame, environmentSystem);
            traffic.Update(deltaTime, physics, cameraPos, cameraFront);

            if (!environmentSystem.IsMenuOpen()) {
                if (!vehicle.IsDriving()) {
                    GameplayInput::ApplyGamepadCamera(gamepad, deltaTime, yaw, pitch, cameraFront);
                }
                vehicle.Update(window, gamepad, deltaTime, physics, cameraPos, cameraPos, cameraFront, yaw, pitch);
                processInput(window, physics, gamepad, vehicle.IsDriving());
            }

            movementAnimation.Update(window, environmentSystem.IsMenuOpen() ? GameplayGamepadInput{} : gamepad, deltaTime, cameraFront, cameraUp, !isFlying && !vehicle.IsDriving());

            // Gravedad del mundo unificado
            if (!isFlying && !vehicle.IsDriving()) {
                float distanceToGround = 0.0f;
                glm::vec3 rayOrigin = cameraPos;
                rayOrigin.y += 1.0f;
                glm::vec3 rayDir = glm::vec3(0.0f, -1.0f, 0.0f);

                if (physics.Raycast(rayOrigin, rayDir, distanceToGround)) {
                    float floorY = rayOrigin.y - distanceToGround;
                    float currentFeetY = cameraPos.y - PLAYER_HEIGHT;
                    float stepHeight = floorY - currentFeetY;
                    if (stepHeight > 0.6f || stepHeight < -0.5f) {
                        cameraPos.y -= 9.81f * deltaTime;
                    }
                    else {
                        cameraPos.y = floorY + PLAYER_HEIGHT;
                    }
                }
                else {
                    cameraPos.y -= 9.81f * deltaTime;
                }
            }

            glClearColor(environmentFrame.clearColor.x, environmentFrame.clearColor.y, environmentFrame.clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 3000.0f);
            const glm::vec3 animatedCameraPos = cameraPos + movementAnimation.GetCameraOffset();
            glm::mat4 view = glm::lookAt(animatedCameraPos, animatedCameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);

            if (Localization::shadowsEnabled && currentFrame >= nextShadowUpdate && environmentFrame.sunHeight > -0.05f) {
                shadowLightSpaceMatrix = RenderDirectionalShadow(
                    shadowShader, shadowFramebuffer, environmentFrame, animatedCameraPos, cameraFront,
                    physics, cityMeshBounds, citySceneBounds.radius);
                nextShadowUpdate = currentFrame + (1.0f / 6.0f);
            }

            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setMat4("lightSpaceMatrix", shadowLightSpaceMatrix);
            UploadCityEnvironment(cityShader, environmentFrame, animatedCameraPos, currentFrame);
            vehicle.UploadHeadlights(cityShader, environmentFrame.nightFactor, environmentFrame.rainIntensity);
            cityShader.setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowTexture);

            const Optimization::FrameCulling cityCulling = Optimization::BuildFrameCulling(
                projection, view, animatedCameraPos, cameraFront, citySceneBounds.radius);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling, cityRenderSettings);
            vehicle.Draw(cityShader);
            traffic.Draw(cityShader, vehicle, animatedCameraPos);

            DrawDynamicSky(skyboxShader, skyboxVAO, environmentFrame, view, projection, animatedCameraPos, currentFrame);

            // Retícula e Interfaz de Clima
            InteractionReticle::DrawCenterDot(
                static_cast<float>(SCR_WIDTH),
                static_cast<float>(SCR_HEIGHT),
                gamepad.interactDown || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
            WeatherOverlay::Draw(environmentFrame, currentFrame, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));

            // --- COMBINACIÓN: LOGICA DE FICHAS PROCEDURALES ESTILIZADAS ---
            bool inBuilding = IsPlayerInBuildingZone(cameraPos);
            bool inVolcano = IsPlayerInVolcanoZone(cameraPos);

            if (inBuilding || inVolcano) {
                activeMonument = inBuilding ? MONUMENT_RELOJ : MONUMENT_VOLCAN;

                if (!showFicha) {
                    // Mejora estética del prompt: pill oscuro con indicador [E] estilizado
                    ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH / 2.0f, SCR_HEIGHT * 0.78f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::Begin("InteractPrompt", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);


                    ImVec2 padding = ImVec2(22.0f, 10.0f);
                    const char* labelA = "SELECCIONE ";
                    const char* labelB = " PARA INSPECCIONAR MONUMENTO";
                    ImVec2 aSize = ImGui::CalcTextSize(labelA);
                    ImVec2 bSize = ImGui::CalcTextSize(labelB);
                    ImVec2 keySize = ImGui::CalcTextSize("E");
                    float totalW = aSize.x + keySize.x + 24.0f + bSize.x + padding.x * 2; // 24 for key padding
                    float totalH = keySize.y + padding.y * 2;

                    // Force the window to occupy the needed area so GetWindowPos/size are meaningful
                    ImGui::Dummy(ImVec2(totalW, totalH));

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 winPos = ImGui::GetWindowPos();
                    ImVec2 pMin = ImVec2(winPos.x, winPos.y);
                    ImVec2 pMax = ImVec2(winPos.x + totalW, winPos.y + totalH);

                    // Center the pill horizontally around the window area
                    // (window was already positioned by SetNextWindowPos)
                    // Background pill
                    drawList->AddRectFilled(pMin, pMax, IM_COL32(14, 18, 28, 220), 10.0f);
                    drawList->AddRect(pMin, pMax, IM_COL32(60, 160, 255, 120), 10.0f, 0, 1.25f);

                    // Text positions
                    ImVec2 textPos = ImVec2(pMin.x + padding.x, pMin.y + padding.y);
                    drawList->AddText(textPos, IM_COL32(230, 230, 240, 255), labelA);

                    // Key pill
                    ImVec2 keyP0 = ImVec2(textPos.x + aSize.x + 6.0f, pMin.y + padding.y - 2.0f);
                    ImVec2 keyP1 = ImVec2(keyP0.x + keySize.x + 14.0f, keyP0.y + keySize.y + 6.0f);
                    drawList->AddRectFilled(keyP0, keyP1, IM_COL32(10, 120, 200, 255), 6.0f);
                    drawList->AddRect(keyP0, keyP1, IM_COL32(255, 255, 255, 40), 6.0f, 0, 1.0f);
                    ImVec2 keyTextPos = ImVec2(keyP0.x + 7.0f, keyP0.y + 4.0f);
                    drawList->AddText(keyTextPos, IM_COL32(245, 245, 245, 255), "E");

                    ImVec2 afterKeyPos = ImVec2(keyP1.x + 8.0f, textPos.y);
                    drawList->AddText(afterKeyPos, IM_COL32(230, 230, 240, 255), labelB);

                    ImGui::End();
                }

                bool e_pressed_now = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) || gamepad.interactDown;
                if (e_pressed_now && !e_pressed_last_frame) {
                    showFicha = !showFicha;
                }
                e_pressed_last_frame = e_pressed_now;
            }
            else {
                showFicha = false;
                e_pressed_last_frame = false;
                activeMonument = MONUMENT_NONE;
            }

            if (showFicha && activeMonument != MONUMENT_NONE) {
                DrawProgrammedMonumentFicha(activeMonument);
            }
            // --- FIN LOGICA DE FICHAS PROCEDURALES ---

            mainMenu.DrawEnvironmentMenu(environmentSystem);
            break;
        }

        case STATE_PAUSE: {
            vehicle.SetAudioPaused(true);
            if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
            if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }

            const EnvironmentFrame environmentFrame = environmentSystem.BuildFrame(currentFrame);

            glClearColor(environmentFrame.clearColor.x, environmentFrame.clearColor.y, environmentFrame.clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 3000.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);
            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setMat4("lightSpaceMatrix", shadowLightSpaceMatrix);
            UploadCityEnvironment(cityShader, environmentFrame, cameraPos, currentFrame);
            vehicle.UploadHeadlights(cityShader, environmentFrame.nightFactor, environmentFrame.rainIntensity);
            cityShader.setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowTexture);

            const Optimization::FrameCulling cityCulling = Optimization::BuildFrameCulling(
                projection, view, cameraPos, cameraFront, citySceneBounds.radius);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling, cityRenderSettings);
            vehicle.Draw(cityShader);
            traffic.Draw(cityShader, vehicle, cameraPos);

            DrawDynamicSky(skyboxShader, skyboxVAO, environmentFrame, view, projection, cameraPos, currentFrame);

            const MainMenuAction pauseAction = mainMenu.DrawPause(environmentSystem);
            if (pauseAction == MainMenuAction::Resume) {
                vehicle.SetAudioPaused(false);
                currentState = STATE_RUNNING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            else if (pauseAction == MainMenuAction::ReturnToMainMenu) {
                vehicle.ResetForMainMenu();
                environmentAudio.StopAmbient();
                currentLoadingProgress = 0.0f;
                returnToMenuStartedAt = currentFrame;
                currentState = STATE_RETURNING_MENU;
            }
            break;
        }

        case STATE_RETURNING_MENU: {
            const float returnProgress = glm::clamp((currentFrame - returnToMenuStartedAt) / 1.25f, 0.0f, 1.0f);
            loadingScreen.DrawReturnToMenu(returnProgress);
            if (returnProgress >= 1.0f) {
                currentState = STATE_MENU;
            }
            break;
        }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        UpdateWindowTitle(window, currentFrame);
        glfwSwapBuffers(window);
    }

    // 5. LIMPIEZA
    environmentAudio.Shutdown();
    vehicle.Shutdown();
    mainMenu.Shutdown();
    ma_sound_uninit(&sfxPasos);
    ma_sound_uninit(&sfxCorrer);
    ma_engine_uninit(&audioEngine);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteFramebuffers(1, &shadowFramebuffer);
    glDeleteTextures(1, &shadowTexture);

    for (const auto& mesh : physics.visualMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO_Pos);
        glDeleteBuffers(1, &mesh.VBO_UV);
        glDeleteBuffers(1, &mesh.VBO_Normal);
        glDeleteBuffers(1, &mesh.EBO);
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window, PhysicsWorld& physics, const GameplayGamepadInput& gamepad, bool vehicleDriving) {
    const bool pausePressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || gamepad.pauseDown;
    if (pausePressed && !pauseToggleWasPressed) {
        if (currentState == STATE_RUNNING) {
            currentState = STATE_PAUSE;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else if (currentState == STATE_PAUSE) {
            currentState = STATE_RUNNING;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    pauseToggleWasPressed = pausePressed;

    if (currentState != STATE_RUNNING) return;
    if (vehicleDriving) return;

    if (processFlightInput(window)) {
        return;
    }

    float baseSpeed = 6.0f;
    bool isSprinting = false;

    if (GameplayInput::IsRunning(window, gamepad)) {
        baseSpeed = 12.0f;
        isSprinting = true;
    }

    float cameraSpeed = baseSpeed * deltaTime;
    glm::vec3 moveDir = GameplayInput::BuildMoveDirection(window, gamepad, cameraFront, cameraUp);

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
        float playerRadius = 0.5f;
        glm::vec3 proposedPos = cameraPos;
        float hitDistance = 0.0f;

        proposedPos.x += moveDir.x * cameraSpeed;
        glm::vec3 chestPosX = proposedPos; chestPosX.y -= 0.5f;
        glm::vec3 dirX = glm::vec3(moveDir.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        if (moveDir.x != 0.0f && physics.Raycast(chestPosX, dirX, hitDistance) && hitDistance < playerRadius) {
            proposedPos.x = cameraPos.x;
        }

        proposedPos.z += moveDir.z * cameraSpeed;
        glm::vec3 chestPosZ = proposedPos; chestPosZ.y -= 0.5f;
        glm::vec3 dirZ = glm::vec3(0.0f, 0.0f, moveDir.z > 0.0f ? 1.0f : -1.0f);
        if (moveDir.z != 0.0f && physics.Raycast(chestPosZ, dirZ, hitDistance) && hitDistance < playerRadius) {
            proposedPos.z = cameraPos.z;
        }

        glm::vec3 chestPosFinal = proposedPos; chestPosFinal.y -= 0.5f;
        if (physics.Raycast(chestPosFinal, moveDir, hitDistance) && hitDistance < (playerRadius * 0.8f)) {
            proposedPos = cameraPos;
        }

        cameraPos = proposedPos;

        if (isSprinting) {
            if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
            if (!isRunningAudio) { ma_sound_start(&sfxCorrer); isRunningAudio = true; }
        }
        else {
            if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }
            if (!isMovingAudio) { ma_sound_start(&sfxPasos); isMovingAudio = true; }
        }
    }
    else {
        if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
        if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }
    }
}

bool processFlightInput(GLFWwindow* window) {
    const bool togglePressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (togglePressed && !flyToggleWasPressed) {
        isFlying = !isFlying;
    }
    flyToggleWasPressed = togglePressed;

    if (!isFlying) {
        return false;
    }

    if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
    if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }

    glm::vec3 right = glm::cross(cameraFront, cameraUp);
    if (glm::length(right) > 0.0001f) {
        right = glm::normalize(right);
    }

    glm::vec3 flyDirection(0.0f);
    if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)) flyDirection += cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)) flyDirection -= cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)) flyDirection -= right;
    if ((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)) flyDirection += right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) flyDirection += cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) flyDirection -= cameraUp;

    if (glm::length(flyDirection) > 0.0001f) {
        const float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 60.0f : 30.0f;
        cameraPos += glm::normalize(flyDirection) * speed * deltaTime;
    }
    return true;
}

void UpdateWindowTitle(GLFWwindow* window, float currentFrame) {
    static float nextUpdateTime = 0.0f;
    if (currentFrame < nextUpdateTime) return;
    nextUpdateTime = currentFrame + 0.1f;

    char title[160];
    std::snprintf(title, sizeof(title), "Motor Grafico - ExplorerMaps | X: %.1f  Y: %.1f  Z: %.1f%s", cameraPos.x, cameraPos.y, cameraPos.z, isFlying ? " | VOLANDO" : "");
    glfwSetWindowTitle(window, title);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (currentState != STATE_RUNNING) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX; float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    float sensitivity = 0.1f; xoffset *= sensitivity; yoffset *= sensitivity;
    yaw += xoffset; pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void UploadCityEnvironment(Shader& shader, const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame) {
    shader.setVec3("objectTint", glm::vec3(1.0f));
    shader.setVec3("viewPos", viewPosition);
    shader.setVec3("light.direction", frame.activeLightDir);
    shader.setVec3("celestialLightPosition", frame.mainLightPosition);
    shader.setVec3("light.ambient", frame.ambient);
    shader.setVec3("light.diffuse", frame.diffuse);
    shader.setVec3("light.specular", frame.specular);
    shader.setFloat("time", currentFrame);
    shader.setFloat("dayFactor", frame.dayFactor);
    shader.setFloat("nightFactor", frame.nightFactor);
    shader.setFloat("rainIntensity", frame.rainIntensity);
    shader.setFloat("fogIntensity", frame.fogIntensity);
    shader.setFloat("sunHeight", frame.sunHeight);
    shader.setFloat("windowLightIntensity", frame.windowLightIntensity);
    shader.setFloat("cloudCoverage", frame.cloudCoverage);
    shader.setFloat("cloudSpeed", frame.cloudSpeed);
    shader.setFloat("cloudDensity", frame.cloudDensity);

    const float shadowStrength = Localization::shadowsEnabled
        ? glm::smoothstep(-0.04f, 0.24f, frame.sunHeight) * (1.0f - frame.rainIntensity * 0.48f) * 0.98f
        : 0.0f;
    shader.setFloat("shadowStrength", shadowStrength);
    shader.setFloat("pointLightIntensity", frame.streetlightIntensity);

    const glm::vec3 lampColor(1.0f, 0.60f, 0.20f);
    for (int i = 0; i < 4; ++i) {
        const std::string name = "pointLights[" + std::to_string(i) + "]";
        shader.setVec3(name + ".position", farolesPos[i]);
        shader.setVec3(name + ".ambient", lampColor * 0.1f);
        shader.setVec3(name + ".diffuse", lampColor);
        shader.setVec3(name + ".specular", lampColor);
        shader.setFloat(name + ".constant", 1.0f);
        shader.setFloat(name + ".linear", 0.09f);
        shader.setFloat(name + ".quadratic", 0.032f);
    }
}

glm::mat4 RenderDirectionalShadow(Shader& shader, unsigned int framebuffer, const EnvironmentFrame& frame, const glm::vec3& cameraPosition, const glm::vec3& cameraForward, const PhysicsWorld& physics, const std::vector<Optimization::MeshBounds>& meshBounds, float sceneRadius) {
    glm::vec3 shadowCenter = cameraPosition + cameraForward * 48.0f;
    constexpr float shadowHalfWidth = 112.0f;
    constexpr float shadowHalfHeight = 94.0f;
    const float shadowWorldTexel = (shadowHalfWidth * 2.0f) / static_cast<float>(SHADOW_MAP_SIZE);
    shadowCenter.x = std::floor(shadowCenter.x / shadowWorldTexel) * shadowWorldTexel;
    shadowCenter.z = std::floor(shadowCenter.z / shadowWorldTexel) * shadowWorldTexel;
    glm::vec3 directionToLight = frame.mainLightPosition - shadowCenter;
    directionToLight = glm::length(directionToLight) > 0.001f ? glm::normalize(directionToLight) : glm::vec3(0.35f, 0.75f, -0.45f);

    const glm::vec3 lightPosition = shadowCenter + directionToLight * 300.0f;
    const glm::vec3 lightUp = std::abs(glm::dot(directionToLight, cameraUp)) > 0.96f ? glm::vec3(0.0f, 0.0f, 1.0f) : cameraUp;
    const glm::mat4 lightProjection = glm::ortho(-shadowHalfWidth, shadowHalfWidth, -shadowHalfHeight, shadowHalfHeight, 1.0f, 600.0f);
    const glm::mat4 lightView = glm::lookAt(lightPosition, shadowCenter, lightUp);
    const glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.25f, 2.4f);

    shader.use();
    shader.setMat4("model", glm::mat4(1.0f));
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    const Optimization::FrameCulling shadowCulling = Optimization::BuildFrameCulling(lightProjection, lightView, lightPosition, glm::normalize(shadowCenter - lightPosition), sceneRadius);

    Optimization::RenderSettings shadowSettings;
    shadowSettings.maxRenderDistance = 520.0f;
    shadowSettings.lodNearDistanceMultiplier = 0.10f;
    shadowSettings.lodMidDistanceMultiplier = 0.30f;
    shadowSettings.lodTinyMeshScreenRatio = 0.0012f;
    shadowSettings.lodSmallMeshScreenRatio = 0.0030f;
    shadowSettings.viewportCullPadding = 0.07f;
    Optimization::DrawCityMeshes(physics.visualMeshes, meshBounds, shadowCulling, shadowSettings);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    return lightSpaceMatrix;
}

void DrawDynamicSky(Shader& shader, unsigned int skyboxVAO, const EnvironmentFrame& frame, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float currentFrame) {
    glm::vec3 moonDirection = frame.moonPosition - cameraPosition;
    moonDirection = glm::length(moonDirection) > 0.001f ? glm::normalize(moonDirection) : -frame.sunDirection;

    glDepthFunc(GL_LEQUAL);
    shader.use();
    shader.setMat4("view", glm::mat4(glm::mat3(view)));
    shader.setMat4("projection", projection);
    shader.setFloat("blendFactor", frame.blendFactor);
    shader.setFloat("time", currentFrame);
    shader.setFloat("sunHeight", frame.sunHeight);
    shader.setVec3("sunDir", frame.sunDirection);
    shader.setVec3("moonDir", moonDirection);
    shader.setFloat("rainIntensity", frame.rainIntensity);
    shader.setFloat("fogIntensity", frame.fogIntensity);
    shader.setFloat("lightningAmount", frame.lightningAmount);
    shader.setFloat("lightningSeed", frame.lightningSeed);
    shader.setVec3("cameraPosition", cameraPosition);
    shader.setFloat("cloudCoverage", frame.cloudCoverage);
    shader.setFloat("cloudSpeed", frame.cloudSpeed);
    shader.setFloat("cloudDensity", frame.cloudDensity);
    shader.setFloat("cloudCrispiness", frame.cloudCrispiness);
    shader.setVec3("cloudColor", frame.cloudColor);
    glUniform2f(glGetUniformLocation(shader.ID, "resolution"), static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));

    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

// --- LOGICA DE DETECCION DE ZONAS GEOGRAFICAS ---
bool IsPlayerInBuildingZone(glm::vec3 playerPos) {
    bool enAltura = playerPos.y >= 51.0f;
    if (!enAltura) return false;

    float centroX = 179.65f;
    float centroZ = 223.75f;
    float radioCilindro = 32.0f;

    float distanciaX = playerPos.x - centroX;
    float distanciaZ = playerPos.z - centroZ;
    float distanciaCuadrada = (distanciaX * distanciaX) + (distanciaZ * distanciaZ);

    return distanciaCuadrada <= (radioCilindro * radioCilindro);
}

bool IsPlayerInVolcanoZone(glm::vec3 playerPos) {
    bool enAltura = playerPos.y >= 37.0f;
    if (!enAltura) return false;

    float minX = -980.0f, maxX = -450.0f;
    float minZ = -200.0f, maxZ = -125.0f;

    return (playerPos.x >= minX && playerPos.x <= maxX) && (playerPos.z >= minZ && playerPos.z <= maxZ);
}

// --- RENDERS PROCEDURALES LIMPIOS CON EL ESTILO DEL JUEGO ---
// --- RENDERS PROCEDURALES LIMPIOS CON EL ESTILO DEL JUEGO ---
// --- RENDERS PROCEDURALES LIMPIOS CON EL ESTILO DEL JUEGO ---
void DrawProgrammedMonumentFicha(MonumentType type) {
    const float panelWidth = 920.0f;
    float panelHeight = 400.0f; // Alto base aumentado para fuente más grande

    const char* title = "";
    const char* desc = "";
    ImVec4 headerColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    if (type == MONUMENT_RELOJ) {
        title = "RELOJ DE DIRIAMBA";
        desc = "Construido en 1935, esta torre de 15.5 metros es el segundo monumento mas importante de la ciudad. Su estructura fue declarada Patrimonio Cultural en 2002.";
        headerColor = ImVec4(0.98f, 0.73f, 0.05f, 1.0f);
        panelHeight = 380.0f;
    }
    else if (type == MONUMENT_VOLCAN) {
        title = "VOLCAN MASAYA";
        desc = "Este volcan activo alberga el crater Santiago, uno de los pocos lugares del mundo con un lago de lava persistente. Como dato historico, el fraile espanol Francisco de Bobadilla coloco una gran cruz en su cumbre en 1529 para exorcizar al volcan, al que los conquistadores creian una entrada directa al infierno.";
        headerColor = ImVec4(1.00f, 0.25f, 0.05f, 1.0f);
        panelHeight = 560.0f; // Alto expandido para que quepa TODA la info grande y legible
    }

    // Centrado simétrico en pantalla
    ImGui::SetNextWindowPos(ImVec2((SCR_WIDTH - panelWidth) * 0.5f, (SCR_HEIGHT - panelHeight) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    // Margen interno entre elementos: ¡Crucial para que no se encime el texto grande!
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 14.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.04f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.50f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("FichaInformativaProcedural", nullptr, flags)) {
        ImDrawList* wndDraw = ImGui::GetWindowDrawList();
        ImVec2 wndPos = ImGui::GetWindowPos();

        // 1. ENCABEZADO PREMIUM
        float headerHeight = 90.0f;
        ImVec2 headerP0 = ImVec2(wndPos.x + 12.0f, wndPos.y + 12.0f);
        ImVec2 headerP1 = ImVec2(headerP0.x + panelWidth - 24.0f, headerP0.y + headerHeight);

        wndDraw->AddRectFilled(headerP0, headerP1, IM_COL32(15, 18, 26, 255), 12.0f);
        wndDraw->AddRectFilled(headerP0, ImVec2(headerP1.x, headerP0.y + 6.0f), ImGui::ColorConvertFloat4ToU32(headerColor), 12.0f);

        // 2. TÍTULO GRANDE (USANDO PUSH/POP FONT SI TIENES UNA FUENTE CARGADA)
        const float titleFontSize = 36.0f;
        ImVec2 titleSize = ImGui::GetFont()->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, title);

        ImVec2 titlePos = ImVec2(
            headerP0.x + ((panelWidth - 24.0f) - titleSize.x) * 0.5f,
            headerP0.y + (headerHeight - titleSize.y) * 0.5f + 2.0f
        );

        wndDraw->AddText(ImGui::GetFont(), titleFontSize, ImVec2(titlePos.x + 2.0f, titlePos.y + 2.0f), IM_COL32(0, 0, 0, 200), title);
        wndDraw->AddText(ImGui::GetFont(), titleFontSize, titlePos, ImGui::ColorConvertFloat4ToU32(headerColor), title);

        // Espaciado corregido tras el encabezado
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + headerHeight + 10.0f);
        ImGui::Separator();

        // 3. CUERPO DEL TEXTO (DESCRIPCIÓN GRANDE Y LEGIBLE)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.94f, 0.96f, 1.0f));

        // Ajustamos la fuente interna del cuerpo: ¡AUMENTADA!
        const float bodyFontSize = 24.0f; // Mucho más grande, legible y premium

        // IMPORTANTE: Asegurar interlineado correcto al cambiar tamaño
        ImGui::SetCursorPosX(36.0f); // Margen izquierdo
        ImGui::PushTextWrapPos(panelWidth - 36.0f); // Margen derecho

        // --- LA CORRECCIÓN CLAVE ---
        // Si no tienes una fuente específica cargada para cuerpo, usa GetFont() 
        // pero asegúrate de ajustar el interlineado usando CalcTextSize para el salto de línea.
        // ImGui se encarga del interlineado basado en el tamaño de fuente si usas PushFont.
        // Si usas AddText manual, debes manejar el salto tú. Aquí usamos TextWrapped de ImGui 
        // pero ajustando el WindowFontScale **SOLO** si tienes una fuente cargada.

        // Si tienes una fuente específica cargada en io.Fonts para cuerpo, úsala:
        // ImGui::PushFont(bodyFont);

        // Si estás usando la fuente por defecto escalada (causa problemas si no se maneja bien):
        // En lugar de escalar la ventana, usamos AddText en el drawlist para el cuerpo 
        // **RESPECTANDO** el TextWrap, o usamos la fuente grande cargada.

        // Asumiendo que ImGui ya tiene una fuente cargada para el título, úsala aquí:
        wndDraw->AddText(ImGui::GetFont(), bodyFontSize, ImGui::GetCursorScreenPos(), ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.94f, 0.96f, 1.0f)), desc, desc + strlen(desc), panelWidth - 72.0f);

        // Empujamos el cursor hacia abajo manualmente para que ImGui sepa dónde terminó el texto AddText
        ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(bodyFontSize, panelWidth - 72.0f, panelWidth - 72.0f, desc);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textSize.y);

        // Si usabas PushFont, descomenta esto:
        // ImGui::PopFont();

        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3); // PopWindowRounding, PopWindowBorderSize, PopItemSpacing
}