#pragma once

#include <vector>

#include "ColliderTypes.h"

class ColliderManager
{
public:
    void SetColliders(const std::vector<BoxCollider>& values);
    void Clear();

    const std::vector<BoxCollider>& GetColliders() const;
    std::vector<BoxCollider>& GetColliders();

    int AddBox(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotation = glm::vec3(0.0f));
    int Duplicate(int index);
    bool Remove(int index);

    bool IsValidIndex(int index) const;
    BoxCollider* Get(int index);
    const BoxCollider* Get(int index) const;

private:
    void SyncSequence();
    int BuildNextSequence();
    BoxCollider BuildDefaultCollider(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotation) const;

    std::vector<BoxCollider> colliders;
    int sequence = 0;
};

