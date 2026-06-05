#version 330 core

out vec4 FragColor;

in vec3 localDirection;

uniform sampler2D skyboxTexture;

const float PI = 3.14159265359;

vec2 sampleSphericalMap(vec3 direction)
{
	vec3 dir = normalize(direction);
	float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
	float v = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;
	return vec2(u, 1.0 - v);
}

void main()
{
	vec2 uv = sampleSphericalMap(localDirection);
	vec3 color = texture(skyboxTexture, uv).rgb;
	FragColor = vec4(color, 1.0);
}
