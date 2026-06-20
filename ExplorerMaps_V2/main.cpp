#include <iostream>
#include <algorithm>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include <cstdio>
#include <cmath>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// --- INCLUDES EXTERNOS ---
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
#include "CityBackdrop.h"
#include "MountainElevator.h"
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
const float WORLD_MIN_X = -4700.0f;
const float WORLD_MAX_X = 4550.0f;
const float WORLD_MIN_Z = -3350.0f;
const float WORLD_MAX_Z = 4950.0f;

struct TravelDestination {
    glm::vec3 position;
    glm::vec3 lookAt;
};

const TravelDestination TRAVEL_CITY = { PLAYER_SPAWN, PLAYER_SPAWN + glm::vec3(0.0f, 0.0f, -20.0f) };
const TravelDestination TRAVEL_CLOCK = { glm::vec3(180.3f, 52.8f, 193.2f), glm::vec3(179.65f, 72.0f, 223.75f) };
const TravelDestination TRAVEL_STATUE = { glm::vec3(-430.7f, 391.9f, 885.4f), glm::vec3(-430.7f, 393.0f, 915.0f) };
const TravelDestination TRAVEL_VOLCANO = { glm::vec3(-630.5f, 37.8f, -145.6f), glm::vec3(-790.0f, 78.0f, -160.0f) };

// --- VARIABLES DE BARRA DE PROGRESO ---
std::atomic<float> currentLoadingProgress{ 0.0f };
bool loadingStarted = false;

// --- VARIABLES GLOBALES DE AUDIO ---
ma_engine audioEngine;
ma_sound sfxPasos;
ma_sound sfxCorrer;
ma_sound sfxMenuCalm;
ma_sound sfxMenuButtons;
ma_sound sfxCristoPasos;
ma_sound sfxCristoCorrer;
ma_sound sfxCristoNaturaleza;
bool isMovingAudio = false;
bool isRunningAudio = false;
bool isMenuCalmAudio = false;
bool menuCalmAudioReady = false;
bool menuButtonAudioReady = false;
bool isCristoMovingAudio = false;
bool isCristoRunningAudio = false;
bool isCristoNatureAudio = false;
bool cristoAudioReady = false;
bool cristoWalkAudioReady = false;
bool cristoRunAudioReady = false;
bool cristoNatureAudioReady = false;
bool isFlying = false;
bool flyToggleWasPressed = false;
bool pauseToggleWasPressed = false;

// --- MÁQUINA DE ESTADOS DE JUEGO ---
enum GameState { STATE_MENU, STATE_LOADING, STATE_RUNNING, STATE_PAUSE, STATE_RETURNING_MENU };
GameState currentState = STATE_MENU;
float returnToMenuStartedAt = 0.0f;

// --- SISTEMA DE LUCES URBANAS ---
glm::vec3 farolesPos[4]; // Posiciones de los faroles

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

enum MonumentType { MONUMENT_NONE, MONUMENT_RELOJ, MONUMENT_VOLCAN, MONUMENT_CRISTO };
bool showMonumentFicha = false;
bool monumentInteractWasDown = false;
MonumentType activeMonument = MONUMENT_NONE;

