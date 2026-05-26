#pragma once

#include <string>

#include "ColliderManager.h"

class ColliderSerializer
{
public:
    bool Load(const std::string& filePath, ColliderManager& manager) const;
    bool Save(const std::string& filePath, const ColliderManager& manager) const;
};

