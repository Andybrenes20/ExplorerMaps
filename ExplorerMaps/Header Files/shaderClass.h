#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include<glad/glad.h>
#include<iostream>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>
#include<stdexcept>
#include<string>
#include<unordered_map>

std::string get_file_contents(const char* filename);

class Shader {

public:
	GLuint ID;
	Shader(const char* vertexFile, const char* fragmentFile);

	void Activate();
	void Delete();
	GLint GetUniformLocation(const char* uniformName);

private:
	std::unordered_map<std::string, GLint> uniformLocationCache;
};



#endif