// --- DECLARACIÓN DE FUNCIONES ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void processInput(GLFWwindow* window, PhysicsWorld& physics, const GameplayGamepadInput& gamepad, bool vehicleDriving);
bool processFlightInput(GLFWwindow* window);
void UpdateWindowTitle(GLFWwindow* window, float currentFrame);
float ComputeLandmarkFogIntensity(const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame);
float ComputeCristoCityFogIntensity(const glm::vec3& viewPosition, float currentFrame);
EnvironmentFrame BuildLocalizedFogFrame(const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame);
bool ShouldHideCristoArea(const glm::vec3& viewPosition);
bool ShouldHideCristoCutoffArea(const glm::vec3& viewPosition);
bool ShouldHideVolcanoForCristoView(const glm::vec3& viewPosition, const glm::vec3& viewDirection);
glm::vec3 ClampToWorldBounds(const glm::vec3& position);
void AddWorldBoundaryCollision(PhysicsWorld& physics);
bool IsInCristoAudioZone(const glm::vec3& position);
bool IsInLowerCristoForestZone(const glm::vec3& position);
void UpdateCristoNatureAudio(bool insideCristoZone);
void StopFootstepAudio();
void StartWalkingAudio(bool cristoZone);
void StartRunningAudio(bool cristoZone);
void UploadCityEnvironment(Shader& shader, const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame);
void DrawDynamicSky(Shader& shader, unsigned int skyboxVAO, const EnvironmentFrame& frame, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float currentFrame);
glm::mat4 RenderDirectionalShadow(Shader& shader, unsigned int framebuffer, const EnvironmentFrame& frame, const glm::vec3& cameraPosition, const glm::vec3& cameraForward, const PhysicsWorld& physics, const std::vector<Optimization::MeshBounds>& meshBounds, float sceneRadius);
void TravelTo(const TravelDestination& destination);
bool IsPlayerInBuildingZone(const glm::vec3& playerPosition);
bool IsPlayerInVolcanoZone(const glm::vec3& playerPosition);
bool IsPlayerInCristoZone(const glm::vec3& playerPosition);
MonumentType DetectNearbyMonument(const glm::vec3& playerPosition);
void DrawMonumentInteractPrompt();
void DrawProgrammedMonumentFicha(MonumentType type);
void UpdateMonumentInteraction(GLFWwindow* window, const GameplayGamepadInput& gamepad, const glm::vec3& playerPosition);

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

    menuCalmAudioReady = ma_sound_init_from_file(&audioEngine, "Sonidos/calm.mp3", 0, NULL, NULL, &sfxMenuCalm) == MA_SUCCESS;
    if (menuCalmAudioReady) {
        ma_sound_set_looping(&sfxMenuCalm, MA_TRUE);
        ma_sound_set_volume(&sfxMenuCalm, 0.55f);
    }
    menuButtonAudioReady = ma_sound_init_from_file(&audioEngine, "Sonidos/Buttons.mp3", 0, NULL, NULL, &sfxMenuButtons) == MA_SUCCESS;
    if (menuButtonAudioReady) {
        ma_sound_set_volume(&sfxMenuButtons, 0.85f);
    }

    cristoWalkAudioReady = ma_sound_init_from_file(&audioEngine, "Sonidos/forest_steps.mp3", 0, NULL, NULL, &sfxCristoPasos) == MA_SUCCESS;
    cristoRunAudioReady = ma_sound_init_from_file(&audioEngine, "Sonidos/Run_steps.mp3", 0, NULL, NULL, &sfxCristoCorrer) == MA_SUCCESS;
    cristoNatureAudioReady = ma_sound_init_from_file(&audioEngine, "Sonidos/Naturaleza.mp3", 0, NULL, NULL, &sfxCristoNaturaleza) == MA_SUCCESS;
    cristoAudioReady = cristoWalkAudioReady && cristoRunAudioReady;
    if (cristoWalkAudioReady) {
        ma_sound_set_looping(&sfxCristoPasos, MA_TRUE);
        ma_sound_set_volume(&sfxCristoPasos, 0.78f);
    }
    if (cristoRunAudioReady) {
        ma_sound_set_looping(&sfxCristoCorrer, MA_TRUE);
        ma_sound_set_volume(&sfxCristoCorrer, 0.86f);
    }
    if (cristoNatureAudioReady) {
        ma_sound_set_looping(&sfxCristoNaturaleza, MA_TRUE);
        ma_sound_set_volume(&sfxCristoNaturaleza, 0.0f);
    }

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
    mainMenu.SetButtonSoundCallback([]() {
        if (!menuButtonAudioReady) {
            return;
        }
        ma_sound_stop(&sfxMenuButtons);
        ma_sound_seek_to_pcm_frame(&sfxMenuButtons, 0);
        ma_sound_start(&sfxMenuButtons);
    });
    loadingScreen.Initialize();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Instanciar mundos físicos y Shaders
    unsigned int skyboxVAO, skyboxVBO;

    PhysicsWorld physics;
    std::vector<Optimization::MeshBounds> cityMeshBounds;
    Optimization::SceneBounds citySceneBounds;
    PlayerMovementAnimation movementAnimation;
    VehicleController vehicle;
    TrafficSystem traffic;
    CityBackdrop cityBackdrop;
    MountainElevator mountainElevator;
    traffic.Initialize();
    if (!cityBackdrop.Initialize()) {
        std::cout << "Advertencia: No se pudo inicializar la base visual de la ciudad." << std::endl;
    }
    if (!mountainElevator.Initialize()) {
        std::cout << "Advertencia: No se pudo inicializar el ascensor del Cristo." << std::endl;
    }
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

        mountainElevator.AddCollisionTo(physics);
        cityBackdrop.AddCollisionTo(physics);
        AddWorldBoundaryCollision(physics);
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
        farolesPos[3] = lampCenter + glm::vec3(-15.0f, 0.0f, 15.0f);
        yaw = PLAYER_SPAWN_YAW;
        pitch = PLAYER_SPAWN_PITCH;
        cameraFront = glm::normalize(glm::vec3(
            std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            std::sin(glm::radians(pitch)),
            std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
        firstMouse = true;
        isFlying = false;
        mountainElevator.Reset();
        worldReady = true;
        currentLoadingProgress = 1.0f;
        loadingReadyAt = static_cast<float>(glfwGetTime());
    };

    // Cargar texturas del cielo
    // 4. BUCLE PRINCIPAL DE RENDERIZADO
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::clamp(currentFrame - lastFrame, 0.0f, 0.05f);
        lastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (menuCalmAudioReady) {
            if (currentState == STATE_MENU && !isMenuCalmAudio) {
                ma_sound_start(&sfxMenuCalm);
                isMenuCalmAudio = true;
            }
            else if (currentState != STATE_MENU && isMenuCalmAudio) {
                ma_sound_stop(&sfxMenuCalm);
                ma_sound_seek_to_pcm_frame(&sfxMenuCalm, 0);
                isMenuCalmAudio = false;
            }
        }

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
                    mountainElevator.Reset();
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
            // Cambios de ambiente: ver EnvironmentSystem.cpp.
            const EnvironmentFrame environmentFrame = environmentSystem.Update(window, gamepad, deltaTime, currentFrame);
            const bool inCristoZone = IsInCristoAudioZone(cameraPos);
            environmentAudio.SetCityAmbienceMuted(inCristoZone);
            environmentAudio.Update(environmentFrame, environmentSystem);
            traffic.Update(deltaTime, physics, cameraPos, cameraFront);
            if (!environmentSystem.IsMenuOpen()) {
                if (!vehicle.IsDriving()) {
                    GameplayInput::ApplyGamepadCamera(gamepad, deltaTime, yaw, pitch, cameraFront);
                }
                vehicle.Update(window, gamepad, deltaTime, physics, cameraPos, cameraPos, cameraFront, yaw, pitch);
                if (!mountainElevator.IsRiding()) {
                    processInput(window, physics, gamepad, vehicle.IsDriving());
                }
                mountainElevator.Update(window, gamepad, deltaTime, cameraPos, !vehicle.IsDriving() && !isFlying);
            }
            else {
                mountainElevator.Update(window, gamepad, deltaTime, cameraPos, false);
            }
            UpdateCristoNatureAudio(inCristoZone);
            // Animacion de caminar/correr: ver PlayerMovementAnimation.cpp.
            movementAnimation.Update(window, environmentSystem.IsMenuOpen() ? GameplayGamepadInput{} : gamepad, deltaTime, cameraFront, cameraUp, !isFlying && !vehicle.IsDriving() && !mountainElevator.IsRiding());

            // Gravedad
            if (!isFlying && !vehicle.IsDriving() && !mountainElevator.IsRiding()) {
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

            cameraPos = ClampToWorldBounds(cameraPos);
            const glm::vec3 animatedCameraPos = cameraPos + movementAnimation.GetCameraOffset();
            const EnvironmentFrame renderFrame = BuildLocalizedFogFrame(environmentFrame, animatedCameraPos, currentFrame);

            glClearColor(renderFrame.clearColor.x, renderFrame.clearColor.y, renderFrame.clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 3000.0f);
            glm::mat4 view = glm::lookAt(animatedCameraPos, animatedCameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);
            if (Localization::shadowsEnabled && currentFrame >= nextShadowUpdate && environmentFrame.sunHeight > -0.05f) {
                shadowLightSpaceMatrix = RenderDirectionalShadow(
                    shadowShader, shadowFramebuffer, renderFrame, animatedCameraPos, cameraFront,
                    physics, cityMeshBounds, citySceneBounds.radius);
                nextShadowUpdate = currentFrame + (1.0f / 6.0f);
            }

            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setMat4("lightSpaceMatrix", shadowLightSpaceMatrix);
            UploadCityEnvironment(cityShader, renderFrame, animatedCameraPos, currentFrame);
            vehicle.UploadHeadlights(cityShader, renderFrame.nightFactor, renderFrame.rainIntensity);
            cityShader.setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowTexture);

            const Optimization::FrameCulling cityCulling = Optimization::BuildFrameCulling(
                projection,
                view,
                animatedCameraPos,
                cameraFront,
                citySceneBounds.radius);
            Optimization::RenderSettings frameRenderSettings = cityRenderSettings;
            frameRenderSettings.hideCristoArea = ShouldHideCristoArea(animatedCameraPos);
            frameRenderSettings.hideCristoCutoffArea = ShouldHideCristoCutoffArea(animatedCameraPos);
            frameRenderSettings.hideVolcanoArea = ShouldHideVolcanoForCristoView(animatedCameraPos, cameraFront);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling, frameRenderSettings);
            vehicle.Draw(cityShader);
            traffic.Draw(cityShader, vehicle, animatedCameraPos);
            if (!frameRenderSettings.hideCristoArea && !frameRenderSettings.hideCristoCutoffArea) {
                mountainElevator.Draw(cityShader, currentFrame);
            }
            cityBackdrop.Draw(cityShader, renderFrame.fogIntensity, ComputeCristoCityFogIntensity(animatedCameraPos, currentFrame), animatedCameraPos, currentFrame);

            DrawDynamicSky(skyboxShader, skyboxVAO, renderFrame, view, projection, animatedCameraPos, currentFrame);
            // Punto central de interaccion: ver InteractionReticle.cpp.
            const MonumentType nearbyMonument = DetectNearbyMonument(cameraPos);
            InteractionReticle::DrawCenterDot(
                static_cast<float>(SCR_WIDTH),
                static_cast<float>(SCR_HEIGHT),
                gamepad.interactDown || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || mountainElevator.CanInteract() || nearbyMonument != MONUMENT_NONE);
            mountainElevator.DrawHud(static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));
            WeatherOverlay::Draw(environmentFrame, currentFrame, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));
            UpdateMonumentInteraction(window, gamepad, cameraPos);
            mainMenu.DrawEnvironmentMenu(environmentSystem);
            break;
        }

        case STATE_PAUSE: {
            vehicle.SetAudioPaused(true);
            StopFootstepAudio();
            // Mantenemos el renderizado del mapa estático al fondo para ver los cambios del menú en tiempo real
            const EnvironmentFrame environmentFrame = environmentSystem.BuildFrame(currentFrame);
            const EnvironmentFrame renderFrame = BuildLocalizedFogFrame(environmentFrame, cameraPos, currentFrame);

            glClearColor(renderFrame.clearColor.x, renderFrame.clearColor.y, renderFrame.clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 3000.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);
            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setMat4("lightSpaceMatrix", shadowLightSpaceMatrix);
            UploadCityEnvironment(cityShader, renderFrame, cameraPos, currentFrame);
            vehicle.UploadHeadlights(cityShader, renderFrame.nightFactor, renderFrame.rainIntensity);
            cityShader.setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowTexture);

            const Optimization::FrameCulling cityCulling = Optimization::BuildFrameCulling(
                projection,
                view,
                cameraPos,
                cameraFront,
                citySceneBounds.radius);
            Optimization::RenderSettings frameRenderSettings = cityRenderSettings;
            frameRenderSettings.hideCristoArea = ShouldHideCristoArea(cameraPos);
            frameRenderSettings.hideCristoCutoffArea = ShouldHideCristoCutoffArea(cameraPos);
            frameRenderSettings.hideVolcanoArea = ShouldHideVolcanoForCristoView(cameraPos, cameraFront);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling, frameRenderSettings);
            vehicle.Draw(cityShader);
            traffic.Draw(cityShader, vehicle, cameraPos);
            if (!frameRenderSettings.hideCristoArea && !frameRenderSettings.hideCristoCutoffArea) {
                mountainElevator.Draw(cityShader, currentFrame);
            }
            cityBackdrop.Draw(cityShader, renderFrame.fogIntensity, ComputeCristoCityFogIntensity(cameraPos, currentFrame), cameraPos, currentFrame);

            DrawDynamicSky(skyboxShader, skyboxVAO, renderFrame, view, projection, cameraPos, currentFrame);

            // --- INTERFAZ DEL MENÚ DE PAUSA ---
            const MainMenuAction pauseAction = mainMenu.DrawPause(environmentSystem);
            if (pauseAction == MainMenuAction::Resume) {
                vehicle.SetAudioPaused(false);
                currentState = STATE_RUNNING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            else if (pauseAction == MainMenuAction::TravelCity ||
                pauseAction == MainMenuAction::TravelClock ||
                pauseAction == MainMenuAction::TravelStatue ||
                pauseAction == MainMenuAction::TravelVolcano) {
                vehicle.ResetForMainMenu();
                const TravelDestination* destination = &TRAVEL_CITY;
                if (pauseAction == MainMenuAction::TravelClock) {
                    destination = &TRAVEL_CLOCK;
                }
                else if (pauseAction == MainMenuAction::TravelStatue) {
                    destination = &TRAVEL_STATUE;
                }
                else if (pauseAction == MainMenuAction::TravelVolcano) {
                    destination = &TRAVEL_VOLCANO;
                }
                TravelTo(*destination);
                vehicle.SetAudioPaused(false);
                currentState = STATE_RUNNING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            else if (pauseAction == MainMenuAction::ReturnToMainMenu) {
                vehicle.ResetForMainMenu();
                environmentAudio.StopAmbient();
                UpdateCristoNatureAudio(false);
                StopFootstepAudio();
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
    cityBackdrop.Shutdown();
    mountainElevator.Shutdown();
    vehicle.Shutdown();
    mainMenu.Shutdown();
    ma_sound_uninit(&sfxPasos);
    ma_sound_uninit(&sfxCorrer);
    if (menuCalmAudioReady) {
        ma_sound_uninit(&sfxMenuCalm);
    }
    if (menuButtonAudioReady) {
        ma_sound_uninit(&sfxMenuButtons);
    }
    if (cristoWalkAudioReady) {
        ma_sound_uninit(&sfxCristoPasos);
    }
    if (cristoRunAudioReady) {
        ma_sound_uninit(&sfxCristoCorrer);
    }
    if (cristoNatureAudioReady) {
        ma_sound_uninit(&sfxCristoNaturaleza);
    }
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
    // --- CONTROL DE PAUSA CON ESCAPE ---
    const bool pausePressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || gamepad.pauseDown;
    if (pausePressed && !pauseToggleWasPressed && currentState == STATE_RUNNING) {
        currentState = STATE_PAUSE;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    pauseToggleWasPressed = pausePressed;

    // Si el juego está en el Menú Principal o en Pausa, no procesamos el movimiento del jugador
    if (currentState != STATE_RUNNING) return;
    if (vehicleDriving) return;

    if (processFlightInput(window)) {
        return;
    }

    // --- LÓGICA DE MOVIMIENTO (FPS) ---
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

        cameraPos = ClampToWorldBounds(proposedPos);

        const bool cristoZone = IsInLowerCristoForestZone(cameraPos);
        if (isSprinting) {
            StartRunningAudio(cristoZone);
        }
        else {
            StartWalkingAudio(cristoZone);
        }
    }
    else {
        StopFootstepAudio();
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

    StopFootstepAudio();

    glm::vec3 right = glm::cross(cameraFront, cameraUp);
    if (glm::length(right) > 0.0001f) {
        right = glm::normalize(right);
    }

    glm::vec3 flyDirection(0.0f);
    if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)) flyDirection += cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)) flyDirection -= cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)) flyDirection -= right;
    if ((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)) flyDirection += right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) flyDirection += cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) flyDirection -= cameraUp;

    if (glm::length(flyDirection) > 0.0001f) {
        const float speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 60.0f : 30.0f;
        cameraPos += glm::normalize(flyDirection) * speed * deltaTime;
        cameraPos = ClampToWorldBounds(cameraPos);
    }
    return true;
}

