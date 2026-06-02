#include"Model.h"

#include<algorithm>
#include<array>
#include<cctype>
#include<cmath>
#include<cstring>
#include<stdexcept>

#include<assimp/Importer.hpp>
#include<assimp/material.h>
#include<assimp/postprocess.h>
#include<assimp/scene.h>
#include<assimp/texture.h>

namespace
{
	constexpr float kMeshCullDistanceMultiplier = 1.15f;
	constexpr float kCollisionEpsilon = 0.001f;
	constexpr float kLodNearDistanceMultiplier = 0.22f;
	constexpr float kLodMidDistanceMultiplier = 0.46f;
	constexpr float kLodTinyMeshScreenRatio = 0.0085f;
	constexpr float kLodSmallMeshScreenRatio = 0.0200f;
	constexpr float kViewportCullPadding = 0.035f;
	constexpr float kCityMaxRenderDistance = 1450.0f;
	constexpr float kPeripheralLodStart = 0.54f;
	constexpr float kPeripheralLodEnd = 1.0f;

	std::string toLowerCopy(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	bool hasExtension(const std::string& path, const char* extension)
	{
		const std::string lowerPath = toLowerCopy(path);
		const std::string lowerExtension = toLowerCopy(extension);
		return lowerPath.size() >= lowerExtension.size() &&
			lowerPath.compare(lowerPath.size() - lowerExtension.size(), lowerExtension.size(), lowerExtension) == 0;
	}

	bool isVehicleAssetPath(const std::string& path)
	{
		const std::string lowerPath = toLowerCopy(path);
		return lowerPath.find("coches") != std::string::npos ||
			lowerPath.find("car_1") != std::string::npos ||
			lowerPath.find("car/") != std::string::npos ||
			lowerPath.find("car\\") != std::string::npos;
	}

	bool isHeavyVehicleWheelMaterial(const std::string& materialNameLower)
	{
		return materialNameLower == "tire" ||
			materialNameLower == "rims" ||
			materialNameLower == "brake" ||
			materialNameLower.find("breaksredpaint") != std::string::npos;
	}

	std::string getDirectoryFromPath(const std::string& path)
	{
		const std::size_t separator = path.find_last_of("/\\");
		if (separator == std::string::npos)
		{
			return "";
		}

		return path.substr(0, separator + 1);
	}

	glm::mat4 aiToGlmMatrix(const aiMatrix4x4& matrix)
	{
		glm::mat4 result(1.0f);
		result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
		result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
		result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
		result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
		return result;
	}
	Texture buildEmbeddedTexture(const aiTexture* embeddedTexture, const char* texType, GLuint slot)
	{
		if (embeddedTexture == nullptr)
		{
			const unsigned char fallbackData[] = { 80, 80, 80, 255 };
			return Texture(fallbackData, 1, 1, texType, slot, GL_RGBA);
		}

		if (embeddedTexture->mHeight == 0)
		{
			return Texture(
				reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
				static_cast<int>(embeddedTexture->mWidth),
				texType,
				slot
			);
		}

		std::vector<unsigned char> rgbaPixels;
		rgbaPixels.reserve(static_cast<std::size_t>(embeddedTexture->mWidth) * embeddedTexture->mHeight * 4);
		for (unsigned int y = 0; y < embeddedTexture->mHeight; ++y)
		{
			for (unsigned int x = 0; x < embeddedTexture->mWidth; ++x)
			{
				const aiTexel& texel = embeddedTexture->pcData[y * embeddedTexture->mWidth + x];
				rgbaPixels.push_back(texel.r);
				rgbaPixels.push_back(texel.g);
				rgbaPixels.push_back(texel.b);
				rgbaPixels.push_back(texel.a);
			}
		}

		return Texture(
			rgbaPixels.data(),
			static_cast<int>(embeddedTexture->mWidth),
			static_cast<int>(embeddedTexture->mHeight),
			texType,
			slot,
			GL_RGBA
		);
	}

	Texture buildSolidTexture(const glm::vec3& color, const char* texType, GLuint slot)
	{
		const auto toChannel = [](float value) -> unsigned char
		{
			const float clamped = std::clamp(value, 0.0f, 1.0f);
			return static_cast<unsigned char>(clamped * 255.0f);
		};

		const unsigned char pixels[] =
		{
			toChannel(color.r),
			toChannel(color.g),
			toChannel(color.b),
			255
		};
		return Texture(pixels, 1, 1, texType, slot, GL_RGBA);
	}

	glm::vec3 closestPointOnTriangle(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
	{
		const glm::vec3 ab = b - a;
		const glm::vec3 ac = c - a;
		const glm::vec3 ap = point - a;

		const float d1 = glm::dot(ab, ap);
		const float d2 = glm::dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f)
		{
			return a;
		}

		const glm::vec3 bp = point - b;
		const float d3 = glm::dot(ab, bp);
		const float d4 = glm::dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3)
		{
			return b;
		}

		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			const float v = d1 / (d1 - d3);
			return a + v * ab;
		}

