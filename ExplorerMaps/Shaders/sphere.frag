#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 LocalNormal;

uniform vec3 color;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float sunHeight;
uniform float isDay;
uniform float useTexture;
uniform float alpha;
uniform float unlit;
uniform sampler2D sphereTexture;

const float PI = 3.14159265359;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 baseColor = color;

    if (useTexture > 0.5) {
        vec2 moonUv = vec2(
            0.5 + atan(LocalNormal.z, LocalNormal.x) / (2.0 * PI),
            0.5 - asin(clamp(LocalNormal.y, -1.0, 1.0)) / PI
        );

        baseColor = texture(sphereTexture, moonUv).rgb;
        baseColor = mix(baseColor, color, 0.18);
    }

    if (unlit > 0.5) {
        FragColor = vec4(baseColor, alpha);
        return;
    }

    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    
    float diff = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    vec3 ambient = baseColor * 0.3;
    vec3 diffuse = baseColor * diff * 0.8;
    vec3 specular = vec3(1.0, 0.95, 0.85) * spec * 0.5;
    
    vec3 result = ambient + diffuse + specular;
    
    float glow = pow(1.0 - abs(dot(norm, viewDir)), 2.0);
    if (isDay > 0.5) {
        result += vec3(1.0, 0.7, 0.3) * glow * 0.8;
    } else {
        result += vec3(0.5, 0.6, 0.8) * glow * 0.5;
    }
    
    FragColor = vec4(result, alpha);
}
