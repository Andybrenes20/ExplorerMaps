#include "LightManager.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <imgui.h>

namespace
{
    glm::vec3 OffsetForDuplicate(const Light& light)
    {
        const float offset = std::max(light.helperSize * 1.5f, 6.0f);
        return glm::vec3(offset, 0.0f, offset);
    }
}

void LightManager::Bind(std::vector<Light>* boundLights)
{
    lights = boundLights;
    if (!lights)
    {
        return;
    }

    for (const Light& light : *lights)
    {
        if (!light.id.empty())
        {
            const std::size_t lastUnderscore = light.id.find_last_of('_');
            if (lastUnderscore != std::string::npos)
            {
                const int parsed = std::atoi(light.id.substr(lastUnderscore + 1).c_str());
                sequence = std::max(sequence, parsed);
            }
        }
    }
}

void LightManager::SyncSelection(const SceneSelection& selection)
{
    if (!lights)
    {
        return;
    }

    for (int i = 0; i < static_cast<int>(lights->size()); ++i)
    {
        (*lights)[i].selected = selection.type == SceneObjectType::Light && selection.index == i;
    }
}

void LightManager::HandleShortcuts(GLFWwindow* window, const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty)
{
    if (!lights)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || ImGui::IsAnyItemActive())
    {
        duplicateLatch = false;
        deleteLatch = false;
        return;
    }

    const bool ctrlDown = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool dDown = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    const bool deleteDown = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
    const bool nDown = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;

    if (ctrlDown && dDown && !duplicateLatch && HasLightSelection(selection))
    {
        const int duplicateIndex = DuplicateSelected(selection);
        if (duplicateIndex >= 0)
        {
            selection = { SceneObjectType::Light, duplicateIndex };
            dirty = true;
        }
    }
    duplicateLatch = ctrlDown && dDown;

    if (deleteDown && !deleteLatch && HasLightSelection(selection))
    {
        if (DeleteSelected(selection))
        {
            dirty = true;
        }
    }
    deleteLatch = deleteDown;

    static bool createLatch = false;
    if (ctrlDown && nDown && !createLatch)
    {
        const int newIndex = CreatePointLight(spawnPosition);
        selection = { SceneObjectType::Light, newIndex };
        dirty = true;
    }
    createLatch = ctrlDown && nDown;
}

int LightManager::CreatePointLight(const glm::vec3& position)
{
    lights->push_back(MakeLight("Point Light", position, 36.0f, 6.0f, glm::vec3(1.0f, 0.92f, 0.78f)));
    return static_cast<int>(lights->size()) - 1;
}

int LightManager::CreateAreaLight(const glm::vec3& position)
{
    lights->push_back(MakeLight("Area Light", position, 90.0f, 12.0f, glm::vec3(0.85f, 0.92f, 1.0f)));
    return static_cast<int>(lights->size()) - 1;
}

int LightManager::CreateCubeLight(const glm::vec3& position)
{
    Light cubeLight = MakeLight("Cube Light", position, 55.0f, 12.0f, glm::vec3(1.0f, 0.86f, 0.62f));
    cubeLight.visualType = LightVisualType::Cube;
    cubeLight.boxSize = glm::vec3(10.0f, 10.0f, 10.0f);
    cubeLight.intensity = 2.8f;
    lights->push_back(cubeLight);
    return static_cast<int>(lights->size()) - 1;
}

int LightManager::DuplicateSelected(const SceneSelection& selection)
{
    if (!HasLightSelection(selection))
    {
        return -1;
    }

    Light duplicate = lights->at(selection.index);
    duplicate.id = "light_" + std::to_string(NextSequence());
    duplicate.name += " Copy";
    duplicate.position += OffsetForDuplicate(duplicate);
    duplicate.selected = false;
    lights->push_back(duplicate);
    return static_cast<int>(lights->size()) - 1;
}