		const glm::vec3 cp = point - c;
		const float d5 = glm::dot(ab, cp);
		const float d6 = glm::dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6)
		{
			return c;
		}

		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			const float w = d2 / (d2 - d6);
			return a + w * ac;
		}

		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			const glm::vec3 bc = c - b;
			const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + w * bc;
		}

		const float denom = 1.0f / (va + vb + vc);
		const float v = vb * denom;
		const float w = vc * denom;
		return a + ab * v + ac * w;
	}

	float distanceSquaredXZ(const glm::vec3& point, const glm::vec3& candidate)
	{
		const float dx = point.x - candidate.x;
		const float dz = point.z - candidate.z;
		return dx * dx + dz * dz;
	}

	struct Frustum
	{
		std::array<glm::vec4, 6> planes;

		bool IntersectsSphere(const glm::vec3& center, float radius) const
		{
			for (const glm::vec4& plane : planes)
			{
				const float distance = glm::dot(glm::vec3(plane), center) + plane.w;
				if (distance < -radius)
				{
					return false;
				}
			}

			return true;
		}
	};

	struct ProjectedSphere
	{
		glm::vec2 center = glm::vec2(0.0f);
		float radius = 0.0f;
		bool valid = false;
	};

	glm::vec4 NormalizePlane(const glm::vec4& plane)
	{
		const float length = glm::length(glm::vec3(plane));
		if (length <= 0.0001f)
		{
			return plane;
		}

		return plane / length;
	}

	Frustum BuildFrustum(const glm::mat4& viewProjection)
	{
		Frustum frustum;
		frustum.planes[0] = NormalizePlane(glm::vec4(
			viewProjection[0][3] + viewProjection[0][0],
			viewProjection[1][3] + viewProjection[1][0],
			viewProjection[2][3] + viewProjection[2][0],
			viewProjection[3][3] + viewProjection[3][0]));
		frustum.planes[1] = NormalizePlane(glm::vec4(
			viewProjection[0][3] - viewProjection[0][0],
			viewProjection[1][3] - viewProjection[1][0],
			viewProjection[2][3] - viewProjection[2][0],
			viewProjection[3][3] - viewProjection[3][0]));
		frustum.planes[2] = NormalizePlane(glm::vec4(
			viewProjection[0][3] + viewProjection[0][1],
			viewProjection[1][3] + viewProjection[1][1],
			viewProjection[2][3] + viewProjection[2][1],
			viewProjection[3][3] + viewProjection[3][1]));
		frustum.planes[3] = NormalizePlane(glm::vec4(
			viewProjection[0][3] - viewProjection[0][1],
			viewProjection[1][3] - viewProjection[1][1],
			viewProjection[2][3] - viewProjection[2][1],
			viewProjection[3][3] - viewProjection[3][1]));
		frustum.planes[4] = NormalizePlane(glm::vec4(
			viewProjection[0][3] + viewProjection[0][2],
			viewProjection[1][3] + viewProjection[1][2],
			viewProjection[2][3] + viewProjection[2][2],
			viewProjection[3][3] + viewProjection[3][2]));
		frustum.planes[5] = NormalizePlane(glm::vec4(
			viewProjection[0][3] - viewProjection[0][2],
			viewProjection[1][3] - viewProjection[1][2],
			viewProjection[2][3] - viewProjection[2][2],
			viewProjection[3][3] - viewProjection[3][2]));
		return frustum;
	}

	float MaxWorldScale(const glm::mat4& transform)
	{
		return std::max({
			glm::length(glm::vec3(transform[0])),
			glm::length(glm::vec3(transform[1])),
			glm::length(glm::vec3(transform[2]))
		});
	}

	float SmoothStep(float edge0, float edge1, float value)
	{
		const float t = std::clamp((value - edge0) / std::max(edge1 - edge0, 0.0001f), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	ProjectedSphere ProjectSphereToViewport(const glm::mat4& viewProjection, const Camera& camera, const glm::vec3& worldCenter, float worldRadius)
	{
		ProjectedSphere projected;
		const glm::vec4 clipCenter = viewProjection * glm::vec4(worldCenter, 1.0f);
		if (std::abs(clipCenter.w) <= 0.0001f)
		{
			return projected;
		}

		const glm::vec2 centerNdc = glm::vec2(clipCenter) / clipCenter.w;
		glm::vec3 right = glm::cross(camera.Orientation, camera.Up);
		if (glm::length(right) <= 0.0001f)
		{
			right = glm::vec3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			right = glm::normalize(right);
		}

		const glm::vec4 clipOffset = viewProjection * glm::vec4(worldCenter + right * worldRadius, 1.0f);
		float radiusNdc = 0.0f;
		if (std::abs(clipOffset.w) > 0.0001f)
		{
			radiusNdc = glm::length((glm::vec2(clipOffset) / clipOffset.w) - centerNdc);
		}

		if (!std::isfinite(radiusNdc) || radiusNdc <= 0.0001f)
		{
			const float distance = glm::length(camera.Position - worldCenter);
			radiusNdc = worldRadius / std::max(distance, 0.001f);
		}

		projected.center = centerNdc;
		projected.radius = radiusNdc;
		projected.valid = std::isfinite(centerNdc.x) && std::isfinite(centerNdc.y);
		return projected;
	}

	bool IsInsideViewport(const ProjectedSphere& projected)
	{
		if (!projected.valid)
		{
			return false;
		}

		const float paddedRadius = projected.radius + kViewportCullPadding;
		return projected.center.x >= -1.0f - paddedRadius &&
			projected.center.x <= 1.0f + paddedRadius &&
			projected.center.y >= -1.0f - paddedRadius &&
			projected.center.y <= 1.0f + paddedRadius;
	}

	bool ShouldRenderByLod(float cameraDistance, float screenRadius, const ProjectedSphere& projected, float sceneRadius, bool cameraInsideStructure)
	{
		if (cameraInsideStructure || cameraDistance <= sceneRadius * kLodNearDistanceMultiplier)
		{
			return true;
		}

		const float viewportDistance = std::max(std::abs(projected.center.x), std::abs(projected.center.y));
		const float peripheralBoost = 1.0f + SmoothStep(kPeripheralLodStart, kPeripheralLodEnd, viewportDistance) * 0.85f;
		if (cameraDistance > sceneRadius * kLodMidDistanceMultiplier)
		{
			return screenRadius >= kLodSmallMeshScreenRatio * peripheralBoost;
		}

		return screenRadius >= kLodTinyMeshScreenRatio * peripheralBoost;
	}
}

Model::Model(const char* file)
{
	Model::file = file;
	const std::string path = file;
	lightweightVehicleAsset = isVehicleAssetPath(path);

	if (hasExtension(path, ".glb") || hasExtension(path, ".gltf"))
	{
		if (!lightweightVehicleAsset)
		{
			loadAssimpCollision(file);
		}
		loadAssimpModel(file);
		return;
	}

	throw std::runtime_error("Formato de modelo no soportado. Usa .gltf o .glb.");
}

void Model::Draw(Shader& shader, Camera& camera, const glm::mat4& worldTransform, bool cameraInsideStructure)
{
	shader.Activate();
	glUniform3f(shader.GetUniformLocation("camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
	camera.Matrix(shader, "camMatrix");

	const float worldScale = glm::length(glm::vec3(worldTransform[0]));
	const float sceneRadius = GetRadius() * worldScale;
	const float sceneCullDistance = GetRadius() * worldScale * (cameraInsideStructure ? 0.60f : kMeshCullDistanceMultiplier);
	const float maxVisibleDistance = cameraInsideStructure
		? std::max(sceneCullDistance, 220.0f)
		: std::min(std::max(sceneCullDistance, 220.0f), kCityMaxRenderDistance);
	const Frustum frustum = BuildFrustum(camera.cameraMatrix);

	// Go over all meshes and draw each one
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		const glm::mat4 meshWorldTransform = worldTransform * matricesMeshes[i];
		if (i < meshBoundsCenters.size() && i < meshBoundsRadii.size())
		{
			const glm::vec3 worldCenter = glm::vec3(meshWorldTransform * glm::vec4(meshBoundsCenters[i], 1.0f));
			const float worldRadius = meshBoundsRadii[i] * MaxWorldScale(meshWorldTransform);
			const float visibleDistance = maxVisibleDistance + worldRadius;
			const float visibleDistanceSquared = visibleDistance * visibleDistance;
			const glm::vec3 toMesh = camera.Position - worldCenter;
			const float distanceSquared = glm::dot(toMesh, toMesh);
			if (cameraInsideStructure && glm::dot(toMesh, toMesh) < (worldRadius * worldRadius))
			{
				continue;
			}
			if (distanceSquared > visibleDistanceSquared)
			{
				continue;
			}
			if (!frustum.IntersectsSphere(worldCenter, worldRadius))
			{
				continue;
			}
			const ProjectedSphere projected = ProjectSphereToViewport(camera.cameraMatrix, camera, worldCenter, worldRadius);
			if (!IsInsideViewport(projected))
			{
				continue;
			}
			if (!ShouldRenderByLod(std::sqrt(distanceSquared), projected.radius, projected, sceneRadius, cameraInsideStructure))
			{
				continue;
			}
		}

		meshes[i].Mesh::Draw(shader, meshWorldTransform);
	}
}

glm::vec3 Model::GetCenter() const
{
	if (!hasBounds)
	{
		return glm::vec3(0.0f, 0.0f, 0.0f);
	}

	return (boundsMin + boundsMax) * 0.5f;
}

glm::vec3 Model::GetBoundsMin() const
{
	return hasBounds ? boundsMin : glm::vec3(0.0f, 0.0f, 0.0f);
}

glm::vec3 Model::GetBoundsMax() const
{
	return hasBounds ? boundsMax : glm::vec3(0.0f, 0.0f, 0.0f);
}

float Model::GetRadius() const
{
	if (!hasBounds)
	{
		return 1.0f;
	}

	return glm::length(boundsMax - boundsMin) * 0.5f;
}

glm::vec3 Model::ResolveCollision(const glm::vec3& position, const glm::mat4& worldTransform, float radius) const
{
	glm::vec3 resolved = position;
	const glm::mat4 inverseWorldTransform = glm::inverse(worldTransform);
	const glm::vec3 localPosition = glm::vec3(inverseWorldTransform * glm::vec4(position, 1.0f));
	const float scaleX = glm::length(glm::vec3(worldTransform[0]));
	const float scaleY = glm::length(glm::vec3(worldTransform[1]));
	const float scaleZ = glm::length(glm::vec3(worldTransform[2]));
	const float localRadius = radius / std::max((scaleX + scaleY + scaleZ) / 3.0f, 0.0001f);
	glm::vec3 localResolved = localPosition;

	for (int iteration = 0; iteration < 2; ++iteration)
	{
		for (const CollisionMesh& collisionMesh : collisionMeshes)
		{
			const glm::vec3 expandedMin = collisionMesh.boundsMin - glm::vec3(localRadius);
			const glm::vec3 expandedMax = collisionMesh.boundsMax + glm::vec3(localRadius);
			if (localResolved.x < expandedMin.x || localResolved.x > expandedMax.x ||
				localResolved.y < expandedMin.y || localResolved.y > expandedMax.y ||
				localResolved.z < expandedMin.z || localResolved.z > expandedMax.z)
			{
				continue;
			}

			for (std::size_t index = 0; index + 2 < collisionMesh.indices.size(); index += 3)
			{
				const glm::vec3& a = collisionMesh.vertices[collisionMesh.indices[index]];
				const glm::vec3& b = collisionMesh.vertices[collisionMesh.indices[index + 1]];
				const glm::vec3& c = collisionMesh.vertices[collisionMesh.indices[index + 2]];
				const glm::vec3 closestPoint = closestPointOnTriangle(localResolved, a, b, c);
				glm::vec3 delta = localResolved - closestPoint;
				float distance = glm::length(delta);
				if (distance >= localRadius)
				{
					continue;
				}

				if (distance <= 0.0001f)
				{
					glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
					if (glm::length(normal) <= 0.0001f)
					{
						normal = glm::vec3(0.0f, 1.0f, 0.0f);
					}
					delta = normal;
					distance = 0.0f;
				}
				else
				{
					delta /= distance;
				}

				localResolved += delta * (localRadius - distance + kCollisionEpsilon);
			}
		}
	}

	resolved = glm::vec3(worldTransform * glm::vec4(localResolved, 1.0f));
	return resolved;
}

bool Model::TrySnapToWalkableSurface(
	const glm::vec3& position,
	const glm::mat4& worldTransform,
	float probeRadius,
	float eyeHeight,
	float maxStepUp,
	float maxDropDown,
	float maxSlopeDegrees,
	glm::vec3& snappedPosition) const
{
	if (collisionMeshes.empty())
	{
		return false;
	}

	const glm::mat4 inverseWorldTransform = glm::inverse(worldTransform);
	const glm::vec3 localPosition = glm::vec3(inverseWorldTransform * glm::vec4(position, 1.0f));
	const float scaleX = glm::length(glm::vec3(worldTransform[0]));
	const float scaleY = glm::length(glm::vec3(worldTransform[1]));
	const float scaleZ = glm::length(glm::vec3(worldTransform[2]));
	const float averageScale = std::max((scaleX + scaleY + scaleZ) / 3.0f, 0.0001f);
	const float localProbeRadius = probeRadius / averageScale;
	const float localEyeHeight = eyeHeight / averageScale;
	const float localMaxStepUp = maxStepUp / averageScale;
	const float localMaxDropDown = maxDropDown / averageScale;
	const glm::vec3 localFeetPosition = localPosition - glm::vec3(0.0f, localEyeHeight, 0.0f);
	const float minNormalY = std::cos(glm::radians(maxSlopeDegrees));
	const float maxHorizontalDistance = std::max(localProbeRadius * 3.0f, 0.25f);
	const float maxHorizontalDistanceSquared = maxHorizontalDistance * maxHorizontalDistance;

	bool foundSurface = false;
	float bestFootY = -FLT_MAX;
	float bestHorizontalDistanceSquared = FLT_MAX;

	auto evaluateCollisionMesh = [&](const CollisionMesh& collisionMesh, int collisionMeshIndex)
	{
		if (localFeetPosition.x < collisionMesh.boundsMin.x - maxHorizontalDistance ||
			localFeetPosition.x > collisionMesh.boundsMax.x + maxHorizontalDistance ||
			localFeetPosition.z < collisionMesh.boundsMin.z - maxHorizontalDistance ||
			localFeetPosition.z > collisionMesh.boundsMax.z + maxHorizontalDistance)
		{
			return;
		}

		if (localFeetPosition.y < collisionMesh.boundsMin.y - localMaxStepUp ||
			localFeetPosition.y > collisionMesh.boundsMax.y + localMaxDropDown)
		{
			return;
		}

		for (std::size_t index = 0; index + 2 < collisionMesh.indices.size(); index += 3)
		{
			const glm::vec3& a = collisionMesh.vertices[collisionMesh.indices[index]];
			const glm::vec3& b = collisionMesh.vertices[collisionMesh.indices[index + 1]];
			const glm::vec3& c = collisionMesh.vertices[collisionMesh.indices[index + 2]];
			const float triangleMinX = std::min(a.x, std::min(b.x, c.x));
			const float triangleMaxX = std::max(a.x, std::max(b.x, c.x));
			const float triangleMinZ = std::min(a.z, std::min(b.z, c.z));
			const float triangleMaxZ = std::max(a.z, std::max(b.z, c.z));
			if (localFeetPosition.x < triangleMinX - maxHorizontalDistance ||
				localFeetPosition.x > triangleMaxX + maxHorizontalDistance ||
				localFeetPosition.z < triangleMinZ - maxHorizontalDistance ||
				localFeetPosition.z > triangleMaxZ + maxHorizontalDistance)
			{
				continue;
			}

			const glm::vec3 triangleNormal = glm::cross(b - a, c - a);
			const float normalLength = glm::length(triangleNormal);
			if (normalLength <= 0.0001f)
			{
				continue;
			}

			const glm::vec3 normal = triangleNormal / normalLength;
			if (normal.y <= 0.0f || normal.y < minNormalY || std::abs(normal.y) <= 0.0001f)
			{
				continue;
			}

			const glm::vec3 closestPoint = closestPointOnTriangle(localFeetPosition, a, b, c);
			const float horizontalDistanceSquared = distanceSquaredXZ(localFeetPosition, closestPoint);
			if (horizontalDistanceSquared > maxHorizontalDistanceSquared)
			{
				continue;
			}

			const float heightDelta = closestPoint.y - localFeetPosition.y;
			if (heightDelta > localMaxStepUp || heightDelta < -localMaxDropDown)
			{
				continue;
			}

			const bool isBetterSurface =
				!foundSurface ||
				horizontalDistanceSquared < bestHorizontalDistanceSquared - 0.0001f ||
				(std::abs(horizontalDistanceSquared - bestHorizontalDistanceSquared) <= 0.0001f && closestPoint.y > bestFootY);
			if (!isBetterSurface)
			{
				continue;
			}

			foundSurface = true;
			bestFootY = closestPoint.y;
			bestHorizontalDistanceSquared = horizontalDistanceSquared;
			lastWalkableCollisionMeshIndex = collisionMeshIndex;
		}
	};

	if (lastWalkableCollisionMeshIndex >= 0 &&
		lastWalkableCollisionMeshIndex < static_cast<int>(collisionMeshes.size()))
	{
		evaluateCollisionMesh(collisionMeshes[static_cast<std::size_t>(lastWalkableCollisionMeshIndex)], lastWalkableCollisionMeshIndex);
		if (foundSurface && bestHorizontalDistanceSquared <= localProbeRadius * localProbeRadius)
		{
			glm::vec3 localSnappedPosition = localPosition;
			localSnappedPosition.y = bestFootY + localEyeHeight;
			snappedPosition = glm::vec3(worldTransform * glm::vec4(localSnappedPosition, 1.0f));
			return true;
		}
	}

	for (std::size_t collisionMeshIndex = 0; collisionMeshIndex < collisionMeshes.size(); ++collisionMeshIndex)
	{
		if (static_cast<int>(collisionMeshIndex) == lastWalkableCollisionMeshIndex)
		{
			continue;
		}

		evaluateCollisionMesh(collisionMeshes[collisionMeshIndex], static_cast<int>(collisionMeshIndex));
	}

	if (!foundSurface)
	{
		return false;
	}

	glm::vec3 localSnappedPosition = localPosition;
	localSnappedPosition.y = bestFootY + localEyeHeight;
	snappedPosition = glm::vec3(worldTransform * glm::vec4(localSnappedPosition, 1.0f));
	return true;
}

glm::vec3 Model::ResolveStructureCollision(
	const glm::vec3& targetPosition,
	const glm::vec3& previousPosition,
	const glm::mat4& worldTransform,
	float radius,
	float eyeHeight) const
{
	if (collisionMeshes.empty())
	{
		return targetPosition;
	}

	const glm::mat4 inverseWorldTransform = glm::inverse(worldTransform);
	const glm::vec3 localTargetPosition = glm::vec3(inverseWorldTransform * glm::vec4(targetPosition, 1.0f));
	const glm::vec3 localPreviousPosition = glm::vec3(inverseWorldTransform * glm::vec4(previousPosition, 1.0f));
	const float scaleX = glm::length(glm::vec3(worldTransform[0]));
	const float scaleY = glm::length(glm::vec3(worldTransform[1]));
	const float scaleZ = glm::length(glm::vec3(worldTransform[2]));
	const float averageScale = std::max((scaleX + scaleY + scaleZ) / 3.0f, 0.0001f);
	const float localRadius = radius / averageScale;
	const float localEyeHeight = eyeHeight / averageScale;
	const float localBodyHeight = std::max(localEyeHeight * 1.8f, localRadius * 2.0f);
	const float sceneRadius = std::max(GetRadius(), 1.0f);
	glm::vec3 localResolved = localTargetPosition;

	auto resolveAgainstMesh = [&](const CollisionMesh& collisionMesh, int collisionMeshIndex)
	{
		const glm::vec3 extents = collisionMesh.boundsMax - collisionMesh.boundsMin;
		const float footprint = std::max(extents.x, extents.z);
		if (extents.y < localBodyHeight * 3.5f ||
			footprint < localRadius * 4.0f ||
			footprint > sceneRadius * 0.18f)
		{
			return;
		}

		const float lowerCollisionTop = collisionMesh.boundsMin.y + extents.y * 0.55f;
		const float feetY = localResolved.y - localEyeHeight;
		const float headY = feetY + localBodyHeight;
		if (headY < collisionMesh.boundsMin.y || feetY > lowerCollisionTop)
		{
			return;
		}

		const float expandedMinX = collisionMesh.boundsMin.x - localRadius;
		const float expandedMaxX = collisionMesh.boundsMax.x + localRadius;
		const float expandedMinZ = collisionMesh.boundsMin.z - localRadius;
		const float expandedMaxZ = collisionMesh.boundsMax.z + localRadius;

		if (localResolved.x < expandedMinX || localResolved.x > expandedMaxX ||
			localResolved.z < expandedMinZ || localResolved.z > expandedMaxZ)
		{
			return;
		}

		const float pushLeft = std::abs(localResolved.x - expandedMinX);
		const float pushRight = std::abs(expandedMaxX - localResolved.x);
		const float pushBack = std::abs(localResolved.z - expandedMinZ);
		const float pushFront = std::abs(expandedMaxZ - localResolved.z);

		float minPush = pushLeft;
		glm::vec3 resolvedCandidate(expandedMinX, localResolved.y, localResolved.z);

		if (pushRight < minPush)
		{
			minPush = pushRight;
			resolvedCandidate = glm::vec3(expandedMaxX, localResolved.y, localResolved.z);
		}
		if (pushBack < minPush)
		{
			minPush = pushBack;
			resolvedCandidate = glm::vec3(localResolved.x, localResolved.y, expandedMinZ);
		}
		if (pushFront < minPush)
		{
			resolvedCandidate = glm::vec3(localResolved.x, localResolved.y, expandedMaxZ);
		}

		if (std::abs(localPreviousPosition.x - localResolved.x) > std::abs(localPreviousPosition.z - localResolved.z))
		{
			resolvedCandidate.x = (localPreviousPosition.x <= collisionMesh.boundsMin.x)
				? expandedMinX
				: expandedMaxX;
			resolvedCandidate.z = localResolved.z;
		}
		else if (std::abs(localPreviousPosition.z - localResolved.z) > 0.0001f)
		{
			resolvedCandidate.z = (localPreviousPosition.z <= collisionMesh.boundsMin.z)
				? expandedMinZ
				: expandedMaxZ;
			resolvedCandidate.x = localResolved.x;
		}

		localResolved = resolvedCandidate;
		lastStructureCollisionMeshIndex = collisionMeshIndex;
	};

	if (lastStructureCollisionMeshIndex >= 0 &&
		lastStructureCollisionMeshIndex < static_cast<int>(collisionMeshes.size()))
	{
		resolveAgainstMesh(collisionMeshes[static_cast<std::size_t>(lastStructureCollisionMeshIndex)], lastStructureCollisionMeshIndex);
	}

	for (std::size_t collisionMeshIndex = 0; collisionMeshIndex < collisionMeshes.size(); ++collisionMeshIndex)
	{
		if (static_cast<int>(collisionMeshIndex) == lastStructureCollisionMeshIndex)
		{
			continue;
		}

		resolveAgainstMesh(collisionMeshes[collisionMeshIndex], static_cast<int>(collisionMeshIndex));
	}

	return glm::vec3(worldTransform * glm::vec4(localResolved, 1.0f));
}

bool Model::IsInsideStructureVolume(
	const glm::vec3& position,
	const glm::mat4& worldTransform,
	float radius,
	float eyeHeight) const
{
	if (collisionMeshes.empty())
	{
		return false;
	}

	const glm::mat4 inverseWorldTransform = glm::inverse(worldTransform);
	const glm::vec3 localPosition = glm::vec3(inverseWorldTransform * glm::vec4(position, 1.0f));
	const float scaleX = glm::length(glm::vec3(worldTransform[0]));
	const float scaleY = glm::length(glm::vec3(worldTransform[1]));
	const float scaleZ = glm::length(glm::vec3(worldTransform[2]));
	const float averageScale = std::max((scaleX + scaleY + scaleZ) / 3.0f, 0.0001f);
	const float localRadius = radius / averageScale;
	const float localEyeHeight = eyeHeight / averageScale;
	const float localBodyHeight = std::max(localEyeHeight * 1.8f, localRadius * 2.0f);
	const float sceneRadius = std::max(GetRadius(), 1.0f);
	const float feetY = localPosition.y - localEyeHeight;
	const float headY = feetY + localBodyHeight;

	for (const CollisionMesh& collisionMesh : collisionMeshes)
	{
		const glm::vec3 extents = collisionMesh.boundsMax - collisionMesh.boundsMin;
		const float footprint = std::max(extents.x, extents.z);
		if (extents.y < localBodyHeight * 3.5f ||
			footprint < localRadius * 4.0f ||
			footprint > sceneRadius * 0.18f)
		{
			continue;
		}

		const float lowerCollisionTop = collisionMesh.boundsMin.y + extents.y * 0.95f;
		if (headY < collisionMesh.boundsMin.y || feetY > lowerCollisionTop)
		{
			continue;
		}

		const float expandedMinX = collisionMesh.boundsMin.x - localRadius;
		const float expandedMaxX = collisionMesh.boundsMax.x + localRadius;
		const float expandedMinZ = collisionMesh.boundsMin.z - localRadius;
		const float expandedMaxZ = collisionMesh.boundsMax.z + localRadius;
		if (localPosition.x >= expandedMinX && localPosition.x <= expandedMaxX &&
			localPosition.z >= expandedMinZ && localPosition.z <= expandedMaxZ)
		{
			return true;
		}
	}

	return false;
}

void Model::loadMesh(unsigned int indMesh)
{
	// Get all accessor indices
	unsigned int posAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
	unsigned int normalAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
	unsigned int texAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
	unsigned int indAccInd = JSON["meshes"][indMesh]["primitives"][0]["indices"];

	// Use accessor indices to get all vertices components
	std::vector<float> posVec = getFloats(JSON["accessors"][posAccInd]);
	std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
	std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
	std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
	std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);
	std::vector<Texture> textures = getTextures();

	glm::vec3 meshMin(FLT_MAX);
	glm::vec3 meshMax(-FLT_MAX);
	for (const glm::vec3& position : positions)
	{
		meshMin = glm::min(meshMin, position);
		meshMax = glm::max(meshMax, position);
	}
	const glm::vec3 meshCenter = (meshMin + meshMax) * 0.5f;
	float meshRadius = 0.0f;
	for (const glm::vec3& position : positions)
	{
		meshRadius = std::max(meshRadius, glm::distance(position, meshCenter));
	}
	meshBoundsCenters.push_back(meshCenter);
	meshBoundsRadii.push_back(meshRadius);

	// Combine the vertices, indices, and textures into a mesh
	meshes.push_back(Mesh(vertices, indices, textures));
}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix)
{
	// Current node
	json node = JSON["nodes"][nextNode];

	// Get translation if it exists
	glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.find("translation") != node.end())
	{
		float transValues[3];
		for (unsigned int i = 0; i < node["translation"].size(); i++)
			transValues[i] = (node["translation"][i]);
		translation = glm::make_vec3(transValues);
	}
	// Get quaternion if it exists
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (node.find("rotation") != node.end())
	{
		float rotValues[4] =
		{
			node["rotation"][3],
			node["rotation"][0],
			node["rotation"][1],
			node["rotation"][2]
		};
		rotation = glm::make_quat(rotValues);
	}
	// Get scale if it exists
	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.find("scale") != node.end())
	{
		float scaleValues[3];
		for (unsigned int i = 0; i < node["scale"].size(); i++)
			scaleValues[i] = (node["scale"][i]);
		scale = glm::make_vec3(scaleValues);
	}
	// Get matrix if it exists
	glm::mat4 matNode = glm::mat4(1.0f);
	if (node.find("matrix") != node.end())
	{
		float matValues[16];
		for (unsigned int i = 0; i < node["matrix"].size(); i++)
			matValues[i] = (node["matrix"][i]);
		matNode = glm::make_mat4(matValues);
	}

	// Initialize matrices
	glm::mat4 trans = glm::mat4(1.0f);
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 sca = glm::mat4(1.0f);

	// Use translation, rotation, and scale to change the initialized matrices
	trans = glm::translate(trans, translation);
	rot = glm::mat4_cast(rotation);
	sca = glm::scale(sca, scale);

	// Multiply all matrices together
	glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

	// Check if the node contains a mesh and if it does load it
	if (node.find("mesh") != node.end())
	{
		translationsMeshes.push_back(translation);
		rotationsMeshes.push_back(rotation);
		scalesMeshes.push_back(scale);
		matricesMeshes.push_back(matNextNode);

		loadMesh(node["mesh"]);
	}

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (node.find("children") != node.end())
	{
		for (unsigned int i = 0; i < node["children"].size(); i++)
			traverseNode(node["children"][i], matNextNode);
	}
}

