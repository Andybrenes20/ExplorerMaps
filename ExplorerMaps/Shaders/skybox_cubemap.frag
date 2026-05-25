#version 330 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube daySkybox;
uniform samplerCube nightSkybox;
uniform float blendFactor;
uniform float time;
uniform float sunHeight;
uniform vec3 sunDir;
uniform float cloudCoverage;
uniform float cloudSpeed;
uniform float cloudCrispiness;
uniform float cloudCurliness;
uniform float cloudDensity;

const bool useProceduralClouds = true;
const bool useRepoInspiredDaySky = true;

float hash(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; ++i)
	{
		value += noise(p) * amplitude;
		p = p * 2.03 + vec2(13.1, 7.7);
		amplitude *= 0.5;
	}
	return value;
}

float remap01(float value, float minValue, float maxValue)
{
	return clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);
}

void main()
{
	vec4 dayColor = texture(daySkybox, TexCoords);
	vec4 nightColor = texture(nightSkybox, TexCoords);

	vec3 dir = normalize(TexCoords);
	vec3 skySunDir = normalize(sunDir);
	vec4 proceduralDayColor = dayColor;
	vec4 proceduralNightColor = nightColor;

	if (useRepoInspiredDaySky)
	{
		float highSunBlend = remap01(sunHeight, 0.18, 0.82);
		vec3 highSunTop = vec3(0.5, 0.7, 0.8) * 1.05;
		vec3 highSunBottom = vec3(0.9, 0.9, 0.95);
		vec3 sunsetTop = vec3(133.0, 158.0, 214.0) / 255.0;
		vec3 sunsetBottom = vec3(241.0, 161.0, 161.0) / 255.0;

		vec3 skyTop = mix(sunsetTop, highSunTop, highSunBlend);
		vec3 skyBottom = mix(sunsetBottom, highSunBottom, highSunBlend);
		float gradient = clamp(1.0 - exp(8.5 - 17.0 * clamp(dir.y * 0.5 + 0.5, 0.0, 1.0)), 0.0, 1.0);
		vec3 proceduralSky = mix(skyBottom, skyTop, gradient);
		proceduralDayColor = vec4(proceduralSky, 1.0);
	}

	float stars = smoothstep(0.985, 1.0, fbm(dir.xz * 180.0 + dir.y * 23.0));
	float moonGlow = pow(max(dot(dir, normalize(-skySunDir)), 0.0), 24.0);
	moonGlow *= smoothstep(-0.10, -0.45, sunHeight);
	vec3 nightBase = mix(vec3(0.01, 0.02, 0.05), vec3(0.05, 0.07, 0.12), clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
	nightBase += vec3(0.85, 0.90, 1.0) * moonGlow * 0.18;
	nightBase += vec3(1.0) * stars * smoothstep(-0.12, -0.55, sunHeight);
	proceduralNightColor = vec4(nightBase, 1.0);

	vec4 skyColor = mix(proceduralDayColor, proceduralNightColor, clamp(blendFactor, 0.0, 1.0));

	float cloudBand = smoothstep(-0.08, 0.42, dir.y) * (1.0 - smoothstep(0.88, 0.995, dir.y));
	float dayCloudVisibility = (1.0 - clamp(blendFactor, 0.0, 1.0)) * smoothstep(-0.18, 0.18, sunHeight);

	if (useProceduralClouds && dayCloudVisibility > 0.001 && cloudBand > 0.0)
	{
		vec2 cloudUv = dir.xz / max(dir.y + 0.32, 0.16);
		vec2 windDir = normalize(vec2(0.92, 0.22));
		float speed = max(cloudSpeed, 0.05);
		float crispiness = max(cloudCrispiness, 0.35);
		float curliness = max(cloudCurliness, 0.10);
		float densityBoost = max(cloudDensity, 0.20);
		vec2 windLarge = windDir * (0.0045 * speed);
		vec2 windMedium = windDir * (0.0028 * speed) + vec2(-0.0009, 0.0011) * speed;
		vec2 windDetail = windDir * (0.0018 * speed) + vec2(0.0006, -0.0008) * speed;

		vec2 anisotropicUv = vec2(dot(cloudUv, windDir), dot(cloudUv, vec2(-windDir.y, windDir.x)) * (0.42 + curliness * 0.22));
		float distortionX = fbm(anisotropicUv * 0.32 + time * vec2(0.0007, 0.0002));
		float distortionY = fbm(anisotropicUv * 0.28 - time * vec2(0.0004, 0.0005));
		vec2 distortedUv = anisotropicUv + (vec2(distortionX, distortionY) - 0.5) * (0.45 + curliness * 0.45);

		float largeShape = fbm(distortedUv * (0.72 * crispiness) + time * windLarge);
		float mediumShape = fbm(distortedUv * (1.45 * crispiness) + time * windMedium);
		float detailShape = fbm(distortedUv * (3.20 * crispiness) + time * windDetail);
		float wisps = fbm(vec2(distortedUv.x * 1.8, distortedUv.y * (5.5 + curliness * 3.0)) + time * vec2(0.0020, 0.0004) * speed);

		float cloudShape = largeShape * 0.60 + mediumShape * 0.24 + detailShape * 0.08 + wisps * 0.08;
		float coverage = mix(0.40, 0.48, remap01(sunHeight, -0.08, 0.45)) - (cloudCoverage - 0.5) * 0.18;
		float cloudMask = smoothstep(coverage, coverage + 0.11, cloudShape) * cloudBand;
		float cloudCore = smoothstep(coverage + 0.03, coverage + 0.16, cloudShape);
		float cloudRim = smoothstep(coverage - 0.01, coverage + 0.10, cloudShape) - cloudCore;

		float lightFacing = smoothstep(-0.18, 0.32, dot(dir, skySunDir));
		float warmSun = remap01(sunHeight, -0.02, 0.30);
		float highSun = remap01(sunHeight, 0.25, 0.85);

		vec3 highlightColor = mix(vec3(1.0, 0.86, 0.72), vec3(0.98, 0.98, 0.94), highSun);
		vec3 bodyColor = mix(vec3(0.82, 0.84, 0.90), vec3(0.95, 0.96, 0.98), highSun);
		vec3 shadowColor = mix(vec3(97.0, 98.0, 120.0) / 255.0, vec3(65.0, 70.0, 80.0) * (1.5 / 255.0), highSun);

		float selfShadow = remap01(cloudShape, coverage + 0.01, coverage + 0.14);
		vec3 cloudColor = mix(shadowColor, bodyColor, selfShadow);
		cloudColor = mix(cloudColor, highlightColor, cloudRim * (0.25 + lightFacing * 0.75) * (0.45 + warmSun * 0.55));

		float cloudOpacity = cloudMask * (0.78 + cloudCore * 0.20) * dayCloudVisibility * densityBoost;
		cloudOpacity = clamp(cloudOpacity, 0.0, 1.0);
		skyColor.rgb = mix(skyColor.rgb, cloudColor, cloudOpacity);
		skyColor.rgb += highlightColor * cloudRim * lightFacing * 0.24 * dayCloudVisibility;
	}

	FragColor = vec4(clamp(skyColor.rgb, 0.0, 1.0), skyColor.a);
}