void UpdateWindowTitle(GLFWwindow* window, float currentFrame) {
    static float nextUpdateTime = 0.0f;
    if (currentFrame < nextUpdateTime) {
        return;
    }
    nextUpdateTime = currentFrame + 0.1f;

    char title[160];
    std::snprintf(
        title,
        sizeof(title),
        "Motor Grafico - ExplorerMaps | X: %.1f  Y: %.1f  Z: %.1f%s",
        cameraPos.x,
        cameraPos.y,
        cameraPos.z,
        isFlying ? " | VOLANDO" : "");
    glfwSetWindowTitle(window, title);
}

glm::vec3 ClampToWorldBounds(const glm::vec3& position) {
    glm::vec3 clamped = position;
    clamped.x = std::clamp(clamped.x, WORLD_MIN_X, WORLD_MAX_X);
    clamped.z = std::clamp(clamped.z, WORLD_MIN_Z, WORLD_MAX_Z);
    return clamped;
}

void AddWorldBoundaryCollision(PhysicsWorld& physics) {
    const float centerX = (WORLD_MIN_X + WORLD_MAX_X) * 0.5f;
    const float centerZ = (WORLD_MIN_Z + WORLD_MAX_Z) * 0.5f;
    const float width = WORLD_MAX_X - WORLD_MIN_X;
    const float depth = WORLD_MAX_Z - WORLD_MIN_Z;
    const float wallHeight = 900.0f;
    const float wallY = 220.0f;
    const float wallThickness = 48.0f;

    physics.AddCollisionBox(glm::vec3(WORLD_MIN_X - wallThickness * 0.5f, wallY, centerZ), glm::vec3(wallThickness, wallHeight, depth + wallThickness * 2.0f));
    physics.AddCollisionBox(glm::vec3(WORLD_MAX_X + wallThickness * 0.5f, wallY, centerZ), glm::vec3(wallThickness, wallHeight, depth + wallThickness * 2.0f));
    physics.AddCollisionBox(glm::vec3(centerX, wallY, WORLD_MIN_Z - wallThickness * 0.5f), glm::vec3(width + wallThickness * 2.0f, wallHeight, wallThickness));
    physics.AddCollisionBox(glm::vec3(centerX, wallY, WORLD_MAX_Z + wallThickness * 0.5f), glm::vec3(width + wallThickness * 2.0f, wallHeight, wallThickness));
}