std::vector<unsigned char> Model::getData()
{
	// Create a place to store the raw text, and get the uri of the .bin file
	std::string bytesText;
	std::string uri = JSON["buffers"][0]["uri"];

	// Store raw text data into bytesText
	std::string fileStr = std::string(file);
	std::string fileDirectory = getDirectoryFromPath(fileStr);
	bytesText = get_file_contents((fileDirectory + uri).c_str());

	// Transform the raw text data into bytes and put them in a vector
	std::vector<unsigned char> data(bytesText.begin(), bytesText.end());
	return data;
}

std::vector<float> Model::getFloats(json accessor)
{
	std::vector<float> floatVec;

	// Get properties from the accessor
	unsigned int buffViewInd = accessor.value("bufferView", 1);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	// Get properties from the bufferView
	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];

	// Interpret the type and store it into numPerVert
	unsigned int numPerVert;
	if (type == "SCALAR") numPerVert = 1;
	else if (type == "VEC2") numPerVert = 2;
	else if (type == "VEC3") numPerVert = 3;
	else if (type == "VEC4") numPerVert = 4;
	else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");

	// Go over all the bytes in the data at the correct place using the properties from above
	unsigned int beginningOfData = byteOffset + accByteOffset;
	unsigned int lengthOfData = count * 4 * numPerVert;
	for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i)
	{
		unsigned char bytes[] = { data[i++], data[i++], data[i++], data[i++] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}

	return floatVec;
}

