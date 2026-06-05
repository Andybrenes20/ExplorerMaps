#include "VolumetricCloudRenderer.h"

#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif

#ifndef GL_TEXTURE_FETCH_BARRIER_BIT
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#endif

namespace
{
	using DispatchComputeProc = void(APIENTRY*)(GLuint, GLuint, GLuint);
	using BindImageTextureProc = void(APIENTRY*)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
	using MemoryBarrierProc = void(APIENTRY*)(GLbitfield);

	DispatchComputeProc DispatchCompute = nullptr;
	BindImageTextureProc BindImageTexture = nullptr;
	MemoryBarrierProc MemoryBarrier = nullptr;

	int DivCeil(int value, int divisor)
	{
		return (value + divisor - 1) / divisor;
	}


	bool CheckFramebufferComplete(const char* name)
	{
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status == GL_FRAMEBUFFER_COMPLETE)
			return true;

		std::cerr << "Framebuffer incompleto (" << name << "): 0x" << std::hex << status << std::dec << std::endl;
		return false;
	}

	void CheckComputeShader(GLuint shader, const char* path)
	{
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success)
			return;

		GLchar infoLog[2048] = {};
		glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Error compilando compute shader " << path << ":\n" << infoLog << std::endl;
	}

	void CheckComputeProgram(GLuint program, const char* path)
	{
		GLint success = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (success)
			return;

		GLchar infoLog[2048] = {};
		glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Error enlazando compute shader " << path << ":\n" << infoLog << std::endl;
	}

	GLuint CreateColorTexture2D(int width, int height, GLint internalFormat, GLenum format, GLenum type)
	{
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		return texture;
	}

	GLuint CreateDepthTexture2D(int width, int height)
	{
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		return texture;
	}

	GLuint CreateNoiseTexture3D(int size)
	{
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_3D, texture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, size, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
		glBindTexture(GL_TEXTURE_3D, 0);
		return texture;
	}
}

VolumetricCloudRenderer::ComputeProgram::ComputeProgram(const char* computeShaderPath)
{
	Load(computeShaderPath);
}

VolumetricCloudRenderer::ComputeProgram::~ComputeProgram()
{
	Reset();
}

void VolumetricCloudRenderer::ComputeProgram::Reset()
{
	if (id != 0)
	{
		glDeleteProgram(id);
		id = 0;
	}
	uniformLocationCache.clear();
}

bool VolumetricCloudRenderer::ComputeProgram::Load(const char* computeShaderPath)
{
	Reset();

	std::string computeCode;
	try
	{
		computeCode = get_file_contents(computeShaderPath);
	}
	catch (const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return false;
	}

	const char* computeSource = computeCode.c_str();
	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &computeSource, nullptr);
	glCompileShader(shader);
	CheckComputeShader(shader, computeShaderPath);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		glDeleteShader(shader);
		return false;
	}

	id = glCreateProgram();
	glAttachShader(id, shader);
	glLinkProgram(id);
	CheckComputeProgram(id, computeShaderPath);
	glDeleteShader(shader);

	GLint linked = 0;
	glGetProgramiv(id, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		glDeleteProgram(id);
		id = 0;
		return false;
	}

	return true;
}

bool VolumetricCloudRenderer::ComputeProgram::IsValid() const
{
	return id != 0;
}

void VolumetricCloudRenderer::ComputeProgram::Use() const
{
	glUseProgram(id);
}

GLint VolumetricCloudRenderer::ComputeProgram::GetUniformLocation(const char* uniformName)
{
	const auto cached = uniformLocationCache.find(uniformName);
	if (cached != uniformLocationCache.end())
		return cached->second;

	const GLint location = glGetUniformLocation(id, uniformName);
	uniformLocationCache.emplace(uniformName, location);
	return location;
}

GLuint VolumetricCloudRenderer::ComputeProgram::GetId() const
{
	return id;
}

VolumetricCloudRenderer::VolumetricCloudRenderer(int screenWidth, int screenHeight)
	: screenWidth(screenWidth),
	screenHeight(screenHeight),
	cloudPostShader("Shaders/screen.vert", "Shaders/clouds_post.frag"),
	compositeShader("Shaders/screen.vert", "Shaders/post_processing.frag"),
	terrainSkyShader("Shaders/screen.vert", "Shaders/volumetric_sky.frag")
{
	if (!LoadComputeFunctions())
		return;

	if (!perlinWorleyProgram.Load("Shaders/perlinworley.comp") ||
		!worleyProgram.Load("Shaders/worley.comp") ||
		!weatherProgram.Load("Shaders/weather.comp") ||
		!volumetricProgram.Load("Shaders/volumetric_clouds.comp"))
	{
		statusMessage = "No se pudieron compilar los compute shaders de nubes.";
		return;
	}

	if (!CreateFramebuffers() || !CreateCloudTextures() || !CreateNoiseTextures() || !CreateFullscreenQuad())
		return;

	GenerateModelTextures();
	ready = true;
	statusMessage = "Nubes volumetricas activas.";
}

