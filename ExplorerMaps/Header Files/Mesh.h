#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include<string>
#include<vector>

#include"VAO.h"
#include"EBO.h"
#include"Camara.h"
#include"Texture.h"
#include"Vertex.h"

class Mesh
{
public:
	std::vector <Vertex> vertices;
	std::vector <GLuint> indices;
	std::vector <Texture> textures;
	// Store VAO in public so it can be used in the Draw function
	VAO VAO;

	// Initializes the mesh
	Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures);

	// Draws the mesh
	void Draw
	(
		Shader& shader,
		const glm::mat4& modelMatrix
	);
};
#endif