std::vector<GLuint> Model::getIndices(json accessor)
{
	std::vector<GLuint> indices;

	// Get properties from the accessor
	unsigned int buffViewInd = accessor.value("bufferView", 0);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	unsigned int componentType = accessor["componentType"];

	// Get properties from the bufferView
	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];

	// Get indices with regards to their type: unsigned int, unsigned short, or short
	unsigned int beginningOfData = byteOffset + accByteOffset;
	if (componentType == 5125)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
		{
			unsigned char bytes[] = { data[i++], data[i++], data[i++], data[i++] };
			unsigned int value;
			std::memcpy(&value, bytes, sizeof(unsigned int));
			indices.push_back((GLuint)value);
		}
	}
	else if (componentType == 5123)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			unsigned char bytes[] = { data[i++], data[i++] };
			unsigned short value;
			std::memcpy(&value, bytes, sizeof(unsigned short));
			indices.push_back((GLuint)value);
		}
	}
	else if (componentType == 5122)
	{
		for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			unsigned char bytes[] = { data[i++], data[i++] };
			short value;
			std::memcpy(&value, bytes, sizeof(short));
			indices.push_back((GLuint)value);
		}
	}

	return indices;
}

std::vector<Texture> Model::getTextures()
{
	std::vector<Texture> textures;

	std::string fileStr = std::string(file);
	std::string fileDirectory = getDirectoryFromPath(fileStr);

	// Go over all images
	for (unsigned int i = 0; i < JSON["images"].size(); i++)
	{
		// uri of current texture
		std::string texPath = JSON["images"][i]["uri"];

		// Check if the texture has already been loaded
		bool skip = false;
		for (unsigned int j = 0; j < loadedTexName.size(); j++)
		{
			if (loadedTexName[j] == texPath)
			{
				textures.push_back(loadedTex[j]);
				skip = true;
				break;
			}
		}

		// If the texture has been loaded, skip this
		if (!skip)
		{
			// Load diffuse texture
			if (texPath.find("baseColor") != std::string::npos)
			{
				Texture diffuse = Texture((fileDirectory + texPath).c_str(), "diffuse", static_cast<GLuint>(loadedTex.size()));
				textures.push_back(diffuse);
				loadedTex.push_back(diffuse);
				loadedTexName.push_back(texPath);
			}
			// Load specular texture
			else if (texPath.find("metallicRoughness") != std::string::npos)
			{
				Texture specular = Texture((fileDirectory + texPath).c_str(), "specular", static_cast<GLuint>(loadedTex.size()));
				textures.push_back(specular);
				loadedTex.push_back(specular);
				loadedTexName.push_back(texPath);
			}
		}
	}

	return textures;
}

