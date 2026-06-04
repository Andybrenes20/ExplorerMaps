#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- INCLUDES EXTERNOS ---
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "stb/stb_image.h" 

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "PhysicsWorld.h"
#include "Shader.h"

// --- CONFIGURACIÓN GLOBAL ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const float PLAYER_HEIGHT = 1.75f;

// --- VARIABLES DE BARRA DE PROGRESO ---
float currentLoadingProgress = 0.0f;
bool loadingStarted = false;

// --- VARIABLES GLOBALES DE AUDIO ---
ma_engine audioEngine;
ma_sound bgmCity;
ma_sound sfxPasos;
ma_sound sfxCorrer;
bool isMovingAudio = false;
bool isRunningAudio = false;

// --- MÁQUINA DE ESTADOS DE JUEGO ---
enum GameState { STATE_MENU, STATE_LOADING, STATE_RUNNING, STATE_PAUSE };
GameState currentState = STATE_MENU;

// --- CONTROL DE AMBIENTE MANUAL/AUTOMÁTICO ---
enum ModoAmbiente { MANUAL_DIA, MANUAL_NOCHE, AUTOMATICO };
ModoAmbiente modoActual = AUTOMATICO; // Controla qué lógica aplicar

// --- SISTEMA DE TIEMPO Y LUCES ---
float timeOfDay = 10.0f; // Empezamos a las 10:00 AM
float timeScale = 0.5f;  // Velocidad del tiempo
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
void processInput(GLFWwindow* window, PhysicsWorld& physics);
unsigned int loadCubemap(std::vector<std::string> faces);

