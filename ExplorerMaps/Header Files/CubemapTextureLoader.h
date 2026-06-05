#ifndef CUBEMAP_TEXTURE_LOADER_H
#define CUBEMAP_TEXTURE_LOADER_H

#include <string>
#include <vector>

#include <glad/glad.h>

GLuint LoadCubemapTexture(const std::vector<std::string>& facePaths);

#endif
