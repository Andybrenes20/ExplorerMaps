#include "Skybox.h"

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

#include "CubemapTextureLoader.h"

namespace
{
	constexpr float skyboxVertices[] =
	{
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	GLuint CreateProceduralCubemap(bool nightTheme)
	{
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

		constexpr int faceSize = 512;
		for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
		{
			const std::vector<unsigned char> pixels = Skybox::BuildProceduralFace(faceSize, faceSize, faceIndex, nightTheme);
			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
				0,
				GL_RGB,
				faceSize,
				faceSize,
				0,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				pixels.data()
			);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		return texture;
	}

	GLuint LoadCubemapOrFallback(const std::vector<std::string>& facePaths, bool nightTheme)
	{
		try
		{
			GLuint texture = LoadCubemapTexture(facePaths);
			if (texture != 0)
				return texture;
		}
		catch (...)
		{
		}

		return CreateProceduralCubemap(nightTheme);
	}
}

Skybox::Skybox(
	const std::vector<std::string>& dayFacePaths,
	const std::vector<std::string>& nightFacePaths,
	const char* vertexShaderPath,
	const char* fragmentShaderPath
)
	: shader(vertexShaderPath, fragmentShaderPath)
{
	setupCube();
	dayCubemapTexture = LoadCubemapOrFallback(dayFacePaths, false);
	nightCubemapTexture = LoadCubemapOrFallback(nightFacePaths, true);
}

Skybox::~Skybox()
{
	if (dayCubemapTexture != 0)
	{
		glDeleteTextures(1, &dayCubemapTexture);
	}
	if (nightCubemapTexture != 0)
	{
		glDeleteTextures(1, &nightCubemapTexture);
	}
	if (VBO != 0)
	{
		glDeleteBuffers(1, &VBO);
	}
	if (VAO != 0)
	{
		glDeleteVertexArrays(1, &VAO);
	}
	shader.Delete();
}


void Skybox::SetBlendFactor(float factor)
{
	blendFactor = glm::clamp(factor, 0.0f, 1.0f);
}

void Skybox::SetCloudSettings(const SkyCloudSettings& settings)
{
	cloudSettings = settings;
}

void Skybox::SetProceduralCloudsEnabled(bool enabled)
{
	proceduralCloudsEnabled = enabled;
}

void Skybox::Draw(const Camera& camera, float FOVdeg, float nearPlane, float farPlane, float time, float sunHeight, const glm::vec3& sunDirection, const glm::vec3& moonPosition)
{
	glDepthFunc(GL_LEQUAL);

	shader.Activate();

	const glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
	const glm::mat4 projection = camera.GetProjectionMatrix(FOVdeg, nearPlane, farPlane);

	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1i(glGetUniformLocation(shader.ID, "daySkybox"), 0);
	glUniform1i(glGetUniformLocation(shader.ID, "nightSkybox"), 1);
	glUniform1f(glGetUniformLocation(shader.ID, "blendFactor"), blendFactor);
	glUniform1f(glGetUniformLocation(shader.ID, "time"), time);
	glUniform1f(glGetUniformLocation(shader.ID, "sunHeight"), sunHeight);
	glUniform3f(glGetUniformLocation(shader.ID, "sunDir"), sunDirection.x, sunDirection.y, sunDirection.z);
	glm::vec3 moonDirection = moonPosition - camera.Position;
	if (glm::length(moonDirection) < 0.001f)
	{
		moonDirection = -sunDirection;
	}
	else
	{
		moonDirection = glm::normalize(moonDirection);
	}
	glUniform3f(glGetUniformLocation(shader.ID, "moonDir"), moonDirection.x, moonDirection.y, moonDirection.z);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudCoverage"), cloudSettings.coverage);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudSpeed"), cloudSettings.speed);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudCrispiness"), cloudSettings.crispiness);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudCurliness"), cloudSettings.curliness);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudDensity"), cloudSettings.density);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudLightAbsorption"), cloudSettings.lightAbsorption);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudDomeRadius"), cloudSettings.skyDomeRadius);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudBottom"), cloudSettings.cloudBottom);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudTop"), cloudSettings.cloudTop);
	glUniform1f(glGetUniformLocation(shader.ID, "cloudFrequency"), cloudSettings.cloudFrequency);
	glUniform3f(glGetUniformLocation(shader.ID, "cloudColor"), cloudSettings.cloudColor.r, cloudSettings.cloudColor.g, cloudSettings.cloudColor.b);
	glUniform1f(glGetUniformLocation(shader.ID, "rainIntensity"), cloudSettings.rainIntensity);
	glUniform1f(glGetUniformLocation(shader.ID, "lightningIntensity"), cloudSettings.lightningIntensity);
	glUniform1f(glGetUniformLocation(shader.ID, "lightningSeed"), cloudSettings.lightningSeed);
	glUniform3f(glGetUniformLocation(shader.ID, "cameraPosition"), camera.Position.x, camera.Position.y, camera.Position.z);
	glUniform2f(glGetUniformLocation(shader.ID, "resolution"), static_cast<float>(camera.width), static_cast<float>(camera.height));
	glUniform1i(glGetUniformLocation(shader.ID, "useProceduralClouds"), proceduralCloudsEnabled ? 1 : 0);

	glBindVertexArray(VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, dayCubemapTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, nightCubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glDepthFunc(GL_LESS);
}

