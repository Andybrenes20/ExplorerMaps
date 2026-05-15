#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;


// Imports the color from the Vertex Shader
in vec3 color;
// Imports the texture coordinates from the Vertex Shader
in vec2 texCoord;
// Imports the normal from the Vertex Shader
in vec3 Normal;
// Imports the current position from the Vertex Shader
in vec3 crntPos;

// Gets the Texture Unit from the main function
uniform sampler2D tex0;
// Gets the position of the camera from the main function
uniform vec3 camPos;

struct PointLight
{
	vec3 position;
	vec3 color;
	float intensity;
};

uniform PointLight pointLights[3];
uniform vec3 dirLightDirection;
uniform vec3 dirLightColor;
uniform vec3 spotLightPos;
uniform vec3 spotLightDirection;
uniform vec3 spotLightColor;

void main()
{
	vec3 normal = normalize(Normal);
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 textureColor = texture(tex0, texCoord).rgb;

	vec3 lighting = textureColor * 0.16f;

	vec3 directionalDirection = normalize(-dirLightDirection);
	float directionalDiffuse = max(dot(normal, directionalDirection), 0.0f);
	vec3 directionalReflection = reflect(-directionalDirection, normal);
	float directionalSpecular = pow(max(dot(viewDirection, directionalReflection), 0.0f), 16.0f);
	lighting += textureColor * dirLightColor * directionalDiffuse * 0.70f;
	lighting += dirLightColor * directionalSpecular * 0.18f;

	for (int i = 0; i < 3; ++i)
	{
		vec3 lightDirection = normalize(pointLights[i].position - crntPos);
		float distanceToLight = length(pointLights[i].position - crntPos);
		float attenuation = 1.0f / (1.0f + 0.14f * distanceToLight + 0.12f * distanceToLight * distanceToLight);
		float diffuse = max(dot(normal, lightDirection), 0.0f);
		vec3 reflectionDirection = reflect(-lightDirection, normal);
		float specular = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 24.0f);
		vec3 pointContribution = pointLights[i].color * pointLights[i].intensity * attenuation;
		lighting += textureColor * pointContribution * diffuse * 0.95f;
		lighting += pointContribution * specular * 0.30f;
	}

	vec3 spotDirection = normalize(spotLightPos - crntPos);
	float theta = dot(normalize(-spotDirection), normalize(spotLightDirection));
	float innerCutoff = cos(radians(12.5f));
	float outerCutoff = cos(radians(18.0f));
	float spotlightStrength = clamp((theta - outerCutoff) / (innerCutoff - outerCutoff), 0.0f, 1.0f);
	float spotDiffuse = max(dot(normal, spotDirection), 0.0f);
	vec3 spotReflection = reflect(-spotDirection, normal);
	float spotSpecular = pow(max(dot(viewDirection, spotReflection), 0.0f), 20.0f);
	lighting += textureColor * spotLightColor * spotDiffuse * spotlightStrength * 0.75f;
	lighting += spotLightColor * spotSpecular * spotlightStrength * 0.22f;

	FragColor = vec4(lighting, 1.0f);
}