bool IsInCristoAudioZone(const glm::vec3& position) {
    const glm::vec2 point(position.x, position.z);
    const glm::vec2 zone[] = {
        glm::vec2(329.3f, 420.0f),
        glm::vec2(151.0f, 1730.7f),
        glm::vec2(-1169.6f, 1556.5f),
        glm::vec2(-994.6f, 238.6f)
    };

    bool inside = false;
    for (int i = 0, j = 3; i < 4; j = i++) {
        const bool crosses = ((zone[i].y > point.y) != (zone[j].y > point.y)) &&
            (point.x < (zone[j].x - zone[i].x) * (point.y - zone[i].y) / (zone[j].y - zone[i].y) + zone[i].x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

bool IsInLowerCristoForestZone(const glm::vec3& position) {
    return IsInCristoAudioZone(position) && position.y < 145.0f;
}

void UpdateCristoNatureAudio(bool insideCristoZone) {
    if (!cristoNatureAudioReady) {
        return;
    }

    const float targetVolume = insideCristoZone ? 0.44f : 0.0f;
    ma_sound_set_volume(&sfxCristoNaturaleza, targetVolume);

    if (insideCristoZone && !isCristoNatureAudio) {
        ma_sound_start(&sfxCristoNaturaleza);
        isCristoNatureAudio = true;
    }
    else if (!insideCristoZone && isCristoNatureAudio) {
        ma_sound_stop(&sfxCristoNaturaleza);
        isCristoNatureAudio = false;
    }
}

void StopFootstepAudio() {
    if (isMovingAudio) {
        ma_sound_stop(&sfxPasos);
        isMovingAudio = false;
    }
    if (isRunningAudio) {
        ma_sound_stop(&sfxCorrer);
        isRunningAudio = false;
    }
    if (isCristoMovingAudio && cristoWalkAudioReady) {
        ma_sound_stop(&sfxCristoPasos);
        isCristoMovingAudio = false;
    }
    if (isCristoRunningAudio && cristoRunAudioReady) {
        ma_sound_stop(&sfxCristoCorrer);
        isCristoRunningAudio = false;
    }
}

void StartWalkingAudio(bool cristoZone) {
    if (cristoZone && cristoWalkAudioReady) {
        if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
        if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }
        if (isCristoRunningAudio && cristoRunAudioReady) { ma_sound_stop(&sfxCristoCorrer); isCristoRunningAudio = false; }
        if (!isCristoMovingAudio) { ma_sound_start(&sfxCristoPasos); isCristoMovingAudio = true; }
        return;
    }

    if (isCristoMovingAudio && cristoWalkAudioReady) { ma_sound_stop(&sfxCristoPasos); isCristoMovingAudio = false; }
    if (isCristoRunningAudio && cristoRunAudioReady) { ma_sound_stop(&sfxCristoCorrer); isCristoRunningAudio = false; }
    if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }
    if (!isMovingAudio) { ma_sound_start(&sfxPasos); isMovingAudio = true; }
}

void StartRunningAudio(bool cristoZone) {
    if (cristoZone && cristoRunAudioReady) {
        if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
        if (isRunningAudio) { ma_sound_stop(&sfxCorrer); isRunningAudio = false; }
        if (isCristoMovingAudio && cristoWalkAudioReady) { ma_sound_stop(&sfxCristoPasos); isCristoMovingAudio = false; }
        if (!isCristoRunningAudio) { ma_sound_start(&sfxCristoCorrer); isCristoRunningAudio = true; }
        return;
    }

    if (isCristoMovingAudio && cristoWalkAudioReady) { ma_sound_stop(&sfxCristoPasos); isCristoMovingAudio = false; }
    if (isCristoRunningAudio && cristoRunAudioReady) { ma_sound_stop(&sfxCristoCorrer); isCristoRunningAudio = false; }
    if (isMovingAudio) { ma_sound_stop(&sfxPasos); isMovingAudio = false; }
    if (!isRunningAudio) { ma_sound_start(&sfxCorrer); isRunningAudio = true; }
}

float ComputeLandmarkFogIntensity(const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame) {
    const auto zoneMask = [](const glm::vec3& position, const glm::vec3& center, float innerRadius, float outerRadius) {
        const float distance = glm::length(glm::vec2(position.x - center.x, position.z - center.z));
        return 1.0f - glm::smoothstep(innerRadius, outerRadius, distance);
    };

    const glm::vec3 cristoViewCenter(-455.0f, 382.0f, 940.0f);
    const glm::vec3 volcanoFogCenter(-105.0f, 388.0f, 880.0f);
    const float cristoMask = zoneMask(viewPosition, cristoViewCenter, 95.0f, 390.0f);
    const float volcanoMask = zoneMask(viewPosition, volcanoFogCenter, 170.0f, 580.0f);
    const float landmarkMask = (std::max)(cristoMask * 0.92f, volcanoMask * 0.52f);
    const float mountainHeightMask = glm::smoothstep(40.0f, 180.0f, viewPosition.y);

    const float breathing = 0.92f + 0.08f * std::sin(currentFrame * 0.37f);
    const float weatherBoost = frame.rainIntensity * 0.10f + frame.fogIntensity * 0.04f;
    return std::clamp(landmarkMask * mountainHeightMask * (0.62f * breathing + weatherBoost), 0.0f, 0.50f);
}

float ComputeCristoCityFogIntensity(const glm::vec3& viewPosition, float currentFrame) {
    const glm::vec3 cristoViewCenter(-455.0f, 382.0f, 940.0f);
    const float distance = glm::length(glm::vec2(viewPosition.x - cristoViewCenter.x, viewPosition.z - cristoViewCenter.z));
    const float cristoMask = 1.0f - glm::smoothstep(120.0f, 430.0f, distance);
    const float heightMask = glm::smoothstep(250.0f, 360.0f, viewPosition.y);
    const float breathing = 0.94f + 0.06f * std::sin(currentFrame * 0.31f);
    return std::clamp(cristoMask * heightMask * 0.72f * breathing, 0.0f, 0.68f);
}

EnvironmentFrame BuildLocalizedFogFrame(const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame) {
    EnvironmentFrame localizedFrame = frame;
    const float landmarkFog = ComputeLandmarkFogIntensity(frame, viewPosition, currentFrame);
    const float cristoZCutoffFog = 1.0f - glm::smoothstep(-660.0f, -420.0f, viewPosition.z);
    const float cristoXCutoffFog = 1.0f - glm::smoothstep(-1800.0f, -200.0f, viewPosition.x);
    const float cristoCutoffFog = (std::max)(cristoZCutoffFog, cristoXCutoffFog) * (0.82f + frame.rainIntensity * 0.12f);
    localizedFrame.fogIntensity = std::clamp((std::max)(landmarkFog, cristoCutoffFog), 0.0f, 0.86f);

    const glm::vec3 mountainFogColor = glm::mix(
        glm::vec3(0.72f, 0.78f, 0.84f),
        glm::vec3(0.22f, 0.25f, 0.31f),
        glm::clamp(frame.nightFactor + frame.rainIntensity * 0.35f, 0.0f, 1.0f));
    localizedFrame.clearColor = glm::mix(
        frame.clearColor,
        mountainFogColor,
        localizedFrame.fogIntensity * 0.30f);

    return localizedFrame;
}

bool ShouldHideCristoArea(const glm::vec3& viewPosition) {
    const glm::vec3 markedViewpoints[] = {
        glm::vec3(-1240.7f, 136.2f, 367.4f)
    };

    for (const glm::vec3& viewpoint : markedViewpoints) {
        const float horizontalDistance = glm::length(glm::vec2(viewPosition.x - viewpoint.x, viewPosition.z - viewpoint.z));
        const float verticalDistance = std::abs(viewPosition.y - viewpoint.y);
        if (horizontalDistance < 115.0f && verticalDistance < 95.0f) {
            return true;
        }
    }
    return false;
}

bool ShouldHideCristoCutoffArea(const glm::vec3& viewPosition) {
    constexpr float cristoCutoffZ = -460.0f;
    constexpr float cristoCutoffX = -980.0f;
    constexpr float displayRoundingTolerance = 0.5f;
    return viewPosition.z <= cristoCutoffZ + displayRoundingTolerance ||
        viewPosition.x <= cristoCutoffX + displayRoundingTolerance;
}

bool ShouldHideVolcanoForCristoView(const glm::vec3& viewPosition, const glm::vec3& viewDirection) {
    (void)viewDirection;
    return IsInCristoAudioZone(viewPosition);
}

bool IsPlayerInBuildingZone(const glm::vec3& playerPosition) {
    if (playerPosition.y < 51.0f) {
        return false;
    }

    constexpr float centerX = 179.65f;
    constexpr float centerZ = 223.75f;
    constexpr float radius = 32.0f;
    const float distanceX = playerPosition.x - centerX;
    const float distanceZ = playerPosition.z - centerZ;
    return (distanceX * distanceX + distanceZ * distanceZ) <= radius * radius;
}

bool IsPlayerInVolcanoZone(const glm::vec3& playerPosition) {
    if (playerPosition.y < 37.0f) {
        return false;
    }

    constexpr float minX = -980.0f;
    constexpr float maxX = -450.0f;
    constexpr float minZ = -200.0f;
    constexpr float maxZ = -125.0f;
    return playerPosition.x >= minX && playerPosition.x <= maxX &&
        playerPosition.z >= minZ && playerPosition.z <= maxZ;
}

bool IsPlayerInCristoZone(const glm::vec3& playerPosition) {
    if (playerPosition.y < 360.0f) {
        return false;
    }

    const glm::vec3 center = TRAVEL_STATUE.lookAt;
    constexpr float radius = 48.0f;
    const float distanceX = playerPosition.x - center.x;
    const float distanceZ = playerPosition.z - center.z;
    return (distanceX * distanceX + distanceZ * distanceZ) <= radius * radius;
}

MonumentType DetectNearbyMonument(const glm::vec3& playerPosition) {
    if (IsPlayerInBuildingZone(playerPosition)) {
        return MONUMENT_RELOJ;
    }
    if (IsPlayerInCristoZone(playerPosition)) {
        return MONUMENT_CRISTO;
    }
    if (IsPlayerInVolcanoZone(playerPosition)) {
        return MONUMENT_VOLCAN;
    }
    return MONUMENT_NONE;
}

void DrawMonumentInteractPrompt() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const char* beforeKey = Localization::Text("PRESIONA ", "PRESS ");
    const char* afterKey = Localization::Text(" PARA INSPECCIONAR MONUMENTO", " TO INSPECT MONUMENT");
    const ImVec2 padding(22.0f, 10.0f);
    const ImVec2 beforeSize = ImGui::CalcTextSize(beforeKey);
    const ImVec2 keySize = ImGui::CalcTextSize("E");
    const ImVec2 afterSize = ImGui::CalcTextSize(afterKey);
    const float keyWidth = keySize.x + 18.0f;
    const float totalWidth = beforeSize.x + keyWidth + afterSize.x + padding.x * 2.0f + 14.0f;
    const float totalHeight = (std::max)(beforeSize.y, keySize.y + 6.0f) + padding.y * 2.0f;

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.78f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("MonumentInteractPrompt", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + totalWidth, min.y + totalHeight);
        drawList->AddRectFilled(min, max, IM_COL32(14, 18, 28, 220), 10.0f);
        drawList->AddRect(min, max, IM_COL32(60, 160, 255, 120), 10.0f, 0, 1.25f);

        const ImVec2 textPos(min.x + padding.x, min.y + padding.y + 2.0f);
        drawList->AddText(textPos, IM_COL32(230, 230, 240, 255), beforeKey);

        const ImVec2 keyMin(textPos.x + beforeSize.x + 6.0f, min.y + padding.y - 1.0f);
        const ImVec2 keyMax(keyMin.x + keyWidth, keyMin.y + keySize.y + 8.0f);
        drawList->AddRectFilled(keyMin, keyMax, IM_COL32(10, 120, 200, 255), 6.0f);
        drawList->AddRect(keyMin, keyMax, IM_COL32(255, 255, 255, 48), 6.0f, 0, 1.0f);
        drawList->AddText(ImVec2(keyMin.x + (keyWidth - keySize.x) * 0.5f, keyMin.y + 4.0f), IM_COL32(245, 245, 245, 255), "E");

        drawList->AddText(ImVec2(keyMax.x + 8.0f, textPos.y), IM_COL32(230, 230, 240, 255), afterKey);
    }
    ImGui::End();
}

