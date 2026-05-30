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

float starLayer(vec2 p, float scale, float threshold, float sizeBoost)
{
	vec2 cell = floor(p * scale);
	vec2 local = fract(p * scale) - 0.5;
	float seed = hash(cell);
	float active = smoothstep(threshold, 1.0, seed);
	float radius = length(local);
	float core = smoothstep(0.055 * sizeBoost, 0.0, radius);
	return active * core;
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

	vec2 starUv = vec2(atan(dir.z, dir.x) / 6.2831853 + 0.5, asin(clamp(dir.y, -1.0, 1.0)) / 3.1415926 + 0.5);
	float twinkle = 0.75 + 0.25 * sin(time * 0.55 + dir.x * 43.0 + dir.z * 31.0);
	float denseStars =
		starLayer(starUv + vec2(0.0, time * 0.0002), 180.0, 0.962, 1.10) * 1.10 +
		starLayer(starUv + vec2(0.17, -0.11), 260.0, 0.972, 0.92) * 1.00 +
		starLayer(starUv + vec2(-0.23, 0.09), 340.0, 0.982, 0.78) * 0.88 +
		starLayer(starUv + vec2(0.31, 0.27), 420.0, 0.989, 0.64) * 0.62;
	float hazeStars = smoothstep(0.66, 0.96, fbm(starUv * 95.0 + vec2(dir.y * 14.0, dir.x * 11.0))) * 0.30;
	float stars = (denseStars + hazeStars) * twinkle * 1.22;
	float starVisibility = smoothstep(-0.02, -0.48, sunHeight) * smoothstep(-0.28, 0.18, dir.y);

	vec3 nightBase = mix(vec3(0.008, 0.014, 0.038), vec3(0.05, 0.07, 0.13), clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
	float nightNebula = fbm(starUv * 18.0 + vec2(0.0, time * 0.00008));
	nightBase += vec3(0.03, 0.035, 0.06) * smoothstep(0.52, 0.88, nightNebula) * 0.45;
	nightBase += vec3(1.0, 0.98, 0.96) * stars * starVisibility * 1.10;
	proceduralNightColor = vec4(nightBase, 1.0);

	vec4 skyColor = mix(proceduralDayColor, proceduralNightColor, clamp(blendFactor, 0.0, 1.0));

	// During dusk, push the warm tint upward so it reads above the city silhouette.
	float duskFactor = smoothstep(-0.10, 0.20, sunHeight) * (1.0 - smoothstep(0.28, 0.72, sunHeight));
	float upperSky = smoothstep(0.18, 1.0, dir.y * 0.5 + 0.5);
	float lowerSky = 1.0 - upperSky;
	skyColor.rgb = mix(skyColor.rgb, skyColor.rgb * vec3(0.90, 0.94, 1.00), lowerSky * duskFactor * 0.35);
	skyColor.rgb += vec3(0.16, 0.05, 0.03) * duskFactor * pow(upperSky, 3.0);
	skyColor.rgb += vec3(0.08, 0.02, 0.01) * duskFactor * pow(upperSky, 1.6) * 0.55;

	float cloudBand = smoothstep(-0.12, 0.46, dir.y) * (1.0 - smoothstep(0.90, 0.995, dir.y));
	float dayCloudVisibility = (1.0 - clamp(blendFactor, 0.0, 1.0) * 0.75) * smoothstep(-0.26, 0.20, sunHeight);

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
		float coverage = mix(0.38, 0.46, remap01(sunHeight, -0.08, 0.45)) - (cloudCoverage - 0.5) * 0.28;
		float cloudMask = smoothstep(coverage, coverage + 0.14, cloudShape) * cloudBand;
		float cloudCore = smoothstep(coverage + 0.02, coverage + 0.18, cloudShape);
		float cloudRim = smoothstep(coverage - 0.02, coverage + 0.12, cloudShape) - cloudCore;

		float lightFacing = smoothstep(-0.18, 0.32, dot(dir, skySunDir));
		float warmSun = remap01(sunHeight, -0.02, 0.30);
		float highSun = remap01(sunHeight, 0.25, 0.85);

		vec3 highlightColor = mix(vec3(1.0, 0.86, 0.72), vec3(0.98, 0.98, 0.94), highSun);
		vec3 bodyColor = mix(vec3(0.82, 0.84, 0.90), vec3(0.95, 0.96, 0.98), highSun);
		vec3 shadowColor = mix(vec3(97.0, 98.0, 120.0) / 255.0, vec3(65.0, 70.0, 80.0) * (1.5 / 255.0), highSun);

		float selfShadow = remap01(cloudShape, coverage + 0.01, coverage + 0.14);
		vec3 cloudColor = mix(shadowColor, bodyColor, selfShadow);
		cloudColor = mix(cloudColor, highlightColor, cloudRim * (0.25 + lightFacing * 0.75) * (0.45 + warmSun * 0.55));

		float cloudOpacity = cloudMask * (0.88 + cloudCore * 0.24) * dayCloudVisibility * densityBoost;
		cloudOpacity = clamp(cloudOpacity, 0.0, 1.0);
		skyColor.rgb = mix(skyColor.rgb, cloudColor, cloudOpacity);
		skyColor.rgb += highlightColor * cloudRim * lightFacing * 0.28 * dayCloudVisibility;
	}

	FragColor = vec4(clamp(skyColor.rgb, 0.0, 1.0), skyColor.a);
}
