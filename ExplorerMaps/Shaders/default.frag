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
uniform float objectLightBoost;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 ambientColor;
uniform float dayFactor;
uniform float nightFactor;
uniform float diffuseIntensity;
uniform float specularIntensity;
uniform float sunHeight;
uniform float time;
uniform float rainIntensity;
uniform float cloudCoverage;
uniform float cloudSpeed;
uniform float cloudDensity;
uniform int lampLightCount;
uniform vec3 lampLightPositions[12];
uniform vec3 lampLightColors[12];
uniform float lampLightRadii[12];
uniform float lampLightIntensities[12];

float Hash31(vec3 p)
{
	p = fract(p * 0.1031f);
	p += dot(p, p.yzx + 33.33f);
	return fract((p.x + p.y) * p.z);
}

float Hash21(vec2 p)
{
	p = fract(p * vec2(123.34f, 456.21f));
	p += dot(p, p + 45.32f);
	return fract(p.x * p.y);
}

float Noise2D(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0f - 2.0f * f);

	float a = Hash21(i);
	float b = Hash21(i + vec2(1.0f, 0.0f));
	float c = Hash21(i + vec2(0.0f, 1.0f));
	float d = Hash21(i + vec2(1.0f, 1.0f));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float Fbm2D(vec2 p)
{
	float value = 0.0f;
	float amplitude = 0.5f;
	for (int i = 0; i < 4; ++i)
	{
		value += Noise2D(p) * amplitude;
		p = p * 2.03f + vec2(13.1f, 7.7f);
		amplitude *= 0.5f;
	}
	return value;
}

vec3 TonemapACES(vec3 color)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
}

float SaturatedColorMask(vec3 value)
{
	float colorMax = max(value.r, max(value.g, value.b));
	float colorMin = min(value.r, min(value.g, value.b));
	return smoothstep(0.12f, 0.48f, colorMax - colorMin) * smoothstep(0.14f, 0.62f, colorMax);
}

