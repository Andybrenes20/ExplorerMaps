#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    // Eliminamos la traslación de la vista para que el cielo te siga
    vec4 pos = projection * view * vec4(aPos, 1.0);
    // Truco Z=W: Fomzar la profundidad al valor máximo (1.0)
    gl_Position = pos.xyww; 
}