#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "shaderClass.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"

namespace
{
    constexpr unsigned int kWindowWidth = 900;
    constexpr unsigned int kWindowHeight = 700;

    void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    void processInput(GLFWwindow* window)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }
}

int main()
{
    // Initialize GLFW before creating the window.
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Cubo 3D", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Load OpenGL function pointers with GLAD.
    if (!gladLoadGL())
    {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, kWindowWidth, kWindowHeight);
    glEnable(GL_DEPTH_TEST);

    // Each vertex stores position (x, y, z) and color (r, g, b).
    GLfloat vertices[] =
    {
        -0.5f, -0.5f, -0.5f, 1.0f, 0.2f, 0.2f,
         0.5f, -0.5f, -0.5f, 0.2f, 1.0f, 0.2f,
         0.5f,  0.5f, -0.5f, 0.2f, 0.2f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.2f,
        -0.5f, -0.5f,  0.5f, 1.0f, 0.2f, 1.0f,
         0.5f, -0.5f,  0.5f, 0.2f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 0.6f, 0.2f,
        -0.5f,  0.5f,  0.5f, 0.7f, 0.7f, 1.0f
    };

    GLuint indices[] =
    {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0,
        2, 3, 7, 7, 6, 2,
        1, 2, 6, 6, 5, 1,
        3, 0, 4, 4, 7, 3
    };

    Shader shaderProgram("Shaders/default.vert", "Shaders/default.frag");

    VAO vao;
    vao.Bind();

    VBO vbo(vertices, sizeof(vertices));
    EBO ebo(indices, sizeof(indices));

    // Attribute 0 reads the vertex position.
    vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, 6 * sizeof(float), reinterpret_cast<void*>(0));
    // Attribute 1 reads the vertex color.
    vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    vao.Unbind();
    vbo.Unbind();
    ebo.Unbind();

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.08f, 0.10f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shaderProgram.Activate();
        vao.Bind();
        ebo.Bind();

        // Build a simple camera and a rotating model transform.
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), static_cast<float>(glfwGetTime()) * glm::radians(35.0f), glm::vec3(0.5f, 1.0f, 0.0f));
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight), 0.1f, 100.0f);
        glm::mat4 mvp = projection * view * model;

        GLuint matrixLocation = glGetUniformLocation(shaderProgram.ID, "uMVP");
        glUniformMatrix4fv(matrixLocation, 1, GL_FALSE, glm::value_ptr(mvp));

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(std::size(indices)), GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    vao.Delete();
    vbo.Delete();
    ebo.Delete();
    shaderProgram.Delete();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