void Model::updateBounds(const glm::vec3& worldPosition)
{
	boundsMin = glm::min(boundsMin, worldPosition);
	boundsMax = glm::max(boundsMax, worldPosition);
	hasBounds = true;
}

void Model::loadAssimpModel(const char* path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		path,
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices |
		aiProcess_OptimizeMeshes |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_ImproveCacheLocality
	);

	if (scene == nullptr || scene->mRootNode == nullptr)
	{
		throw std::runtime_error(std::string("No se pudo cargar el modelo con Assimp: ") + importer.GetErrorString());
	}

	processAssimpNode(
		scene->mRootNode,
		scene,
		glm::mat4(1.0f),
		getDirectoryFromPath(path)
	);
	finalizeAssimpBatches();
}

void Model::loadAssimpCollision(const char* path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		path,
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices
	);

	if (scene == nullptr)
	{
		return;
	}

	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* mesh = scene->mMeshes[meshIndex];
		if (mesh == nullptr || mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
		{
			continue;
		}

		CollisionMesh collisionMesh;
		collisionMesh.vertices.reserve(mesh->mNumVertices);
		for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			const aiVector3D& vertex = mesh->mVertices[vertexIndex];
			const glm::vec3 position(vertex.x, vertex.y, vertex.z);
			collisionMesh.vertices.push_back(position);
			collisionMesh.boundsMin = glm::min(collisionMesh.boundsMin, position);
			collisionMesh.boundsMax = glm::max(collisionMesh.boundsMax, position);
		}

		collisionMesh.indices.reserve(mesh->mNumFaces * 3);
		for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			const aiFace& face = mesh->mFaces[faceIndex];
			if (face.mNumIndices != 3)
			{
				continue;
			}

			collisionMesh.indices.push_back(face.mIndices[0]);
			collisionMesh.indices.push_back(face.mIndices[1]);
			collisionMesh.indices.push_back(face.mIndices[2]);
		}

		const glm::vec3 extents = collisionMesh.boundsMax - collisionMesh.boundsMin;
		if (extents.x < 0.01f || extents.y < 0.01f || extents.z < 0.01f)
		{
			continue;
		}

		if (collisionMesh.indices.empty())
		{
			continue;
		}

		collisionMeshes.push_back(std::move(collisionMesh));
	}
}

