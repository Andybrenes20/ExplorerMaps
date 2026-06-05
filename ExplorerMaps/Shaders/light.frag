#version 330 core

out vec4 FragColor;

in vec3 Normal;
in vec3 crntPos;

uniform vec4 lightColor;
uniform vec3 dirLightDirection;

void main()
{
	vec3 normal = normalize(Normal);
	vec3 directionalDirection = normalize(-dirLightDirection);
	float ambient = 0.28f;
	float diffuse = max(dot(normal, directionalDirection), 0.0f);
	vec3 finalColor = lightColor.rgb * (ambient + diffuse * 0.72f);
	FragColor = vec4(finalColor, lightColor.a);
}
