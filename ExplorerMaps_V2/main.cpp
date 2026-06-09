#include <iostream>
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
#include "MainMenu.h"
#include "LoadingScreen.h"
#include "Shader.h"

// --- CONFIGURACIÓN GLOBAL ---
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const float PLAYER_HEIGHT = 1.75f;
const glm::vec3 PLAYER_SPAWN = glm::vec3(304.1f, 37.8f, 143.5f);
const float PLAYER_SPAWN_YAW = -90.0f;
const float PLAYER_SPAWN_PITCH = 0.0f;
const unsigned int SHADOW_MAP_SIZE = 1536;

// --- VARIABLES DE BARRA DE PROGRESO ---
float currentLoadingProgress = 0.0f;
bool loadingStarted = false;

// --- VARIABLES GLOBALES DE AUDIO ---
ma_engine audioEngine;
ma_sound sfxPasos;
ma_sound sfxCorrer;
bool isMovingAudio = false;
bool isRunningAudio = false;
bool isFlying = false;
bool flyToggleWasPressed = false;

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

// --- DECLARACIÓN DE FUNCIONES ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void processInput(GLFWwindow* window, PhysicsWorld& physics, const GameplayGamepadInput& gamepad, bool vehicleDriving);
bool processFlightInput(GLFWwindow* window);
void UpdateWindowTitle(GLFWwindow* window, float currentFrame);
void UploadCityEnvironment(Shader& shader, const EnvironmentFrame& frame, const glm::vec3& viewPosition, float currentFrame);
void DrawDynamicSky(Shader& shader, unsigned int skyboxVAO, const EnvironmentFrame& frame, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float currentFrame);
glm::mat4 RenderDirectionalShadow(Shader& shader, unsigned int framebuffer, const EnvironmentFrame& frame, const glm::vec3& cameraPosition, const glm::vec3& cameraForward, const PhysicsWorld& physics, const std::vector<Optimization::MeshBounds>& meshBounds, float sceneRadius);

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

    // Instanciar mundos físicos y Shaders
    unsigned int skyboxVAO, skyboxVBO;

    PhysicsWorld physics;
    std::vector<Optimization::MeshBounds> cityMeshBounds;
    Optimization::SceneBounds citySceneBounds;
    PlayerMovementAnimation movementAnimation;
    VehicleController vehicle;
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

    static std::future<bool> loadFuture;

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

    // Cargar texturas del cielo
    // 4. BUCLE PRINCIPAL DE RENDERIZADO
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        switch (currentState) {

        case STATE_MENU: {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const MainMenuAction menuAction = mainMenu.Draw(currentFrame);
            if (menuAction == MainMenuAction::Start) {
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

            if (!loadingStarted) {
                loadFuture = std::async(std::launch::async, &PhysicsWorld::LoadCollisionData, &physics, "modelos/city.glb");
                loadingStarted = true;
            }

            loadingScreen.Draw(currentLoadingProgress, currentFrame);

            if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                bool exito = loadFuture.get();
                if (exito && !physics.cityCollider.vertices.empty()) {

                    cityMeshBounds = Optimization::BuildMeshBounds(physics.visualMeshes);
                    citySceneBounds = Optimization::BuildSceneBounds(cityMeshBounds);
                    physics.UploadToGPU();
                    if (vehicle.Load("modelos/nissan.glb")) {
                        vehicle.UploadToGPU();
                    }

                    glm::vec3 minBounds = physics.cityCollider.vertices[0];
                    glm::vec3 maxBounds = physics.cityCollider.vertices[0];
                    for (const auto& v : physics.cityCollider.vertices) {
                        if (v.x < minBounds.x) minBounds.x = v.x; if (v.y < minBounds.y) minBounds.y = v.y; if (v.z < minBounds.z) minBounds.z = v.z;
                        if (v.x > maxBounds.x) maxBounds.x = v.x; if (v.y > maxBounds.y) maxBounds.y = v.y; if (v.z > maxBounds.z) maxBounds.z = v.z;
                    }
                    float centroX = (minBounds.x + maxBounds.x) / 2.0f;
                    float centroZ = (minBounds.z + maxBounds.z) / 2.0f;

                    glm::vec3 origenCielo(centroX, maxBounds.y + 50.0f, centroZ);
                    float distanciaAlSuelo = 0.0f;

                    if (physics.Raycast(origenCielo, glm::vec3(0.0f, -1.0f, 0.0f), distanciaAlSuelo)) {
                        float alturaSueloFisico = origenCielo.y - distanciaAlSuelo;
                        cameraPos = glm::vec3(centroX, alturaSueloFisico + PLAYER_HEIGHT, centroZ);

                        farolesPos[0] = glm::vec3(centroX + 15.0f, alturaSueloFisico + 4.0f, centroZ + 15.0f);
                        farolesPos[1] = glm::vec3(centroX - 15.0f, alturaSueloFisico + 4.0f, centroZ - 15.0f);
                        farolesPos[2] = glm::vec3(centroX + 15.0f, alturaSueloFisico + 4.0f, centroZ - 15.0f);
                        farolesPos[3] = glm::vec3(centroX - 15.0f, alturaSueloFisico + 4.0f, centroZ + 15.0f);
                    }
                    else {
                        cameraPos = glm::vec3(centroX, 10.0f, centroZ);
                        for (int i = 0; i < 4; i++) farolesPos[i] = glm::vec3(centroX, 14.0f, centroZ);
                    }

                    cameraPos = PLAYER_SPAWN;
                    yaw = PLAYER_SPAWN_YAW;
                    pitch = PLAYER_SPAWN_PITCH;
                    cameraFront = glm::normalize(glm::vec3(
                        std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
                        std::sin(glm::radians(pitch)),
                        std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
                    firstMouse = true;
                    isFlying = false;
                    environmentAudio.StartAmbient();
                }
                currentState = STATE_RUNNING;
            }
            break;
        }

        case STATE_RUNNING: {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            const GameplayGamepadInput gamepad = GameplayInput::ReadGamepad();
            // Cambios de ambiente: ver EnvironmentSystem.cpp.
            const EnvironmentFrame environmentFrame = environmentSystem.Update(window, gamepad, deltaTime, currentFrame);
            environmentAudio.Update(environmentFrame, environmentSystem);
            if (!environmentSystem.IsMenuOpen()) {
                if (!vehicle.IsDriving()) {
                    GameplayInput::ApplyGamepadCamera(gamepad, deltaTime, yaw, pitch, cameraFront);
                }
                vehicle.Update(window, gamepad, deltaTime, physics, cameraPos, cameraPos, cameraFront, yaw, pitch);
                processInput(window, physics, gamepad, vehicle.IsDriving());
            }
            // Animacion de caminar/correr: ver PlayerMovementAnimation.cpp.
            movementAnimation.Update(window, environmentSystem.IsMenuOpen() ? GameplayGamepadInput{} : gamepad, deltaTime, cameraFront, cameraUp, !isFlying && !vehicle.IsDriving());

            // Gravedad
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

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
            const glm::vec3 animatedCameraPos = cameraPos + movementAnimation.GetCameraOffset();
            glm::mat4 view = glm::lookAt(animatedCameraPos, animatedCameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);
            if (currentFrame >= nextShadowUpdate && environmentFrame.sunHeight > -0.05f) {
                shadowLightSpaceMatrix = RenderDirectionalShadow(
                    shadowShader, shadowFramebuffer, environmentFrame, animatedCameraPos, cameraFront,
                    physics, cityMeshBounds, citySceneBounds.radius);
                nextShadowUpdate = currentFrame + (1.0f / 10.0f);
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
                projection,
                view,
                animatedCameraPos,
                cameraFront,
                citySceneBounds.radius);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling);
            vehicle.Draw(cityShader);

            DrawDynamicSky(skyboxShader, skyboxVAO, environmentFrame, view, projection, animatedCameraPos, currentFrame);
            // Punto central de interaccion: ver InteractionReticle.cpp.
            InteractionReticle::DrawCenterDot(
                static_cast<float>(SCR_WIDTH),
                static_cast<float>(SCR_HEIGHT),
                gamepad.interactDown || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
            WeatherOverlay::Draw(environmentFrame, currentFrame, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));
            mainMenu.DrawEnvironmentMenu(environmentSystem);
            break;
        }

        case STATE_PAUSE: {
            // Mantenemos el renderizado del mapa estático al fondo para ver los cambios del menú en tiempo real
            const EnvironmentFrame environmentFrame = environmentSystem.BuildFrame(currentFrame);

            glClearColor(environmentFrame.clearColor.x, environmentFrame.clearColor.y, environmentFrame.clearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
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
                projection,
                view,
                cameraPos,
                cameraFront,
                citySceneBounds.radius);
            Optimization::DrawCityMeshes(physics.visualMeshes, cityMeshBounds, cityCulling);
            vehicle.Draw(cityShader);

            DrawDynamicSky(skyboxShader, skyboxVAO, environmentFrame, view, projection, cameraPos, currentFrame);

            // --- INTERFAZ DEL MENÚ DE PAUSA ---
            const MainMenuAction pauseAction = mainMenu.DrawPause(environmentSystem);
            if (pauseAction == MainMenuAction::Resume) {
                currentState = STATE_RUNNING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            else if (pauseAction == MainMenuAction::ReturnToMainMenu) {
                environmentAudio.StopAmbient();
                loadingStarted = false;
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
        glfwPollEvents();
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
    // --- CONTROL DE PAUSA CON ESCAPE ---
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || gamepad.pauseDown) {
        if (currentState == STATE_RUNNING) {
            currentState = STATE_PAUSE;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Muestra el cursor para ImGui
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Evita rebotes por pulsación larga
        }
        else if (currentState == STATE_PAUSE) {
            currentState = STATE_RUNNING;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Oculta el cursor para jugar
            firstMouse = true; // Evita que la cámara dé un salto brusco al tomar el control del mouse
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

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
    if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)) flyDirection += cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)) flyDirection -= cameraFront;
    if ((glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)) flyDirection -= right;
    if ((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)||(glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)) flyDirection += right;
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
    shader.setFloat("shadowStrength", glm::smoothstep(-0.04f, 0.24f, frame.sunHeight) * (1.0f - frame.rainIntensity * 0.48f) * 0.98f);
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
    shadowSettings.maxRenderDistance = 900.0f;
    shadowSettings.lodNearDistanceMultiplier = 4.0f;
    shadowSettings.viewportCullPadding = 0.12f;
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