bool LightManager::DeleteSelected(SceneSelection& selection)
{
    if (!HasLightSelection(selection))
    {
        return false;
    }

    lights->erase(lights->begin() + selection.index);
    if (lights->empty())
    {
        selection = {};
    }
    else
    {
        selection.index = std::clamp(selection.index, 0, static_cast<int>(lights->size()) - 1);
    }

    SyncSelection(selection);
    return true;
}

void LightManager::DrawHierarchy(const glm::vec3& spawnPosition, SceneSelection& selection)
{
    if (!lights)
    {
        return;
    }

    if (ImGui::Button("New point light"))
    {
        selection = { SceneObjectType::Light, CreatePointLight(spawnPosition) };
    }

    ImGui::SameLine();
    if (ImGui::Button("New area light"))
    {
        selection = { SceneObjectType::Light, CreateAreaLight(spawnPosition) };
    }

    ImGui::SameLine();
    if (ImGui::Button("New cube light"))
    {
        selection = { SceneObjectType::Light, CreateCubeLight(spawnPosition) };
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(lights->size()); ++i)
    {
        const bool selected = selection.type == SceneObjectType::Light && selection.index == i;
        if (ImGui::Selectable((*lights)[i].name.c_str(), selected))
        {
            selection = { SceneObjectType::Light, i };
        }
    }

    SyncSelection(selection);
}

void LightManager::DrawInspector(SceneSelection& selection, bool& dirty)
{
    Light* light = GetSelectedLight(selection);
    if (!light)
    {
        return;
    }

    char nameBuffer[128] = {};
    strncpy_s(nameBuffer, light->name.c_str(), sizeof(nameBuffer) - 1);

    ImGui::Text("Light: %s", light->name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
    {
        light->name = nameBuffer;
        dirty = true;
    }
    dirty |= ImGui::DragFloat3("Position", &light->position.x, 0.25f);
    dirty |= ImGui::ColorEdit3("Color", &light->color.x);
    dirty |= ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 20.0f);
    dirty |= ImGui::DragFloat("Radius", &light->radius, 0.25f, 1.0f, 800.0f);
    dirty |= ImGui::DragFloat("Helper size", &light->helperSize, 0.25f, 1.0f, 200.0f);
    int visualType = static_cast<int>(light->visualType);
    if (ImGui::Combo("Visual", &visualType, "Point\0Cube\0"))
    {
        light->visualType = visualType == 1 ? LightVisualType::Cube : LightVisualType::Point;
        dirty = true;
    }
    if (light->visualType == LightVisualType::Cube)
    {
        dirty |= ImGui::DragFloat3("Box size", &light->boxSize.x, 0.25f, 1.0f, 200.0f);
    }

    if (ImGui::Button("Duplicate light"))
    {
        const int duplicateIndex = DuplicateSelected(selection);
        if (duplicateIndex >= 0)
        {
            selection = { SceneObjectType::Light, duplicateIndex };
            dirty = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete light"))
    {
        dirty |= DeleteSelected(selection);
    }

    SyncSelection(selection);
}

bool LightManager::HasLightSelection(const SceneSelection& selection) const
{
    return selection.type == SceneObjectType::Light &&
        selection.index >= 0 &&
        lights &&
        selection.index < static_cast<int>(lights->size());
}

Light* LightManager::GetSelectedLight(const SceneSelection& selection)
{
    return HasLightSelection(selection) ? &lights->at(selection.index) : nullptr;
}

const Light* LightManager::GetSelectedLight(const SceneSelection& selection) const
{
    return HasLightSelection(selection) ? &lights->at(selection.index) : nullptr;
}

Light LightManager::MakeLight(const std::string& prefix, const glm::vec3& position, float radius, float helperSize, const glm::vec3& color) const
{
    Light light;
    const int id = NextSequence();
    light.id = "light_" + std::to_string(id);
    light.name = prefix + " " + std::to_string(id);
    light.position = position;
    light.color = color;
    light.intensity = 2.0f;
    light.radius = radius;
    light.helperSize = helperSize;
    light.selected = false;
    return light;
}

int LightManager::NextSequence() const
{
    ++sequence;
    return sequence;
}
