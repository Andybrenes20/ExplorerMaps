#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skyboxDay;
uniform samplerCube skyboxNight;
uniform float blendFactor; // 0.0 = Día, 1.0 = Noche

void main() {
    // Leemos ambos cielos
    vec4 colorDay = texture(skyboxDay, TexCoords);
    vec4 colorNight = texture(skyboxNight, TexCoords);
    
    // Interpolación matemática en tiempo real
    FragColor = mix(colorDay, colorNight, blendFactor);
}