int main() {
    // 1. INICIALIZAR GLFW Y CREAR VENTANA
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
    ma_sound_init_from_file(&audioEngine, "Sonidos/City.mp3", 0, NULL, NULL, &bgmCity);
    ma_sound_set_looping(&bgmCity, MA_TRUE);
    ma_sound_set_volume(&bgmCity, 0.4f);

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Instanciar mundos físicos y Shaders
    unsigned int skyboxVAO, skyboxVBO;
    unsigned int cubemapDay, cubemapNight;

    PhysicsWorld physics;
    Shader cityShader("city.vert", "city.frag");
    Shader skyboxShader("skybox.vert", "skybox.frag");

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
    std::vector<std::string> facesDay = {
        "Texturas/Skybox_Day/px.png", "Texturas/Skybox_Day/nx.png",
        "Texturas/Skybox_Day/py.png", "Texturas/Skybox_Day/ny.png",
        "Texturas/Skybox_Day/pz.png", "Texturas/Skybox_Day/nz.png"
    };
    cubemapDay = loadCubemap(facesDay);

    std::vector<std::string> facesNight = {
        "Texturas/Skybox_Nigth/px.png", "Texturas/Skybox_Nigth/nx.png",
        "Texturas/Skybox_Nigth/py.png", "Texturas/Skybox_Nigth/ny.png",
        "Texturas/Skybox_Nigth/pz.png", "Texturas/Skybox_Nigth/nz.png"
    };
    cubemapNight = loadCubemap(facesNight);

    skyboxShader.use();
    skyboxShader.setInt("skyboxDay", 0);
    skyboxShader.setInt("skyboxNight", 1);

    // 4. BUCLE PRINCIPAL DE RENDERIZADO
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
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

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH, SCR_HEIGHT));
            ImGui::Begin("MainMenu", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

            ImGui::SetCursorPosY(150);
            ImGui::SetWindowFontScale(3.0f);
            float tituloWidth = ImGui::CalcTextSize("EXPLORER MAPS").x;
            ImGui::SetCursorPosX((SCR_WIDTH - tituloWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "EXPLORER MAPS");

            ImGui::SetWindowFontScale(1.5f);
            ImGui::SetCursorPosY(350);
            ImGui::SetCursorPosX((SCR_WIDTH - 200) * 0.5f);
            if (ImGui::Button("INICIAR MUNDO", ImVec2(200, 50))) {
                currentState = STATE_LOADING;
            }

            ImGui::SetCursorPosY(420);
            ImGui::SetCursorPosX((SCR_WIDTH - 200) * 0.5f);
            if (ImGui::Button("CERRAR PROGRAMA", ImVec2(200, 50))) {
                glfwSetWindowShouldClose(window, true);
            }

            ImGui::End();
            break;
        }

        case STATE_LOADING: {
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (!loadingStarted) {
                loadFuture = std::async(std::launch::async, &PhysicsWorld::LoadCollisionData, &physics, "modelos/city.glb");
                loadingStarted = true;
            }

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH, SCR_HEIGHT));
            ImGui::Begin("LoadingScreen", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

            ImGui::SetCursorPosY(300);
            ImGui::SetWindowFontScale(2.0f);
            float textoWidth = ImGui::CalcTextSize("CARGANDO EXPLORERMAPS...").x;
            ImGui::SetCursorPosX((SCR_WIDTH - textoWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "CARGANDO EXPLORERMAPS...");

            ImGui::SetCursorPosY(380);
            ImGui::SetCursorPosX((SCR_WIDTH - 600) * 0.5f);
            ImGui::ProgressBar(currentLoadingProgress, ImVec2(600, 30), "PREPARANDO EL MAPA...");

            ImGui::End();

            if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                bool exito = loadFuture.get();
                if (exito && !physics.cityCollider.vertices.empty()) {

                    physics.UploadToGPU();

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

                    ma_sound_start(&bgmCity);
                }
                currentState = STATE_RUNNING;
            }
            break;
        }

        case STATE_RUNNING: {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            processInput(window, physics);

            // Gravedad
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

            // --- SISTEMA DE SELECCIÓN DE AMBIENTE ---
            if (modoActual == MANUAL_DIA) {
                timeOfDay = 12.0f; // Forzar mediodía constante
            }
            else if (modoActual == MANUAL_NOCHE) {
                timeOfDay = 24.0f; // Forzar medianoche constante
            }
            else {
                // Modo AUTOMÁTICO: El tiempo fluye solo
                timeOfDay += deltaTime * timeScale;
                if (timeOfDay >= 24.0f) timeOfDay -= 24.0f;
            }

            // --- CÁLCULOS MATEMÁTICOS DE ILUMINACIÓN ---
            float sunAngle = ((timeOfDay - 6.0f) / 24.0f) * glm::two_pi<float>();
            glm::vec3 sunDirection = glm::vec3(cos(sunAngle), sin(sunAngle), 0.2f);
            float blendFactor = glm::clamp(0.5f - (sunDirection.y * 5.0f), 0.0f, 1.0f);

            glm::vec3 dayAmbient(0.4f, 0.4f, 0.4f);
            glm::vec3 nightAmbient(0.005f, 0.005f, 0.015f);
            glm::vec3 currentAmbient = glm::mix(dayAmbient, nightAmbient, blendFactor);

            glm::vec3 dayDiffuse(0.9f, 0.8f, 0.7f);
            glm::vec3 nightDiffuse(0.01f, 0.02f, 0.05f);

            if (blendFactor > 0.1f && blendFactor < 0.9f) {
                dayDiffuse = glm::mix(dayDiffuse, glm::vec3(1.0f, 0.3f, 0.1f), 1.0f - abs(blendFactor - 0.5f) * 2.0f);
            }
            glm::vec3 currentDiffuse = glm::mix(dayDiffuse, nightDiffuse, blendFactor);

            glm::vec3 activeLightDir = (sunDirection.y >= -0.1f) ? -sunDirection : sunDirection;

            float streetlightIntensity = (blendFactor > 0.5f) ? (blendFactor - 0.5f) * 2.0f : 0.0f;

            glClearColor(currentAmbient.x, currentAmbient.y, currentAmbient.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);

            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setVec3("viewPos", cameraPos);

            // Luz Direccional
            cityShader.setVec3("light.direction", activeLightDir);
            cityShader.setVec3("light.ambient", currentAmbient);
            cityShader.setVec3("light.diffuse", currentDiffuse);
            cityShader.setVec3("light.specular", currentDiffuse * 1.5f);

            // Luces Puntuales (Faroles)
            cityShader.setFloat("pointLightIntensity", streetlightIntensity);
            glm::vec3 farolColor(1.0f, 0.6f, 0.2f);

            for (int i = 0; i < 4; i++) {
                std::string num = std::to_string(i);
                cityShader.setVec3("pointLights[" + num + "].position", farolesPos[i]);
                cityShader.setVec3("pointLights[" + num + "].ambient", farolColor * 0.1f);
                cityShader.setVec3("pointLights[" + num + "].diffuse", farolColor);
                cityShader.setFloat("pointLights[" + num + "].constant", 1.0f);
                cityShader.setFloat("pointLights[" + num + "].linear", 0.09f);
                cityShader.setFloat("pointLights[" + num + "].quadratic", 0.032f);
            }

            for (const auto& mesh : physics.visualMeshes) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
            }

            // DIBUJAR SKYBOX
            glDepthFunc(GL_LEQUAL);
            skyboxShader.use();
            glm::mat4 viewSkybox = glm::mat4(glm::mat3(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)));
            skyboxShader.setMat4("view", viewSkybox);
            skyboxShader.setMat4("projection", projection);
            skyboxShader.setFloat("blendFactor", blendFactor);

            glBindVertexArray(skyboxVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapDay);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapNight);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS);
            break;
        }

        case STATE_PAUSE: {
            // Mantenemos el renderizado del mapa estático al fondo para ver los cambios del menú en tiempo real
            float sunAngle = ((timeOfDay - 6.0f) / 24.0f) * glm::two_pi<float>();
            glm::vec3 sunDirection = glm::vec3(cos(sunAngle), sin(sunAngle), 0.2f);
            float blendFactor = glm::clamp(0.5f - (sunDirection.y * 5.0f), 0.0f, 1.0f);

            glm::vec3 dayAmbient(0.4f, 0.4f, 0.4f);
            glm::vec3 nightAmbient(0.005f, 0.005f, 0.015f);
            glm::vec3 currentAmbient = glm::mix(dayAmbient, nightAmbient, blendFactor);
            glm::vec3 dayDiffuse(0.9f, 0.8f, 0.7f);
            glm::vec3 nightDiffuse(0.01f, 0.02f, 0.05f);
            if (blendFactor > 0.1f && blendFactor < 0.9f) {
                dayDiffuse = glm::mix(dayDiffuse, glm::vec3(1.0f, 0.3f, 0.1f), 1.0f - abs(blendFactor - 0.5f) * 2.0f);
            }
            glm::vec3 currentDiffuse = glm::mix(dayDiffuse, nightDiffuse, blendFactor);
            glm::vec3 activeLightDir = (sunDirection.y >= -0.1f) ? -sunDirection : sunDirection;
            float streetlightIntensity = (blendFactor > 0.5f) ? (blendFactor - 0.5f) * 2.0f : 0.0f;

            glClearColor(currentAmbient.x, currentAmbient.y, currentAmbient.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 model = glm::mat4(1.0f);

            cityShader.use();
            cityShader.setMat4("projection", projection);
            cityShader.setMat4("view", view);
            cityShader.setMat4("model", model);
            cityShader.setVec3("viewPos", cameraPos);
            cityShader.setVec3("light.direction", activeLightDir);
            cityShader.setVec3("light.ambient", currentAmbient);
            cityShader.setVec3("light.diffuse", currentDiffuse);
            cityShader.setVec3("light.specular", currentDiffuse * 1.5f);
            cityShader.setFloat("pointLightIntensity", streetlightIntensity);

            glm::vec3 farolColor(1.0f, 0.6f, 0.2f);
            for (int i = 0; i < 4; i++) {
                std::string num = std::to_string(i);
                cityShader.setVec3("pointLights[" + num + "].position", farolesPos[i]);
                cityShader.setVec3("pointLights[" + num + "].ambient", farolColor * 0.1f);
                cityShader.setVec3("pointLights[" + num + "].diffuse", farolColor);
                cityShader.setFloat("pointLights[" + num + "].constant", 1.0f);
                cityShader.setFloat("pointLights[" + num + "].linear", 0.09f);
                cityShader.setFloat("pointLights[" + num + "].quadratic", 0.032f);
            }

            for (const auto& mesh : physics.visualMeshes) {
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glBindVertexArray(mesh.VAO); glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
            }

            glDepthFunc(GL_LEQUAL);
            skyboxShader.use();
            glm::mat4 viewSkybox = glm::mat4(glm::mat3(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)));
            skyboxShader.setMat4("view", viewSkybox); skyboxShader.setMat4("projection", projection);
            skyboxShader.setFloat("blendFactor", blendFactor);
            glBindVertexArray(skyboxVAO);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapDay);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapNight);
            glDrawArrays(GL_TRIANGLES, 0, 36); glBindVertexArray(0); glDepthFunc(GL_LESS);

            // --- INTERFAZ DEL MENÚ DE PAUSA ---
            ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH * 0.35f, SCR_HEIGHT * 0.25f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(400, 320), ImGuiCond_Always);

            ImGui::Begin("MENU DE PAUSA", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "--- CONTROL AMBIENTAL ---");
            ImGui::Separator();

            int opcionModo = static_cast<int>(modoActual);
            if (ImGui::RadioButton("Dia", &opcionModo, 0)) modoActual = MANUAL_DIA;
            if (ImGui::RadioButton("Noche", &opcionModo, 1)) modoActual = MANUAL_NOCHE;
            if (ImGui::RadioButton("AUTO", &opcionModo, 2)) modoActual = AUTOMATICO;

            ImGui::Separator();

            if (modoActual == AUTOMATICO) {
                ImGui::SliderFloat("Velocidad del Tiempo", &timeScale, 0.0f, 5.0f, "%.1fx");
                ImGui::Text("Hora actual: %.2f hrs", timeOfDay);
            }
            else {
                if (ImGui::SliderFloat("Ajustar Hora", &timeOfDay, 0.0f, 24.0f, "%.1f hrs")) {
                    if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) modoActual = MANUAL_DIA;
                    else modoActual = MANUAL_NOCHE;
                }
            }

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            if (ImGui::Button("REGRESAR AL JUEGO", ImVec2(380, 40))) {
                currentState = STATE_RUNNING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            if (ImGui::Button("SALIR AL MENU PRINCIPAL", ImVec2(380, 40))) {
                ma_sound_stop(&bgmCity);
                // Reiniciamos variables de carga para permitir re-entrada limpia
                loadingStarted = false;
                currentLoadingProgress = 0.0f;
                currentState = STATE_MENU;
            }

            ImGui::End();
            break;
        }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. LIMPIEZA
    ma_sound_uninit(&bgmCity);
    ma_sound_uninit(&sfxPasos);
    ma_sound_uninit(&sfxCorrer);
    ma_engine_uninit(&audioEngine);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

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

void processInput(GLFWwindow* window, PhysicsWorld& physics) {
    // --- CONTROL DE PAUSA CON ESCAPE ---
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
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

    // --- LÓGICA DE MOVIMIENTO (FPS) ---
    float baseSpeed = 6.0f;
    bool isSprinting = false;

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        baseSpeed = 12.0f;
        isSprinting = true;
    }

    float cameraSpeed = baseSpeed * deltaTime;
    glm::vec3 frontPlano = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 rightPlano = glm::normalize(glm::cross(frontPlano, cameraUp));

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += frontPlano;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= frontPlano;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= rightPlano;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += rightPlano;

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

unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = GL_RGB;
            if (nrChannels == 4) format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap falló en: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}