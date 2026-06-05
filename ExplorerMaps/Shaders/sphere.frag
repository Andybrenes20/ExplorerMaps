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
    bool isSun = isDay > 0.5;

    if (useTexture > 0.5) {
        vec2 moonUv = vec2(
            0.5 + atan(LocalNormal.z, LocalNormal.x) / (2.0 * PI),
            0.5 - asin(clamp(LocalNormal.y, -1.0, 1.0)) / PI
        );

        baseColor = texture(sphereTexture, moonUv).rgb;
        baseColor = mix(baseColor, color, isSun ? 0.55 : 0.18);
    }

    if (unlit > 0.5) {
        FragColor = vec4(baseColor, alpha);
        return;
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
    
    vec3 ambient = baseColor * (isSun ? 0.78 : 0.3);
    vec3 diffuse = baseColor * diff * (isSun ? 1.30 : 0.8);
    vec3 specular = vec3(1.0, 0.95, 0.85) * spec * (isSun ? 0.85 : 0.5);
    
    vec3 result = ambient + diffuse + specular;
    
    float rim = pow(1.0 - abs(dot(norm, viewDir)), isSun ? 1.35 : 2.0);
    if (isSun) {
        float coreGlow = 1.0 - clamp(distance(LocalNormal.xy, vec2(0.0)), 0.0, 1.0);
        coreGlow = pow(coreGlow, 1.8);
        result += vec3(1.0, 0.78, 0.36) * rim * 1.6;
        result += vec3(1.0, 0.92, 0.62) * coreGlow * 1.1;
        result *= 1.18;
    } else {
        result += vec3(0.5, 0.6, 0.8) * rim * 0.5;
    }
    
    FragColor = vec4(result, alpha);
}