void Model::processAssimpNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform, const std::string& fileDirectory)
{
	const glm::mat4 nodeTransform = parentTransform * aiToGlmMatrix(node->mTransformation);

	for (unsigned int i = 0; i < node->mNumMeshes; ++i)
	{
		processAssimpMesh(scene->mMeshes[node->mMeshes[i]], scene, nodeTransform, fileDirectory);
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		processAssimpNode(node->mChildren[i], scene, nodeTransform, fileDirectory);
	}
}

void Model::processAssimpMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform, const std::string& fileDirectory)
{
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
	std::vector<Texture> textures;
	glm::vec3 materialBaseColor(1.0f, 1.0f, 1.0f);
	bool hasMaterialBaseColor = false;
	glm::vec3 materialEmissiveColor(0.0f, 0.0f, 0.0f);
	bool hasMaterialEmissiveColor = false;

	if (mesh->mMaterialIndex < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString materialName;
		material->Get(AI_MATKEY_NAME, materialName);
		const std::string materialNameLower = toLowerCopy(materialName.C_Str());
		if (lightweightVehicleAsset && isHeavyVehicleWheelMaterial(materialNameLower))
		{
			return;
		}
		const bool isThreadMaterial = materialNameLower.find("thread") != std::string::npos;
		const bool isTireMaterial = isThreadMaterial || materialNameLower.find("tire") != std::string::npos;
		aiColor4D baseColor;
		if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &baseColor) == aiReturn_SUCCESS)
		{
			materialBaseColor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
			hasMaterialBaseColor = true;
		}
		else if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &baseColor) == aiReturn_SUCCESS)
		{
			materialBaseColor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
			hasMaterialBaseColor = true;
		}

		aiColor4D emissiveColor;
		if (aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissiveColor) == aiReturn_SUCCESS)
		{
			materialEmissiveColor = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
			hasMaterialEmissiveColor = glm::length(materialEmissiveColor) > 0.0001f;
		}

		if ((!hasMaterialBaseColor || glm::length(materialBaseColor) < 0.0001f) && hasMaterialEmissiveColor)
		{
			materialBaseColor = materialEmissiveColor;
			hasMaterialBaseColor = true;
		}

		const auto loadMaterialTextures = [&](aiTextureType textureType, const char* textureLabel)
		{
			for (unsigned int i = 0; i < material->GetTextureCount(textureType); ++i)
			{
				aiString texturePath;
				if (material->GetTexture(textureType, i, &texturePath) != aiReturn_SUCCESS)
				{
					continue;
				}

				const std::string textureKey = texturePath.C_Str();
				bool alreadyLoaded = false;
				for (unsigned int loadedIndex = 0; loadedIndex < loadedTexName.size(); ++loadedIndex)
				{
					if (loadedTexName[loadedIndex] == textureKey)
					{
						textures.push_back(loadedTex[loadedIndex]);
						alreadyLoaded = true;
						break;
					}
				}

				if (alreadyLoaded)
				{
					continue;
				}

				Texture texture = [&]() -> Texture
				{
					if (!textureKey.empty() && textureKey[0] == '*')
					{
						const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str());
						return buildEmbeddedTexture(embeddedTexture, textureLabel, static_cast<GLuint>(loadedTex.size()));
					}

					return Texture((fileDirectory + textureKey).c_str(), textureLabel, static_cast<GLuint>(loadedTex.size()));
				}();

				textures.push_back(texture);
				loadedTex.push_back(texture);
				loadedTexName.push_back(textureKey);
			}
		};

		loadMaterialTextures(aiTextureType_DIFFUSE, "diffuse");
		loadMaterialTextures(aiTextureType_BASE_COLOR, "diffuse");
		loadMaterialTextures(aiTextureType_SPECULAR, "specular");

		if (isThreadMaterial)
		{
			materialBaseColor = glm::vec3(0.08f, 0.08f, 0.08f);
			hasMaterialBaseColor = true;
		}
		else if (isTireMaterial)
		{
			materialBaseColor = glm::vec3(0.16f, 0.16f, 0.16f);
			hasMaterialBaseColor = true;
		}
	}

	bool hasDiffuseTexture = false;
	for (const Texture& texture : textures)
	{
		if (std::string(texture.type) == "diffuse")
		{
			hasDiffuseTexture = true;
			break;
		}
	}

	vertices.reserve(mesh->mNumVertices);
	for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
	{
		Vertex vertex{};
		vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		vertex.normal = mesh->HasNormals()
			? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
			: glm::vec3(0.0f, 1.0f, 0.0f);
		if (mesh->HasVertexColors(0))
		{
			vertex.color = glm::vec3(
				mesh->mColors[0][i].r,
				mesh->mColors[0][i].g,
				mesh->mColors[0][i].b
			);
			if (glm::length(vertex.color) < 0.03f)
			{
				vertex.color = hasMaterialBaseColor ? materialBaseColor : glm::vec3(1.0f);
			}
		}
		else if (hasDiffuseTexture && !hasMaterialBaseColor)
		{
			vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
		}
		else if (hasMaterialBaseColor)
		{
			vertex.color = materialBaseColor;
		}
		else
		{
			vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
		}
		vertex.texUV = mesh->HasTextureCoords(0)
			? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
			: glm::vec2(0.0f, 0.0f);
		vertices.push_back(vertex);
		const glm::vec3 worldPosition = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
		updateBounds(worldPosition);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	if (textures.empty())
	{
		const glm::vec3 diffuseColor = hasMaterialBaseColor ? materialBaseColor : glm::vec3(1.0f, 1.0f, 1.0f);
		textures.push_back(buildSolidTexture(diffuseColor, "diffuse", static_cast<GLuint>(loadedTex.size())));
		loadedTex.push_back(textures.back());
		loadedTexName.push_back("generated_diffuse_" + std::to_string(mesh->mMaterialIndex));
	}

	bool hasSpecularTexture = false;
	for (const Texture& texture : textures)
	{
		if (std::string(texture.type) == "specular")
		{
			hasSpecularTexture = true;
			break;
		}
	}
	if (!hasSpecularTexture)
	{
		textures.push_back(buildSolidTexture(glm::vec3(0.0f, 0.0f, 0.0f), "specular", static_cast<GLuint>(loadedTex.size())));
		loadedTex.push_back(textures.back());
		loadedTexName.push_back("generated_specular_" + std::to_string(mesh->mMaterialIndex));
	}

	const unsigned int batchKey = mesh->mMaterialIndex;
	const auto batchLookup = assimpBatchLookup.find(batchKey);
	if (batchLookup == assimpBatchLookup.end())
	{
		AssimpMeshBatch batch;
		batch.textures = textures;
		assimpBatchLookup.emplace(batchKey, assimpBatches.size());
		assimpBatches.push_back(std::move(batch));
	}

	AssimpMeshBatch& batch = assimpBatches[assimpBatchLookup[batchKey]];
	const GLuint baseVertex = static_cast<GLuint>(batch.vertices.size());
	batch.vertices.insert(batch.vertices.end(), vertices.begin(), vertices.end());
	for (GLuint index : indices)
	{
		batch.indices.push_back(baseVertex + index);
	}
	for (const Vertex& vertex : vertices)
	{
		batch.boundsMin = glm::min(batch.boundsMin, vertex.position);
		batch.boundsMax = glm::max(batch.boundsMax, vertex.position);
	}

}

