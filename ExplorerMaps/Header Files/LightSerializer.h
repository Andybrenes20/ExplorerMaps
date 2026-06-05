#pragma once

#include <string>
#include <vector>

#include "EditorTypes.h"

class LightSerializer
{
public:
    bool Load(const std::string& filePath, std::vector<Light>& lights) const;
    bool Save(const std::string& filePath, const std::vector<Light>& lights) const;
};
