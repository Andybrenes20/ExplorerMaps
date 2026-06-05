#include "CubemapTextureLoader.h"

#include <iostream>
#include <stdexcept>

#include <stb/stb_image.h>

namespace
{
	GLenum GetCubemapFormat(int channelCount)
	{
		switch (channelCount)
		{
		case 1:
			return GL_RED;
		case 2:
			return GL_RG;
		case 3:
			return GL_RGB;
		case 4:
			return GL_RGBA;
		default:
			throw std::invalid_argument("Formato de textura cubemap no soportado.");
		}
	}
}

GLuint LoadCubemapTexture(const std::vector<std::string>& facePaths)
{
	if (facePaths.size() != 6)
	{
		throw std::invalid_argument("Un cubemap necesita exactamente 6 texturas.");
	}

	GLuint textureID = 0;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	stbi_set_flip_vertically_on_load(false);

	for (GLuint i = 0; i < static_cast<GLuint>(facePaths.size()); ++i)
	{
		int width = 0;
		int height = 0;
		int channelCount = 0;
		unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channelCount, 0);
		if (data == nullptr)
		{
			glDeleteTextures(1, &textureID);
			throw std::runtime_error("Fallo al cargar la textura del cubemap: " + facePaths[i]);
		}

		const GLenum format = GetCubemapFormat(channelCount);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0,
			format,
			width,
			height,
			0,
			format,
			GL_UNSIGNED_BYTE,
				data
			);
		stbi_image_free(data);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	return textureID;
}
