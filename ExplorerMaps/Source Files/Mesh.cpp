#include "Mesh.h"

#include<utility>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures)
{
	Mesh::vertices = std::move(vertices);
	Mesh::indices = std::move(indices);
	Mesh::textures = std::move(textures);

	VAO.Bind();
	// Generates Vertex Buffer Object and links it to vertices
	VBO VBO(reinterpret_cast<GLfloat*>(Mesh::vertices.data()), static_cast<GLsizeiptr>(Mesh::vertices.size() * sizeof(Vertex)));
	// Generates Element Buffer Object and links it to indices
	EBO EBO(Mesh::indices.data(), static_cast<GLsizeiptr>(Mesh::indices.size() * sizeof(GLuint)));
	// Links VBO attributes such as coordinates and colors to VAO
	VAO.LinkAttrib(VBO, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	VAO.LinkAttrib(VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	VAO.LinkAttrib(VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	VAO.LinkAttrib(VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));
	// Unbind all to prevent accidentally modifying them
	VAO.Unbind();
	VBO.Unbind();
	EBO.Unbind();
}


void Mesh::Draw
(
	Shader& shader,
	const glm::mat4& modelMatrix
)
{
	VAO.Bind();

	// Keep track of how many of each type of textures we have
	unsigned int numDiffuse = 0;
	bool hasDiffuse = false;

	for (unsigned int i = 0; i < textures.size(); i++)
	{
		std::string num;
		std::string type = textures[i].type;
		if (type == "diffuse")
		{
			num = std::to_string(numDiffuse++);
			hasDiffuse = true;
		}
		else
		{
			continue;
		}
		glUniform1i(shader.GetUniformLocation((type + num).c_str()), i);
		textures[i].unit = i;
		textures[i].Bind();
	}

	glUniformMatrix4fv(shader.GetUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

	// Draw the actual mesh
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
}