void Skybox::setupCube()
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::vector<unsigned char> Skybox::BuildProceduralFace(int width, int height, int faceIndex, bool nightTheme)
{
	std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);

	const glm::vec3 zenithColor = nightTheme ? glm::vec3(0.03f, 0.05f, 0.12f) : glm::vec3(0.25f, 0.52f, 0.92f);
	const glm::vec3 horizonColor = nightTheme ? glm::vec3(0.12f, 0.16f, 0.26f) : glm::vec3(0.83f, 0.92f, 1.0f);
	const glm::vec3 groundColor = nightTheme ? glm::vec3(0.04f, 0.05f, 0.08f) : glm::vec3(0.58f, 0.67f, 0.83f);
	const glm::vec3 sunDirection = glm::normalize(glm::vec3(0.35f, 0.42f, -0.65f));

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
			const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
			const glm::vec3 direction = DirectionForFace(faceIndex, u, v);

			const float skyBlend = glm::clamp(direction.y * 0.5f + 0.5f, 0.0f, 1.0f);
			glm::vec3 color = direction.y >= 0.0f
				? glm::mix(horizonColor, zenithColor, std::pow(skyBlend, 0.7f))
				: glm::mix(horizonColor, groundColor, std::pow(-direction.y, 0.55f));

			const float sunAmount = std::pow(glm::clamp(glm::dot(direction, sunDirection), 0.0f, 1.0f), nightTheme ? 180.0f : 96.0f);
			color += (nightTheme ? glm::vec3(0.72f, 0.78f, 0.98f) : glm::vec3(1.0f, 0.88f, 0.62f)) * sunAmount * (nightTheme ? 0.35f : 1.35f);

			if (std::abs(direction.y) < 0.012f)
			{
				color = glm::mix(color, nightTheme ? glm::vec3(0.20f, 0.24f, 0.34f) : glm::vec3(0.78f, 0.87f, 0.98f), 0.45f);
			}

			color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
			const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 3;
			pixels[index + 0] = static_cast<unsigned char>(color.r * 255.0f);
			pixels[index + 1] = static_cast<unsigned char>(color.g * 255.0f);
			pixels[index + 2] = static_cast<unsigned char>(color.b * 255.0f);
		}
	}

	return pixels;
}

glm::vec3 Skybox::DirectionForFace(int faceIndex, float u, float v)
{
	const float x = 2.0f * u - 1.0f;
	const float y = 1.0f - 2.0f * v;

	switch (faceIndex)
	{
	case 0:
		return glm::normalize(glm::vec3(1.0f, y, -x));
	case 1:
		return glm::normalize(glm::vec3(-1.0f, y, x));
	case 2:
		return glm::normalize(glm::vec3(x, 1.0f, -y));
	case 3:
		return glm::normalize(glm::vec3(x, -1.0f, y));
	case 4:
		return glm::normalize(glm::vec3(x, y, 1.0f));
	case 5:
	default:
		return glm::normalize(glm::vec3(-x, y, -1.0f));
	}
}