void DrawProgrammedMonumentFicha(MonumentType type) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float panelWidth = (std::min)(display.x * 0.82f, 920.0f);
    float panelHeight = 380.0f;
    const char* title = "";
    const char* description = "";
    ImVec4 headerColor(1.0f, 1.0f, 1.0f, 1.0f);

    if (type == MONUMENT_RELOJ) {
        title = Localization::Text("RELOJ DE DIRIAMBA", "DIRIAMBA CLOCK");
        description = Localization::Text(
            "Construido en 1935, esta torre de 15.5 metros es el segundo monumento mas importante de la ciudad. Su estructura fue declarada Patrimonio Cultural en 2002.",
            "Built in 1935, this 15.5-meter tower is the second most important monument in the city. Its structure was declared Cultural Heritage in 2002.");
        headerColor = ImVec4(0.98f, 0.73f, 0.05f, 1.0f);
    }
    else if (type == MONUMENT_VOLCAN) {
        title = Localization::Text("VOLCAN MASAYA", "MASAYA VOLCANO");
        description = Localization::Text(
            "Este volcan activo alberga el crater Santiago, uno de los pocos lugares del mundo con un lago de lava persistente. Como dato historico, el fraile espanol Francisco de Bobadilla coloco una gran cruz en su cumbre en 1529 para exorcizar al volcan, al que los conquistadores creian una entrada directa al infierno.",
            "This active volcano contains the Santiago crater, one of the few places in the world with a persistent lava lake. Historically, the Spanish friar Francisco de Bobadilla placed a large cross on its summit in 1529 to exorcise the volcano, which conquistadors believed was a direct entrance to hell.");
        headerColor = ImVec4(1.00f, 0.25f, 0.05f, 1.0f);
        panelHeight = 470.0f;
    }
    else if (type == MONUMENT_CRISTO) {
        title = Localization::Text("CRISTO REDENTOR", "CHRIST THE REDEEMER");
        description = Localization::Text(
            "El Cristo Redentor o Cristo del Corcovado es una estatua art deco que representa a Jesus de Nazaret, con los brazos abiertos, mostrando a la ciudad de Rio de Janeiro, Brasil.",
            "Christ the Redeemer, also known as Christ of Corcovado, is an art deco statue representing Jesus of Nazareth with open arms overlooking the city of Rio de Janeiro, Brazil.");
        headerColor = ImVec4(0.62f, 0.84f, 1.0f, 1.0f);
    }

    panelHeight = (std::min)(panelHeight, display.y * 0.82f);
    ImGui::SetNextWindowPos(ImVec2((display.x - panelWidth) * 0.5f, (display.y - panelHeight) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.04f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.55f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("MonumentFicha", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 headerMin(windowPos.x + 12.0f, windowPos.y + 12.0f);
        const ImVec2 headerMax(windowPos.x + panelWidth - 12.0f, headerMin.y + 86.0f);
        drawList->AddRectFilled(headerMin, headerMax, IM_COL32(15, 18, 26, 255), 10.0f);
        drawList->AddRectFilled(headerMin, ImVec2(headerMax.x, headerMin.y + 6.0f), ImGui::ColorConvertFloat4ToU32(headerColor), 10.0f);

        const float titleFontSize = 24.0f;
        const ImVec2 titleSize = ImGui::GetFont()->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, title);
        const ImVec2 titlePos(
            headerMin.x + ((panelWidth - 24.0f) - titleSize.x) * 0.5f,
            headerMin.y + (86.0f - titleSize.y) * 0.5f + 2.0f);
        drawList->AddText(ImGui::GetFont(), titleFontSize, ImVec2(titlePos.x + 2.0f, titlePos.y + 2.0f), IM_COL32(0, 0, 0, 200), title);
        drawList->AddText(ImGui::GetFont(), titleFontSize, titlePos, ImGui::ColorConvertFloat4ToU32(headerColor), title);

        ImGui::SetCursorPosY(116.0f);
        ImGui::PushTextWrapPos(panelWidth - 48.0f);
        ImGui::SetWindowFontScale(0.62f);
        ImGui::TextWrapped("%s", description);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopTextWrapPos();
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void UpdateMonumentInteraction(GLFWwindow* window, const GameplayGamepadInput& gamepad, const glm::vec3& playerPosition) {
    const MonumentType nearbyMonument = DetectNearbyMonument(playerPosition);
    if (nearbyMonument == MONUMENT_NONE) {
        showMonumentFicha = false;
        monumentInteractWasDown = false;
        activeMonument = MONUMENT_NONE;
        return;
    }

    activeMonument = nearbyMonument;
    if (!showMonumentFicha) {
        DrawMonumentInteractPrompt();
    }

    const bool interactDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gamepad.interactDown;
    if (interactDown && !monumentInteractWasDown) {
        showMonumentFicha = !showMonumentFicha;
    }
    monumentInteractWasDown = interactDown;

    if (showMonumentFicha) {
        DrawProgrammedMonumentFicha(activeMonument);
    }
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

void TravelTo(const TravelDestination& destination) {
    cameraPos = destination.position;
    cameraFront = glm::normalize(destination.lookAt - destination.position);
    yaw = glm::degrees(std::atan2(cameraFront.z, cameraFront.x));
    pitch = glm::degrees(std::asin(glm::clamp(cameraFront.y, -1.0f, 1.0f)));
    firstMouse = true;
    isFlying = false;
}

void UploadCityEnvironment(Shader& shader, const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame) {
    shader.setVec3("objectTint", glm::vec3(1.0f));
    shader.setFloat("objectAlpha", 1.0f);
    shader.setFloat("vehicleSurface", 0.0f);
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

glm::mat4 RenderDirectionalShadow(
    Shader& shader,
    unsigned int framebuffer,
    const EnvironmentFrame& frame,
    const glm::vec3& cameraPosition,
    const glm::vec3& cameraForward,
    const PhysicsWorld& physics,
    const std::vector<Optimization::MeshBounds>& meshBounds,
    float sceneRadius) {
    glm::vec3 shadowCenter = cameraPosition + cameraForward * 48.0f;
    constexpr float shadowHalfWidth = 112.0f;
    constexpr float shadowHalfHeight = 94.0f;
    const float shadowWorldTexel = (shadowHalfWidth * 2.0f) / static_cast<float>(SHADOW_MAP_SIZE);
    shadowCenter.x = std::floor(shadowCenter.x / shadowWorldTexel) * shadowWorldTexel;
    shadowCenter.z = std::floor(shadowCenter.z / shadowWorldTexel) * shadowWorldTexel;
    glm::vec3 directionToLight = frame.mainLightPosition - shadowCenter;
    directionToLight = glm::length(directionToLight) > 0.001f
        ? glm::normalize(directionToLight)
        : glm::vec3(0.35f, 0.75f, -0.45f);

    const glm::vec3 lightPosition = shadowCenter + directionToLight * 300.0f;
    const glm::vec3 lightUp = std::abs(glm::dot(directionToLight, cameraUp)) > 0.96f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : cameraUp;
    const glm::mat4 lightProjection = glm::ortho(
        -shadowHalfWidth, shadowHalfWidth,
        -shadowHalfHeight, shadowHalfHeight,
        1.0f, 600.0f);
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
    const Optimization::FrameCulling shadowCulling = Optimization::BuildFrameCulling(
        lightProjection,
        lightView,
        lightPosition,
        glm::normalize(shadowCenter - lightPosition),
        sceneRadius);
    Optimization::RenderSettings shadowSettings;
    shadowSettings.maxRenderDistance = 520.0f;
    shadowSettings.lodNearDistanceMultiplier = 0.10f;
    shadowSettings.lodMidDistanceMultiplier = 0.30f;
    shadowSettings.lodTinyMeshScreenRatio = 0.0012f;
    shadowSettings.lodSmallMeshScreenRatio = 0.0030f;
    shadowSettings.viewportCullPadding = 0.07f;
    shadowSettings.hideCristoArea = ShouldHideCristoArea(cameraPosition);
    shadowSettings.hideCristoCutoffArea = ShouldHideCristoCutoffArea(cameraPosition);
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