VolumetricCloudRenderer::~VolumetricCloudRenderer()
{
	Shutdown();
}

void VolumetricCloudRenderer::Shutdown()
{
	ReleaseFramebuffer(sceneFramebuffer);
	ReleaseFramebuffer(skyFramebuffer);
	ReleaseFramebuffer(cloudPostFramebuffer);

	for (GLuint& texture : cloudTextures)
	{
		if (texture != 0)
			glDeleteTextures(1, &texture);
		texture = 0;
	}

	if (perlinWorleyTexture != 0)
		glDeleteTextures(1, &perlinWorleyTexture);
	if (worleyTexture != 0)
		glDeleteTextures(1, &worleyTexture);
	if (weatherTexture != 0)
		glDeleteTextures(1, &weatherTexture);
	perlinWorleyTexture = 0;
	worleyTexture = 0;
	weatherTexture = 0;
	if (quadVBO != 0)
		glDeleteBuffers(1, &quadVBO);
	if (quadVAO != 0)
		glDeleteVertexArrays(1, &quadVAO);
	quadVBO = 0;
	quadVAO = 0;

	if (cloudPostShader.ID != 0)
	{
		cloudPostShader.Delete();
		cloudPostShader.ID = 0;
	}
	if (compositeShader.ID != 0)
	{
		compositeShader.Delete();
		compositeShader.ID = 0;
	}
	if (terrainSkyShader.ID != 0)
	{
		terrainSkyShader.Delete();
		terrainSkyShader.ID = 0;
	}

	perlinWorleyProgram.Reset();
	worleyProgram.Reset();
	weatherProgram.Reset();
	volumetricProgram.Reset();

	ready = false;
}

bool VolumetricCloudRenderer::IsReady() const
{
	return ready;
}

const std::string& VolumetricCloudRenderer::GetStatusMessage() const
{
	return statusMessage;
}

bool VolumetricCloudRenderer::LoadComputeFunctions()
{
	DispatchCompute = reinterpret_cast<DispatchComputeProc>(glfwGetProcAddress("glDispatchCompute"));
	BindImageTexture = reinterpret_cast<BindImageTextureProc>(glfwGetProcAddress("glBindImageTexture"));
	MemoryBarrier = reinterpret_cast<MemoryBarrierProc>(glfwGetProcAddress("glMemoryBarrier"));

	if (!DispatchCompute || !BindImageTexture || !MemoryBarrier)
	{
		statusMessage = "OpenGL 4.3/compute shaders no estan disponibles en este contexto.";
		std::cerr << statusMessage << std::endl;
		return false;
	}

	return true;
}

