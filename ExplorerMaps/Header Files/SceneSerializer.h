#pragma once

#include <string>

#include "EditorTypes.h"

class SceneSerializer
{
public:
    bool Load(const std::string& filePath, EditorSceneData& scene) const;
    bool Save(const std::string& filePath, const EditorSceneData& scene) const;
};
