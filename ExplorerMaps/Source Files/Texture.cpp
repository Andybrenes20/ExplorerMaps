#include"Texture.h"

namespace
{
	void initializeTexture(GLuint& id, GLuint slot)
	{
		glGenTextures(1, &id);
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	GLenum getTextureFormat(int numColCh)
	{
		switch (numColCh)
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
			throw std::invalid_argument("Automatic Texture type recognition failed");
		}
	}

	void uploadTextureBytes(GLuint id, const unsigned char* bytes, int widthImg, int heightImg, int numColCh)
	{
		const GLenum format = getTextureFormat(numColCh);
		glTexImage2D(GL_TEXTURE_2D, 0, format, widthImg, heightImg, 0, format, GL_UNSIGNED_BYTE, bytes);

		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

Texture::Texture(const char* image, const char* texType, GLuint slot)
{
	// Assigns the type of the texture ot the texture object
	type = texType;

	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(true);
	// Reads the image from a file and stores it in bytes
	unsigned char* bytes = stbi_load(image, &widthImg, &heightImg, &numColCh, 0);
	if (bytes == nullptr)
	{
		throw std::runtime_error(std::string("No se pudo cargar la textura: ") + image);
	}

	unit = slot;
	initializeTexture(ID, slot);
	uploadTextureBytes(ID, bytes, widthImg, heightImg, numColCh);

	// Deletes the image data as it is already in the OpenGL Texture object
	stbi_image_free(bytes);
}

Texture::Texture(const unsigned char* bytes, int length, const char* texType, GLuint slot)
{
	type = texType;
	unit = slot;

	int widthImg, heightImg, numColCh;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* decodedBytes = stbi_load_from_memory(bytes, length, &widthImg, &heightImg, &numColCh, 0);
	if (decodedBytes == nullptr)
	{
		throw std::runtime_error("No se pudo decodificar la textura embebida.");
	}

	initializeTexture(ID, slot);
	uploadTextureBytes(ID, decodedBytes, widthImg, heightImg, numColCh);
	stbi_image_free(decodedBytes);
}

Texture::Texture(const unsigned char* pixels, int width, int height, const char* texType, GLuint slot, GLenum format)
{
	type = texType;
	unit = slot;
	initializeTexture(ID, slot);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit)
{
	this->unit = unit;
	// Gets the location of the uniform
	GLuint texUni = glGetUniformLocation(shader.ID, uniform);
	// Shader needs to be activated before changing the value of a uniform
	shader.Activate();
	// Sets the value of the uniform
	glUniform1i(texUni, unit);
}

void Texture::Bind()
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

void Texture::Unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Delete()
{
	glDeleteTextures(1, &ID);
}