void main()
{
	vec3 normal = normalize(gl_FrontFacing ? Normal : -Normal);
	vec3 viewDirection = normalize(viewPos - crntPos);
	vec3 lightDirection = normalize(lightPos - crntPos);
	float lambert = max(dot(normal, lightDirection), 0.0f);
	float hemi = clamp(normal.y * 0.5f + 0.5f, 0.0f, 1.0f);
	vec4 base = texture(diffuse0, texCoord) * vec4(color, 1.0f);
	float baseLuma = dot(base.rgb, vec3(0.299f, 0.587f, 0.114f));
	float rain = clamp(rainIntensity, 0.0f, 1.0f);
	float cinematicDay = dayFactor * (1.0f - nightFactor) * (1.0f - rain * 0.55f);
	float roadMacroNoise = Hash31(floor(crntPos * vec3(0.018f, 0.0f, 0.018f) + vec3(5.0f, 0.0f, 11.0f)));
	float surfaceFineNoise = Hash31(floor(crntPos * vec3(0.145f, 0.27f, 0.145f) + vec3(19.0f, 7.0f, 3.0f)));

	vec3 halfDirection = normalize(lightDirection + viewDirection);
	float specular = pow(max(dot(normal, halfDirection), 0.0f), 48.0f) * specularIntensity;
	float glassMask = smoothstep(0.03f, 0.22f, base.b - base.r) *
		smoothstep(0.12f, 0.50f, base.g) *
		smoothstep(0.16f, 0.62f, base.b);
	float darkSurfaceMask = (1.0f - smoothstep(0.10f, 0.34f, baseLuma)) *
		smoothstep(0.08f, 0.42f, hemi);
	float horizontalSurface = smoothstep(0.62f, 0.92f, normal.y);
	float roadMask = darkSurfaceMask * horizontalSurface * (1.0f - glassMask);
	float brightGroundMask = horizontalSurface * (1.0f - glassMask) * smoothstep(0.46f, 0.86f, baseLuma);
	float facadeMask = (1.0f - horizontalSurface * 0.85f) * (1.0f - glassMask * 0.55f);
	float detailEdges = clamp(length(vec2(dFdx(baseLuma), dFdy(baseLuma))) * 5.0f, 0.0f, 0.32f);
	float nightRainGlow = max(nightFactor, rain * 0.46f);
	float dayDepth = dayFactor * (1.0f - rain * 0.35f);
	float cinematicNight = nightFactor * (1.0f - rain * 0.18f);
	float goldenDay = dayFactor * (1.0f - nightFactor) * (1.0f - rain * 0.45f) * (1.0f - smoothstep(0.72f, 0.98f, sunHeight));

	float saturatedMask = SaturatedColorMask(base.rgb) * facadeMask;
	float redNeon = smoothstep(0.12f, 0.42f, base.r - max(base.g, base.b));
	float greenNeon = smoothstep(0.10f, 0.36f, base.g - max(base.r, base.b));
	float blueNeon = smoothstep(0.10f, 0.34f, base.b - max(base.r, base.g));
	float yellowNeon = smoothstep(0.68f, 0.92f, base.r) * smoothstep(0.52f, 0.86f, base.g) * (1.0f - smoothstep(0.38f, 0.72f, base.b));
	vec3 neonPalette =
		vec3(1.00f, 0.16f, 0.06f) * redNeon +
		vec3(0.16f, 1.00f, 0.42f) * greenNeon +
		vec3(0.10f, 0.54f, 1.00f) * blueNeon +
		vec3(1.00f, 0.78f, 0.18f) * yellowNeon;
	float neonMask = saturatedMask * clamp(redNeon + greenNeon + blueNeon + yellowNeon, 0.0f, 1.0f);
	vec3 neonEmissive = neonPalette * neonMask * (0.20f + nightRainGlow * 1.90f);

	vec3 glassTint = mix(vec3(0.16f, 0.20f, 0.28f), vec3(0.42f, 0.62f, 0.84f), dayFactor);
	vec3 surfaceAlbedo = base.rgb;
	surfaceAlbedo = mix(surfaceAlbedo, mix(surfaceAlbedo, glassTint, 0.56f), glassMask * (0.45f + dayFactor * 0.45f));
	surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.58f, 0.61f, 0.64f), roadMask * (0.68f + dayFactor * 0.12f + rain * 0.38f));
	surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.62f, 0.69f, 0.76f), rain * (0.18f + roadMask * 0.42f));
	surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.56f, 0.58f, 0.60f), brightGroundMask * dayDepth * 0.62f);
	surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.88f, 0.96f, 1.08f), glassMask * dayDepth * 0.20f);
	float surfaceVariation = (roadMacroNoise - 0.5f) * 0.18f + (surfaceFineNoise - 0.5f) * 0.08f;
	surfaceAlbedo *= 1.0f + surfaceVariation * cinematicDay * (roadMask * 0.85f + brightGroundMask * 0.58f + facadeMask * 0.22f);
	float roadGrime = roadMask * cinematicDay * smoothstep(0.34f, 0.95f, roadMacroNoise);
	surfaceAlbedo *= 1.0f - roadGrime * 0.18f;
	surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * vec3(0.92f, 0.98f, 1.08f), glassMask * cinematicDay * 0.18f);
	surfaceAlbedo *= 1.0f - detailEdges * (0.22f + dayFactor * 0.40f) * (1.0f - glassMask * 0.45f);

	vec3 skyBounceColor = mix(vec3(0.10f, 0.12f, 0.18f), vec3(0.50f, 0.68f, 0.92f), dayFactor);
	skyBounceColor = mix(skyBounceColor, vec3(0.96f, 0.66f, 0.36f), goldenDay * 0.42f);
	float sunKey = pow(lambert, 0.62f) * diffuseIntensity * (1.22f + dayFactor * 0.56f) * mix(1.0f, 0.46f, rain);
	float ambientStrength = mix(0.68f + hemi * 0.34f, 0.27f + hemi * 0.30f, dayFactor);
	vec2 cloudShadowWind = normalize(vec2(0.92f, 0.22f));
	vec2 cloudShadowUv = crntPos.xz * 0.00145f + cloudShadowWind * time * (0.0085f * max(cloudSpeed, 0.05f)) + lightDirection.xz * 0.070f;
	float cloudLarge = Fbm2D(cloudShadowUv * 0.82f);
	float cloudMedium = Fbm2D(cloudShadowUv * 1.70f + vec2(9.3f, 2.7f));
	float cloudDetail = Fbm2D(cloudShadowUv * 3.90f + vec2(-4.6f, 11.2f));
	float cloudShape = cloudLarge * 0.58f + cloudMedium * 0.30f + cloudDetail * 0.12f;
	float cloudThreshold = mix(0.82f, 0.46f, clamp((cloudCoverage - 0.20f) / 1.05f, 0.0f, 1.0f));
	float cloudShadowSoft = smoothstep(cloudThreshold - 0.08f, cloudThreshold + 0.22f, cloudShape);
	float cloudShadowCore = smoothstep(cloudThreshold + 0.06f, cloudThreshold + 0.27f, cloudShape);
	float cloudShadow = cloudShadowSoft * 0.58f + cloudShadowCore * 0.42f;
	cloudShadow *= cinematicDay * smoothstep(0.14f, 0.66f, sunHeight) * (1.0f - rain * 0.82f) * clamp(cloudDensity * 0.34f, 0.08f, 0.48f);
	float cloudShade = cloudShadow * (0.045f + lambert * 0.055f) * (0.62f + horizontalSurface * 0.38f);
	vec3 ambientTerm = ambientColor * ambientStrength;
	vec3 directTerm = lightColor.rgb * (sunKey + hemi * 0.035f * dayFactor) * (1.0f - cloudShade);
	ambientTerm += skyBounceColor * cloudShadowSoft * cinematicDay * 0.030f;
	vec3 litColor = surfaceAlbedo * (ambientTerm + directTerm);
	litColor += surfaceAlbedo * vec3(0.18f, 0.095f, 0.030f) * lambert * goldenDay * 0.34f;
	litColor += vec3(0.26f, 0.16f, 0.055f) * brightGroundMask * goldenDay * 0.12f;

	float shadowSide = (1.0f - lambert) * (1.0f - hemi * 0.20f) * dayFactor * (1.0f + rain * 0.42f);
	float contactShadow = (facadeMask * (1.0f - hemi) * 0.38f + brightGroundMask * detailEdges * 0.66f + detailEdges * facadeMask * 0.42f) * dayDepth;
	litColor *= 1.0f - shadowSide * 0.28f - contactShadow * 0.30f;
	litColor += vec3(0.020f, 0.034f, 0.062f) * shadowSide;
	litColor += skyBounceColor * cinematicDay * (shadowSide * 0.055f + contactShadow * 0.035f + cloudShadow * 0.030f);
	litColor += surfaceAlbedo * vec3(0.050f, 0.082f, 0.170f) * cinematicNight * (0.28f + hemi * 0.72f);
	float moonRim = pow(1.0f - clamp(dot(normal, viewDirection), 0.0f, 1.0f), 2.4f) * facadeMask * cinematicNight;
	litColor += vec3(0.055f, 0.105f, 0.235f) * moonRim * 0.42f;

	float fresnel = pow(1.0f - clamp(dot(normal, viewDirection), 0.0f, 1.0f), 3.0f);
	float sunRim = pow(1.0f - clamp(dot(normal, viewDirection), 0.0f, 1.0f), 2.0f) * lambert * facadeMask * cinematicDay;
	litColor += lightColor.rgb * sunRim * 0.20f;
	litColor += skyBounceColor * fresnel * glassMask * (0.42f + 0.78f * dayFactor);
	litColor += lightColor.rgb * specular * (0.14f + glassMask * 1.15f + darkSurfaceMask * 0.36f + rain * roadMask * 1.35f);
	litColor += skyBounceColor * fresnel * roadMask * (0.06f + nightFactor * 0.12f + rain * 0.52f);
	float roadSunGlint = roadMask * cinematicDay * pow(max(dot(reflect(-viewDirection, normal), lightDirection), 0.0f), 4.0f);
	litColor += vec3(1.0f, 0.78f, 0.42f) * roadSunGlint * (0.10f + goldenDay * 0.10f);

	vec2 streetGrid = crntPos.xz * vec2(0.0065f, 0.019f);
	vec2 streetCell = floor(streetGrid);
	vec2 streetLocal = fract(streetGrid) - 0.5f;
	float streetSeed = Hash31(vec3(streetCell, 8.0f));
	float streetPool = exp(-(streetLocal.x * streetLocal.x * 16.0f + streetLocal.y * streetLocal.y * 4.0f)) *
		smoothstep(0.28f, 0.96f, streetSeed) * roadMask;
	vec3 streetPoolColor = mix(vec3(1.0f, 0.62f, 0.22f), vec3(0.40f, 0.75f, 1.0f), Hash31(vec3(streetCell, 19.0f)) * 0.38f);
	litColor += streetPoolColor * streetPool * (nightFactor * 0.86f + rain * 0.48f);
	float asphaltSheen = roadMask * cinematicNight * (0.12f + fresnel * 0.28f);
	litColor += vec3(0.045f, 0.082f, 0.160f) * asphaltSheen;

	float puddleNoise = Hash31(floor(crntPos * vec3(0.038f, 0.0f, 0.038f) + vec3(time * 0.12f, 0.0f, time * 0.09f)));
	float puddleMask = roadMask * rain * smoothstep(0.38f, 0.92f, puddleNoise);
	litColor += skyBounceColor * puddleMask * (0.18f + fresnel * 0.70f);
	litColor += (neonEmissive + streetPoolColor * streetPool) * roadMask * rain * (0.24f + fresnel * 0.46f);

	float facadeDepth = (1.0f - hemi) * (1.0f - glassMask) * smoothstep(0.18f, 0.62f, baseLuma);
	float fineGrime = Hash31(floor(crntPos * vec3(0.075f, 0.19f, 0.075f))) * 0.035f;
	litColor *= 1.0f - facadeDepth * (0.14f + fineGrime);

	vec3 lampContribution = vec3(0.0f);
	vec3 lampHaloContribution = vec3(0.0f);
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
		float coneRadius = max(verticalDrop * (0.58f + rain * 0.18f), 12.0f);
		float coneMask = 1.0f - smoothstep(coneRadius * 0.45f, coneRadius, horizontalDistance);
		float lampLightRadius = lampLightRadii[i];
		float heightMask = smoothstep(0.0f, 18.0f, verticalDrop) *
			(1.0f - smoothstep(lampLightRadius, lampLightRadius * 1.15f, verticalDrop));

		float attenuation = coneMask * heightMask;
		attenuation *= attenuation;
		float wideHalo = exp(-(horizontalDistance * horizontalDistance) / max(lampLightRadius * lampLightRadius * 0.42f, 1.0f)) *
			(1.0f - smoothstep(0.0f, lampLightRadius * 1.35f, distanceToLamp));

		vec3 lampDirection = normalize(toLamp);
		float lampDiffuse = max(dot(normal, lampDirection), 0.0f);
		lampContribution += lampLightColors[i] * attenuation * (0.08f + lampDiffuse * lampLightIntensities[i] * (1.0f + rain * 0.35f));
		lampHaloContribution += lampLightColors[i] * wideHalo * (0.05f + roadMask * (0.15f + rain * 0.30f));

		float lampHeadMask = 1.0f - smoothstep(0.0f, 14.0f, distanceToLamp);
		emissiveLamp += lampLightColors[i] * lampHeadMask * (3.2f + rain * 1.4f);
	}

	litColor += surfaceAlbedo * lampContribution * nightFactor;
	litColor += lampHaloContribution * nightRainGlow;
	litColor += emissiveLamp * nightRainGlow;

	vec3 windowCoord = crntPos * vec3(0.055f, 0.16f, 0.055f);
	vec3 windowCell = floor(windowCoord);
	vec3 windowLocal = fract(windowCoord) - 0.5f;
	float windowSeed = Hash31(windowCell);
	float windowBandY = fract(crntPos.y * 0.16f);
	float windowBand = smoothstep(0.12f, 0.30f, windowBandY) *
		(1.0f - smoothstep(0.72f, 0.92f, windowBandY));
	float windowPattern = step(0.48f, windowSeed);
	float windowGlowMask = glassMask * windowBand * windowPattern * smoothstep(0.10f, 0.72f, nightFactor);
	vec3 warmWindow = mix(vec3(1.0f, 0.56f, 0.25f), vec3(1.0f, 0.82f, 0.48f), Hash31(windowCell + vec3(17.0f)));
	vec3 windowEmissive = warmWindow * windowGlowMask * (0.75f + nightFactor * 1.25f);
	float windowAuraMask = facadeMask * windowPattern * exp(-(windowLocal.y * windowLocal.y) * 14.0f) *
		smoothstep(0.10f, 0.80f, nightFactor) * (0.15f + glassMask * 0.25f);
	vec3 windowAura = warmWindow * windowAuraMask * 0.16f;
	litColor += windowEmissive;
	litColor += windowAura;
	litColor += neonEmissive;

	vec3 bloomSource = windowEmissive + windowAura + neonEmissive + emissiveLamp * nightRainGlow + lampHaloContribution * nightRainGlow;
	float bloomAmount = clamp(length(bloomSource), 0.0f, 4.0f);
	litColor += bloomSource * (0.10f + rain * 0.08f + fresnel * 0.08f) * bloomAmount;

	vec3 visibleObjectColor = base.rgb * (1.38f + dayFactor * 0.28f) + skyBounceColor * 0.10f + vec3(0.045f);
	litColor = mix(litColor, max(litColor, visibleObjectColor), clamp(objectLightBoost, 0.0f, 1.0f));

	float viewDistance = length(viewPos - crntPos);
	float fogDensity = mix(0.00030f, 0.00088f, nightFactor) + rain * 0.00048f;
	float distanceFog = 1.0f - exp(-viewDistance * fogDensity);
	distanceFog = clamp(distanceFog * distanceFog * mix(0.78f, 1.35f, nightFactor + rain * 0.45f), 0.0f, mix(0.38f, 0.68f, nightFactor) + rain * 0.18f);
	float twilight = 1.0f - smoothstep(0.05f, 0.38f, abs(sunHeight));
	vec3 dayFogColor = mix(vec3(0.76f, 0.84f, 0.93f), vec3(0.96f, 0.75f, 0.50f), goldenDay * 0.62f);
	vec3 nightFogColor = vec3(0.10f, 0.11f, 0.17f);
	vec3 twilightFogColor = vec3(0.92f, 0.52f, 0.34f);
	vec3 rainFogColor = vec3(0.29f, 0.34f, 0.40f);
	vec3 fogColor = mix(dayFogColor, nightFogColor, nightFactor);
	fogColor = mix(fogColor, twilightFogColor, twilight * 0.42f);
	fogColor = mix(fogColor, vec3(0.075f, 0.095f, 0.170f), cinematicNight * 0.42f);
	fogColor = mix(fogColor, rainFogColor, rain * 0.62f);
	litColor = mix(litColor, fogColor, distanceFog);

	litColor = TonemapACES(litColor * 1.16f);
	float luma = dot(litColor, vec3(0.299f, 0.587f, 0.114f));
	litColor += vec3(0.006f, 0.011f, 0.026f) * cinematicNight * (1.0f - smoothstep(0.025f, 0.18f, luma));
	litColor = mix(vec3(luma), litColor, mix(1.22f, 0.98f, rain));
	float gradedLuma = dot(litColor, vec3(0.299f, 0.587f, 0.114f));
	vec3 dayGrade = mix(vec3(gradedLuma), litColor, 1.18f) * vec3(1.05f, 1.02f, 0.94f) + vec3(0.006f, 0.004f, 0.0f);
	litColor = mix(litColor, dayGrade, cinematicDay * 0.34f);
	litColor = mix(litColor, litColor * vec3(1.08f, 1.01f, 0.90f), goldenDay * 0.16f);
	litColor = mix(litColor, litColor * vec3(0.92f, 1.00f, 1.13f), cinematicNight * 0.18f);
	litColor = clamp((litColor - 0.5f) * (1.16f + cinematicDay * 0.10f) + 0.5f, 0.0f, 1.0f);
	litColor = pow(max(litColor, vec3(0.0f)), vec3(1.0f / 1.18f));
	FragColor = vec4(litColor, base.a);
}
