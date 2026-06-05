#include "shaderClass.h"

std::string get_file_contents(const char* filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(static_cast<size_t>(in.tellg()));
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], static_cast<std::streamsize>(contents.size()));
		in.close();
		return contents;
	}

	throw std::runtime_error(std::string("No se pudo abrir el archivo: ") + filename);
}

namespace
{
	void CheckShaderCompile(GLuint shader, const char* shaderPath)
	{
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success)
			return;

		GLchar infoLog[2048] = {};
		glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Error compilando shader " << shaderPath << ":\n" << infoLog << std::endl;
	}

	void CheckProgramLink(GLuint program, const char* vertexPath, const char* fragmentPath)
	{
		GLint success = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (success)
			return;

		GLchar infoLog[2048] = {};
		glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Error enlazando shader program (" << vertexPath << ", " << fragmentPath << "):\n" << infoLog << std::endl;
	}
}

Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	CheckShaderCompile(vertexShader, vertexFile);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	CheckShaderCompile(fragmentShader, fragmentFile);

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);
	CheckProgramLink(ID, vertexFile, fragmentFile);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::Activate()
{
	glUseProgram(ID);
}

void Shader::Delete()
{
	glDeleteProgram(ID);
}

GLint Shader::GetUniformLocation(const char* uniformName)
{
	const auto cached = uniformLocationCache.find(uniformName);
	if (cached != uniformLocationCache.end())
	{
		return cached->second;
	}

	const GLint location = glGetUniformLocation(ID, uniformName);
	uniformLocationCache.emplace(uniformName, location);
	return location;
}
