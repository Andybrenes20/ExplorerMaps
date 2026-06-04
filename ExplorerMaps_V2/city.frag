#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D texture_diffuse1;
uniform vec3 viewPos;

// 1. EL SOL / LA LUNA
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

// 2. LOS FAROLES
struct PointLight {
    vec3 position;
    
    // Valores de atenuación (qué tan lejos llega la luz)
    float constant;
    float linear;
    float quadratic;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform float pointLightIntensity; // 0.0 de día, 1.0 de noche

// Funciones para calcular cada tipo de luz
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor);

void main() {
    vec4 texColorData = texture(texture_diffuse1, TexCoord);
    if(texColorData.a < 0.1) discard;
    vec3 texColor = texColorData.rgb;

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Fase 1: Luz Global (Sol o Luna)
    vec3 result = CalcDirLight(light, norm, viewDir, texColor);

    // Fase 2: Sumar la luz de los faroles (Solo brillarán si pointLightIntensity > 0)
    if (pointLightIntensity > 0.0) {
        for(int i = 0; i < NR_POINT_LIGHTS; i++) {
            result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
        }
    }

    FragColor = vec4(result, texColorData.a);
}

// Calcula el Sol/Luna
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 texColor) {
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 ambient = light.ambient * texColor;
    vec3 diffuse = light.diffuse * diff * texColor;
    return (ambient + diffuse);
}

// Calcula el halo de luz de un farol
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Calcular atenuación por distancia
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    vec3 ambient = light.ambient * texColor;
    vec3 diffuse = light.diffuse * diff * texColor;
    
    ambient *= attenuation * pointLightIntensity;
    diffuse *= attenuation * pointLightIntensity;
    
    return (ambient + diffuse);
}