bool VolumetricCloudRenderer::CreateFramebuffers()
{
	auto createFramebuffer = [](Framebuffer& framebuffer, int width, int height, bool withDepth, const char* name) -> bool
		{
			framebuffer.width = width;
			framebuffer.height = height;
			framebuffer.color = CreateColorTexture2D(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
			if (withDepth)
				framebuffer.depth = CreateDepthTexture2D(width, height);

			glGenFramebuffers(1, &framebuffer.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color, 0);
			if (withDepth)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depth, 0);

			const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
			glDrawBuffers(1, &drawBuffer);
			const bool complete = CheckFramebufferComplete(name);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return complete;
		};

	auto createPostFramebuffer = [](Framebuffer& framebuffer, int width, int height, const char* name) -> bool
		{
			framebuffer.width = width;
			framebuffer.height = height;
			framebuffer.color = CreateColorTexture2D(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
			glGenFramebuffers(1, &framebuffer.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color, 0);

			const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
			glDrawBuffers(1, &drawBuffer);
			const bool complete = CheckFramebufferComplete(name);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return complete;
		};

	if (!createFramebuffer(sceneFramebuffer, screenWidth, screenHeight, true, "cloud scene"))
	{
		statusMessage = "No se pudo crear el framebuffer de escena para nubes.";
		return false;
	}
	if (!createFramebuffer(skyFramebuffer, screenWidth, screenHeight, false, "cloud sky"))
	{
		statusMessage = "No se pudo crear el framebuffer de cielo para nubes.";
		return false;
	}
	if (!createPostFramebuffer(cloudPostFramebuffer, screenWidth, screenHeight, "cloud post"))
	{
		statusMessage = "No se pudo crear el framebuffer de postproceso de nubes.";
		return false;
	}

	return true;
}

bool VolumetricCloudRenderer::CreateCloudTextures()
{
	for (GLuint& texture : cloudTextures)
	{
		texture = CreateColorTexture2D(screenWidth, screenHeight, GL_RGBA32F, GL_RGBA, GL_FLOAT);
		if (texture == 0)
		{
			statusMessage = "No se pudieron crear las texturas de salida de nubes.";
			return false;
		}
	}

	return true;
}

bool VolumetricCloudRenderer::CreateNoiseTextures()
{
	perlinWorleyTexture = CreateNoiseTexture3D(128);
	worleyTexture = CreateNoiseTexture3D(32);
	weatherTexture = CreateColorTexture2D(1024, 1024, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	glBindTexture(GL_TEXTURE_2D, weatherTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (perlinWorleyTexture == 0 || worleyTexture == 0 || weatherTexture == 0)
	{
		statusMessage = "No se pudieron crear las texturas de ruido de nubes.";
		return false;
	}

	return true;
}

bool VolumetricCloudRenderer::CreateFullscreenQuad()
{
	const float vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f
	};

	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return quadVAO != 0 && quadVBO != 0;
}

void VolumetricCloudRenderer::GenerateModelTextures()
{
	perlinWorleyProgram.Use();
	BindImageTexture(0, perlinWorleyTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	DispatchCompute(DivCeil(128, 4), DivCeil(128, 4), DivCeil(128, 4));
	MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glBindTexture(GL_TEXTURE_3D, perlinWorleyTexture);
	glGenerateMipmap(GL_TEXTURE_3D);

	worleyProgram.Use();
	BindImageTexture(0, worleyTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	DispatchCompute(DivCeil(32, 4), DivCeil(32, 4), DivCeil(32, 4));
	MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glBindTexture(GL_TEXTURE_3D, worleyTexture);
	glGenerateMipmap(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D, 0);

	GenerateWeatherMap(weatherSeed, 0.8f);
}

void VolumetricCloudRenderer::GenerateWeatherMap(const glm::vec3& seed, float perlinFrequency)
{
	weatherSeed = seed;
	weatherFrequency = perlinFrequency;
	weatherProgram.Use();
	glUniform3fv(weatherProgram.GetUniformLocation("seed"), 1, glm::value_ptr(seed));
	glUniform1f(weatherProgram.GetUniformLocation("perlinFrequency"), perlinFrequency);
	BindImageTexture(0, weatherTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
	DispatchCompute(DivCeil(1024, 16), DivCeil(1024, 16), 1);
	MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void VolumetricCloudRenderer::ReleaseFramebuffer(Framebuffer& framebuffer)
{
	if (framebuffer.fbo != 0)
		glDeleteFramebuffers(1, &framebuffer.fbo);
	if (framebuffer.color != 0)
		glDeleteTextures(1, &framebuffer.color);
	if (framebuffer.depth != 0)
		glDeleteTextures(1, &framebuffer.depth);
	framebuffer = Framebuffer();
}

void VolumetricCloudRenderer::BindSceneFramebuffer(const glm::vec3& clearColor)
{
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer.fbo);
	glViewport(0, 0, sceneFramebuffer.width, sceneFramebuffer.height);
	glEnable(GL_DEPTH_TEST);
	glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void VolumetricCloudRenderer::BindSkyFramebuffer(const glm::vec3& clearColor)
{
	glBindFramebuffer(GL_FRAMEBUFFER, skyFramebuffer.fbo);
	glViewport(0, 0, skyFramebuffer.width, skyFramebuffer.height);
	glDisable(GL_DEPTH_TEST);
	glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void VolumetricCloudRenderer::RenderTerrainSky(
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& lightDirection,
	const glm::vec3& skyColorTop,
	const glm::vec3& skyColorBottom,
	float time,
	float sunHeight,
	float rainIntensity)
{
	glBindFramebuffer(GL_FRAMEBUFFER, skyFramebuffer.fbo);
	glViewport(0, 0, skyFramebuffer.width, skyFramebuffer.height);
	glDisable(GL_DEPTH_TEST);

	terrainSkyShader.Activate();
	glUniform2f(glGetUniformLocation(terrainSkyShader.ID, "resolution"), static_cast<float>(screenWidth), static_cast<float>(screenHeight));
	glUniformMatrix4fv(glGetUniformLocation(terrainSkyShader.ID, "inv_proj"), 1, GL_FALSE, glm::value_ptr(glm::inverse(projection)));
	glUniformMatrix4fv(glGetUniformLocation(terrainSkyShader.ID, "inv_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
	glUniform3fv(glGetUniformLocation(terrainSkyShader.ID, "lightDirection"), 1, glm::value_ptr(glm::normalize(lightDirection)));
	glUniform3fv(glGetUniformLocation(terrainSkyShader.ID, "skyColorTop"), 1, glm::value_ptr(skyColorTop));
	glUniform3fv(glGetUniformLocation(terrainSkyShader.ID, "skyColorBottom"), 1, glm::value_ptr(skyColorBottom));
	glUniform1f(glGetUniformLocation(terrainSkyShader.ID, "time"), time);
	glUniform1f(glGetUniformLocation(terrainSkyShader.ID, "sunHeight"), sunHeight);
	glUniform1f(glGetUniformLocation(terrainSkyShader.ID, "rainIntensity"), rainIntensity);
	DrawFullscreenQuad();
}

void VolumetricCloudRenderer::BindTexture2D(GLuint texture, GLuint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, texture);
}

void VolumetricCloudRenderer::BindTexture3D(GLuint texture, GLuint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_3D, texture);
}

void VolumetricCloudRenderer::RenderClouds(
	const Camera& camera,
	const glm::mat4& view,
	const glm::mat4& projection,
	const SkyCloudSettings& settings,
	const glm::vec3& lightDirection,
	const glm::vec3& lightColor,
	const glm::vec3& skyColorTop,
	const glm::vec3& skyColorBottom,
	float time)
{
	if (!ready)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenWidth, screenHeight);

	const float requestedWeatherFrequency = glm::clamp(settings.cloudFrequency, 0.0f, 4.0f);
	if (std::abs(requestedWeatherFrequency - weatherFrequency) > 0.001f)
		GenerateWeatherMap(weatherSeed, requestedWeatherFrequency);

	for (int i = 0; i < CloudTextureCount; ++i)
	{
		BindImageTexture(static_cast<GLuint>(i), cloudTextures[i], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

	BindTexture3D(perlinWorleyTexture, 0);
	BindTexture3D(worleyTexture, 1);
	BindTexture2D(weatherTexture, 2);
	BindTexture2D(sceneFramebuffer.depth, 3);
	BindTexture2D(skyFramebuffer.color, 4);

	volumetricProgram.Use();
	glUniform2f(volumetricProgram.GetUniformLocation("iResolution"), static_cast<float>(screenWidth), static_cast<float>(screenHeight));
	glUniform1f(volumetricProgram.GetUniformLocation("iTime"), time);
	glUniformMatrix4fv(volumetricProgram.GetUniformLocation("inv_proj"), 1, GL_FALSE, glm::value_ptr(glm::inverse(projection)));
	glUniformMatrix4fv(volumetricProgram.GetUniformLocation("inv_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
	glUniformMatrix4fv(volumetricProgram.GetUniformLocation("invViewProj"), 1, GL_FALSE, glm::value_ptr(glm::inverse(projection * view)));
	glUniform3fv(volumetricProgram.GetUniformLocation("cameraPosition"), 1, glm::value_ptr(camera.Position));
	glUniform1f(volumetricProgram.GetUniformLocation("FOV"), 55.0f);
	glUniform3fv(volumetricProgram.GetUniformLocation("lightDirection"), 1, glm::value_ptr(glm::normalize(lightDirection)));
	glUniform3fv(volumetricProgram.GetUniformLocation("lightColor"), 1, glm::value_ptr(lightColor));

	glUniform1f(volumetricProgram.GetUniformLocation("coverage_multiplier"), glm::clamp(settings.coverage, 0.0f, 1.0f));
	glUniform1f(volumetricProgram.GetUniformLocation("cloudSpeed"), glm::max(settings.speed * 450.0f, 0.0f));
	glUniform1f(volumetricProgram.GetUniformLocation("crispiness"), glm::max(settings.crispiness * 40.0f, 0.05f));
	glUniform1f(volumetricProgram.GetUniformLocation("curliness"), settings.curliness);
	glUniform1f(volumetricProgram.GetUniformLocation("absorption"), settings.lightAbsorption * 0.01f);
	glUniform1f(volumetricProgram.GetUniformLocation("densityFactor"), settings.density * 0.020f);
	glUniform1f(volumetricProgram.GetUniformLocation("earthRadius"), glm::max(settings.skyDomeRadius, 10000.0f));
	glUniform1f(volumetricProgram.GetUniformLocation("sphereInnerRadius"), glm::max(settings.cloudBottom, 1000.0f));
	glUniform1f(volumetricProgram.GetUniformLocation("sphereOuterRadius"), glm::max(settings.cloudTop, 1000.0f));
	glUniform3f(volumetricProgram.GetUniformLocation("cloudColorTop"), 169.0f * 1.5f / 255.0f, 149.0f * 1.5f / 255.0f, 149.0f * 1.5f / 255.0f);
	const glm::vec3 cloudColorBottom = glm::vec3(65.0f, 70.0f, 80.0f) * (1.5f / 255.0f);
	glUniform3fv(volumetricProgram.GetUniformLocation("cloudColorBottom"), 1, glm::value_ptr(cloudColorBottom));
	glUniform3fv(volumetricProgram.GetUniformLocation("skyColorTop"), 1, glm::value_ptr(skyColorTop));
	glUniform3fv(volumetricProgram.GetUniformLocation("skyColorBottom"), 1, glm::value_ptr(skyColorBottom));
	glUniform1i(volumetricProgram.GetUniformLocation("enablePowder"), settings.sugarPowder ? 1 : 0);
	glUniform1i(volumetricProgram.GetUniformLocation("cloud"), 0);
	glUniform1i(volumetricProgram.GetUniformLocation("worley32"), 1);
	glUniform1i(volumetricProgram.GetUniformLocation("weatherTex"), 2);
	glUniform1i(volumetricProgram.GetUniformLocation("depthMap"), 3);
	glUniform1i(volumetricProgram.GetUniformLocation("sky"), 4);

	DispatchCompute(DivCeil(screenWidth, 16), DivCeil(screenHeight, 16), 1);
	MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

	if (settings.postProcessing)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, cloudPostFramebuffer.fbo);
		glViewport(0, 0, cloudPostFramebuffer.width, cloudPostFramebuffer.height);
		glDisable(GL_DEPTH_TEST);

		cloudPostShader.Activate();
		glUniform1i(glGetUniformLocation(cloudPostShader.ID, "clouds"), 0);
		glUniform1i(glGetUniformLocation(cloudPostShader.ID, "emissions"), 1);
		glUniform1i(glGetUniformLocation(cloudPostShader.ID, "depthMap"), 2);
		glUniform1f(glGetUniformLocation(cloudPostShader.ID, "time"), time);
		glUniform2f(glGetUniformLocation(cloudPostShader.ID, "resolution"), static_cast<float>(screenWidth), static_cast<float>(screenHeight));
		glUniform2f(glGetUniformLocation(cloudPostShader.ID, "cloudRenderResolution"), static_cast<float>(screenWidth), static_cast<float>(screenHeight));
		glUniform4f(glGetUniformLocation(cloudPostShader.ID, "lightPos"), 0.5f, 0.5f, 0.0f, 1.0f);
		glUniform1i(glGetUniformLocation(cloudPostShader.ID, "isLightInFront"), 1);
		glUniform1i(glGetUniformLocation(cloudPostShader.ID, "enableGodRays"), settings.godRays ? 1 : 0);
		glUniform1f(glGetUniformLocation(cloudPostShader.ID, "lightDotCameraFront"), 0.0f);

		BindTexture2D(cloudTextures[CloudColor], 0);
		BindTexture2D(cloudTextures[CloudBloom], 1);
		BindTexture2D(sceneFramebuffer.depth, 2);
		DrawFullscreenQuad();
	}
}

GLuint VolumetricCloudRenderer::GetCloudOutputTexture(const SkyCloudSettings& settings) const
{
	return settings.postProcessing ? cloudPostFramebuffer.color : cloudTextures[CloudColor];
}

void VolumetricCloudRenderer::CompositeToScreen(const SkyCloudSettings& settings, bool wireframe)
{
	if (!ready)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenWidth, screenHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	compositeShader.Activate();
	glUniform1i(glGetUniformLocation(compositeShader.ID, "screenTexture"), 0);
	glUniform1i(glGetUniformLocation(compositeShader.ID, "cloudTEX"), 1);
	glUniform1i(glGetUniformLocation(compositeShader.ID, "depthTex"), 2);
	glUniform1i(glGetUniformLocation(compositeShader.ID, "wireframe"), wireframe ? 1 : 0);
	glUniform2f(glGetUniformLocation(compositeShader.ID, "resolution"), static_cast<float>(screenWidth), static_cast<float>(screenHeight));

	BindTexture2D(sceneFramebuffer.color, 0);
	BindTexture2D(GetCloudOutputTexture(settings), 1);
	BindTexture2D(sceneFramebuffer.depth, 2);
	DrawFullscreenQuad();
	glEnable(GL_DEPTH_TEST);
}

void VolumetricCloudRenderer::DrawFullscreenQuad() const
{
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}
