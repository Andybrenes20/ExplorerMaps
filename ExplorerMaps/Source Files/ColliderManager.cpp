#include "ColliderManager.h"

#include <algorithm>

namespace
{
    int ParseSuffixSequence(const std::string& value, const char* prefix)
    {
        const std::string prefixValue(prefix);
        if (value.rfind(prefixValue, 0) != 0)
        {
            return 0;
        }

        int sequence = 0;
        for (std::size_t i = prefixValue.size(); i < value.size(); ++i)
        {
            const char character = value[i];
            if (character < '0' || character > '9')
            {
                return 0;
            }

            sequence = (sequence * 10) + static_cast<int>(character - '0');
        }

        return sequence;
    }
}

void ColliderManager::SetColliders(const std::vector<BoxCollider>& values)
{
    colliders = values;
    SyncSequence();
}

void ColliderManager::Clear()
{
    colliders.clear();
    sequence = 0;
}

const std::vector<BoxCollider>& ColliderManager::GetColliders() const
{
    return colliders;
}

std::vector<BoxCollider>& ColliderManager::GetColliders()
{
    return colliders;
}

int ColliderManager::AddBox(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotation)
{
    colliders.push_back(BuildDefaultCollider(position, scale, rotation));
    return static_cast<int>(colliders.size()) - 1;
}

int ColliderManager::Duplicate(int index)
{
    if (!IsValidIndex(index))
    {
        return -1;
    }

    BoxCollider duplicate = colliders[index];
    const int nextSequence = BuildNextSequence();
    duplicate.id = "collider_" + std::to_string(nextSequence);
    duplicate.name = "Collider " + std::to_string(nextSequence);
    duplicate.position += glm::vec3(2.0f, 0.0f, 2.0f);
    colliders.push_back(duplicate);
    return static_cast<int>(colliders.size()) - 1;
}

bool ColliderManager::Remove(int index)
{
    if (!IsValidIndex(index))
    {
        return false;
    }

    colliders.erase(colliders.begin() + index);
    return true;
}

bool ColliderManager::IsValidIndex(int index) const
{
    return index >= 0 && index < static_cast<int>(colliders.size());
}

BoxCollider* ColliderManager::Get(int index)
{
    return IsValidIndex(index) ? &colliders[index] : nullptr;
}

const BoxCollider* ColliderManager::Get(int index) const
{
    return IsValidIndex(index) ? &colliders[index] : nullptr;
}

void ColliderManager::SyncSequence()
{
    sequence = 0;
    for (const BoxCollider& collider : colliders)
    {
        sequence = std::max(sequence, ParseSuffixSequence(collider.id, "collider_"));
    }
}

int ColliderManager::BuildNextSequence()
{
    ++sequence;
    return sequence;
}

BoxCollider ColliderManager::BuildDefaultCollider(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& rotation) const
{
    BoxCollider collider;
    const int nextSequence = const_cast<ColliderManager*>(this)->BuildNextSequence();
    collider.id = "collider_" + std::to_string(nextSequence);
    collider.name = "Collider " + std::to_string(nextSequence);
    collider.position = position;
    collider.scale = glm::max(scale, glm::vec3(0.1f));
    collider.rotation = rotation;
    return collider;
}

