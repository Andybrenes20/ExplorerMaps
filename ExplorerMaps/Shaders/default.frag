#version 330 core

out vec4 FragColor;

in vec3 Normal;
in vec3 crntPos;
in vec3 color;
in vec2 texCoord;

uniform sampler2D diffuse0;
// Gets the color of the light from the main function
uniform vec4 lightColor;
uniform float objectLightBoost;
uniform vec3 viewPos;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;

uniform vec3 moonDirection;
uniform vec3 moonColor;
uniform float moonIntensity;

uniform vec3 ambientColor;
uniform float sunHeight;
uniform float nightFactor;
uniform int lampLightCount;
uniform vec3 lampLightPositions[12];
uniform vec3 lampLightColors[12];
uniform float lampLightRadii[12];
uniform float lampLightIntensities[12];

float DirectionalTerm(vec3 lightDirection, vec3 normal, vec3 viewDir, float shininess, float wrapAmount)
{
	vec3 lightDir = normalize(-lightDirection);
	float nDotL = dot(normal, lightDir);
	float diffuse = clamp((nDotL + wrapAmount) / (1.0f + wrapAmount), 0.0f, 1.0f);
	vec3 reflectDir = reflect(-lightDir, normal);
	float specular = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
	return diffuse * 0.92f + specular * 0.08f;
}

void main()
{
	vec3 normal = normalize(gl_FrontFacing ? Normal : -Normal);
	vec3 viewDir = normalize(viewPos - crntPos);
	vec4 base = texture(diffuse0, texCoord) * vec4(color, 1.0f);

	float hemi = clamp(normal.y * 0.5f + 0.5f, 0.0f, 1.0f);
	float sunDayBlend = smoothstep(-0.30f, 0.28f, sunHeight);
	float dawnDusk = smoothstep(-0.30f, 0.02f, sunHeight) * (1.0f - smoothstep(0.18f, 0.55f, sunHeight));
	vec3 skyAmbient = vec3(0.16f, 0.19f, 0.24f);
	vec3 dayAmbient = max(ambientColor, skyAmbient);
	vec3 nightAmbient = vec3(0.025f, 0.028f, 0.045f);
	vec3 ambient = mix(nightAmbient, dayAmbient, sunDayBlend);
	ambient = mix(ambient, ambient * vec3(1.12f, 1.08f, 1.02f), hemi * 0.45f);
	ambient += vec3(0.045f, 0.030f, 0.020f) * dawnDusk;
	vec3 litColor = base.rgb * ambient;
	litColor += base.rgb * sunColor * sunIntensity * DirectionalTerm(sunDirection, normal, viewDir, 20.0f, 0.18f) * sunDayBlend;
	litColor += base.rgb * moonColor * moonIntensity * DirectionalTerm(moonDirection, normal, viewDir, 16.0f, 0.28f) * (1.0f - sunDayBlend);

	vec3 lampContribution = vec3(0.0f);
	vec3 emissiveLamp = vec3(0.0f);
	for (int i = 0; i < lampLightCount && i < 12; ++i)
	{
		vec3 toLamp = lampLightPositions[i] - crntPos;
		float distanceToLamp = length(toLamp);
		if (distanceToLamp < 0.001f)
		{
			continue;
		}

		vec3 lampToFragment = crntPos - lampLightPositions[i];
		float verticalDrop = lampLightPositions[i].y - crntPos.y;
		float horizontalDistance = length(lampToFragment.xz);
		float coneRadius = max(verticalDrop * 0.34f, 1.0f);
		float coneMask = smoothstep(coneRadius, coneRadius * 0.45f, horizontalDistance);
		float lampLightRadius = lampLightRadii[i];
		float heightMask = smoothstep(0.0f, 18.0f, verticalDrop) *
			(1.0f - smoothstep(lampLightRadius, lampLightRadius * 1.15f, verticalDrop));

		float attenuation = coneMask * heightMask;
		attenuation *= attenuation;

		vec3 lampDirection = normalize(toLamp);
		float lampDiffuse = max(dot(normal, lampDirection), 0.0f);
		lampContribution += lampLightColors[i] * attenuation * (0.05f + lampDiffuse * lampLightIntensities[i]);

		float lampHeadMask = smoothstep(4.5f, 0.0f, distanceToLamp);
		emissiveLamp += lampLightColors[i] * lampHeadMask * 2.8f;
	}

	litColor += base.rgb * lampContribution * nightFactor;
	litColor += emissiveLamp * nightFactor;
	vec3 visibleObjectColor = base.rgb * 2.35f + vec3(0.18f);
	litColor = mix(litColor, max(litColor, visibleObjectColor), clamp(objectLightBoost, 0.0f, 1.0f));
	litColor = pow(max(litColor, vec3(0.0f)), vec3(1.0f / 1.35f));
	FragColor = vec4(litColor, base.a);
}