void Model::finalizeAssimpBatches()
{
	for (AssimpMeshBatch& batch : assimpBatches)
	{
		if (batch.vertices.empty() || batch.indices.empty())
		{
			continue;
		}

		const glm::vec3 meshCenter = (batch.boundsMin + batch.boundsMax) * 0.5f;
		float meshRadius = 0.0f;
		for (const Vertex& vertex : batch.vertices)
		{
			meshRadius = std::max(meshRadius, glm::distance(vertex.position, meshCenter));
		}

		meshBoundsCenters.push_back(meshCenter);
		meshBoundsRadii.push_back(meshRadius);
		matricesMeshes.push_back(glm::mat4(1.0f));
		translationsMeshes.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
		rotationsMeshes.push_back(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		scalesMeshes.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
		meshes.push_back(Mesh(batch.vertices, batch.indices, batch.textures));
	}

	assimpBatchLookup.clear();
	assimpBatches.clear();
}

std::vector<Vertex> Model::assembleVertices
(
	std::vector<glm::vec3> positions,
	std::vector<glm::vec3> normals,
	std::vector<glm::vec2> texUVs
)
{
	std::vector<Vertex> vertices;
	for (int i = 0; i < positions.size(); i++)
	{
		vertices.push_back
		(
			Vertex
			{
				positions[i],
				normals[i],
				glm::vec3(1.0f, 1.0f, 1.0f),
				texUVs[i]
			}
		);
	}
	return vertices;
}

std::vector<glm::vec2> Model::groupFloatsVec2(std::vector<float> floatVec)
{
	std::vector<glm::vec2> vectors;
	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.push_back(glm::vec2(floatVec[i++], floatVec[i++]));
	}
	return vectors;
}

std::vector<glm::vec3> Model::groupFloatsVec3(std::vector<float> floatVec)
{
	std::vector<glm::vec3> vectors;
	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.push_back(glm::vec3(floatVec[i++], floatVec[i++], floatVec[i++]));
	}
	return vectors;
}

std::vector<glm::vec4> Model::groupFloatsVec4(std::vector<float> floatVec)
{
	std::vector<glm::vec4> vectors;
	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.push_back(glm::vec4(floatVec[i++], floatVec[i++], floatVec[i++], floatVec[i++]));
	}
	return vectors;
}
