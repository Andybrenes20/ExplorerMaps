#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal; // Las normales para la luz

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main() {
    // Posición en el mundo 3D
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Rotar las normales si el modelo rota
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // Tu línea de seguridad que evita el bug del "Doble Volteo" de texturas
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y); 
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
