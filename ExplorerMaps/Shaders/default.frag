#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

// Imports the normal from the Vertex Shader
in vec3 Normal;
// Imports the current fragment position from the Vertex Shader
in vec3 crntPos;
// Imports the color from the Vertex Shader
in vec3 color;
// Imports the texture coordinates from the Vertex Shader
in vec2 texCoord;



// Gets the Texture Units from the main function
uniform sampler2D diffuse0;
// Gets the color of the light from the main function
uniform vec4 lightColor;
uniform float nightFactor;
uniform int lampLightCount;
uniform vec3 lampLightPositions[12];
uniform vec3 lampLightColor;
uniform float lampLightRadius;
uniform float lampLightIntensity;

void main()
{
	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(vec3(-0.45f, 1.0f, 0.35f));
	float lambert = max(dot(normal, lightDirection), 0.0f);
	float hemi = clamp(normal.y * 0.5f + 0.5f, 0.0f, 1.0f);
	float lighting = 0.68f + lambert * 0.32f + hemi * 0.18f;
	vec4 base = texture(diffuse0, texCoord) * vec4(color, 1.0f);
	vec3 litColor = base.rgb * lighting * lightColor.rgb;

	vec3 lampContribution = vec3(0.0f);
	vec3 emissiveLamp = vec3(0.0f);
	for (int i = 0; i < lampLightCount && i < 12; ++i)
	{
		vec3 toLamp = lampLightPositions[i] - crntPos;
		float distanceToLamp = length(toLamp);
		if (distanceToLamp < 0.001f)
			continue;

		vec3 lampToFragment = crntPos - lampLightPositions[i];
		float verticalDrop = lampLightPositions[i].y - crntPos.y;
		float horizontalDistance = length(lampToFragment.xz);
		float coneRadius = max(verticalDrop * 0.34f, 1.0f);
		float coneMask = smoothstep(coneRadius, coneRadius * 0.45f, horizontalDistance);
		float heightMask = smoothstep(0.0f, 18.0f, verticalDrop) *
			(1.0f - smoothstep(lampLightRadius, lampLightRadius * 1.15f, verticalDrop));

		float attenuation = coneMask * heightMask;
		attenuation *= attenuation;

		vec3 lampDirection = normalize(toLamp);
		float lampDiffuse = max(dot(normal, lampDirection), 0.0f);
		lampContribution += lampLightColor * attenuation * (0.05f + lampDiffuse * lampLightIntensity);

		float lampHeadMask = smoothstep(4.5f, 0.0f, distanceToLamp);
		emissiveLamp += lampLightColor * lampHeadMask * 2.8f;
	}

	litColor += base.rgb * lampContribution * nightFactor;
	litColor += emissiveLamp * nightFactor;
	litColor = pow(max(litColor, vec3(0.0f)), vec3(1.0f / 1.35f));
	FragColor = vec4(litColor, base.a);
}
