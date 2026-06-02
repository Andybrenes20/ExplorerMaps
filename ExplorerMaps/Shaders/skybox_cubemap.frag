#version 330 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube daySkybox;
uniform samplerCube nightSkybox;
uniform float blendFactor;
uniform float time;
uniform float sunHeight;
uniform vec3 sunDir;
uniform vec3 moonDir;
uniform float cloudCoverage;
uniform float cloudSpeed;
uniform float cloudCrispiness;
uniform float cloudCurliness;
uniform float cloudDensity;
uniform float cloudLightAbsorption;
uniform float cloudDomeRadius;
uniform float cloudBottom;
uniform float cloudTop;
uniform float cloudFrequency;
uniform vec3 cloudColor;
uniform float rainIntensity;
uniform float lightningIntensity;
uniform float lightningSeed;
uniform vec3 cameraPosition;
uniform vec2 resolution;
uniform bool useProceduralClouds = true;

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
	for (int i = 0; i < 3; ++i)
	{
		value += noise(p) * amplitude;
		p = p * 2.03 + vec2(13.1, 7.7);
		amplitude *= 0.5;
	}
	return value;
}

float hash3(vec3 p)
{
	p = fract(p * vec3(0.1031, 0.11369, 0.13787));
	p += dot(p, p.yzx + 19.19);
	return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	float n000 = hash3(i + vec3(0.0, 0.0, 0.0));
	float n100 = hash3(i + vec3(1.0, 0.0, 0.0));
	float n010 = hash3(i + vec3(0.0, 1.0, 0.0));
	float n110 = hash3(i + vec3(1.0, 1.0, 0.0));
	float n001 = hash3(i + vec3(0.0, 0.0, 1.0));
	float n101 = hash3(i + vec3(1.0, 0.0, 1.0));
	float n011 = hash3(i + vec3(0.0, 1.0, 1.0));
	float n111 = hash3(i + vec3(1.0, 1.0, 1.0));

	float x00 = mix(n000, n100, f.x);
	float x10 = mix(n010, n110, f.x);
	float x01 = mix(n001, n101, f.x);
	float x11 = mix(n011, n111, f.x);
	float y0 = mix(x00, x10, f.y);
	float y1 = mix(x01, x11, f.y);
	return mix(y0, y1, f.z);
}

float fbm3(vec3 p)
{
	float value = 0.0;
	float amplitude = 0.54;
	for (int i = 0; i < 3; ++i)
	{
		value += noise3(p) * amplitude;
		p = p * 2.02 + vec3(11.7, 5.8, 8.3);
		amplitude *= 0.52;
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

float SkyLightningBolt(vec2 uv, float seed)
{
	float skyMask = smoothstep(0.18, 0.58, uv.y);
	float baseX = mix(0.12, 0.88, hash(vec2(seed, seed + 1.7)));
	float yStart = 0.94;
	float yEnd = mix(0.36, 0.70, hash(vec2(seed + 4.3, seed + 9.1)));
	float activeY = smoothstep(yEnd, yEnd + 0.04, uv.y) * (1.0 - smoothstep(yStart - 0.04, yStart, uv.y));

	float x = baseX;
	x += sin((1.0 - uv.y) * 24.0 + seed * 18.0) * 0.030;
	x += sin((1.0 - uv.y) * 66.0 + seed * 7.0) * 0.012;

	float wrappedDistance = abs(uv.x - x);
	wrappedDistance = min(wrappedDistance, 1.0 - wrappedDistance);
	float pixelDistance = wrappedDistance * max(resolution.x, 1.0);
	float core = exp(-pixelDistance * pixelDistance * 0.070) * activeY;
	float glow = exp(-pixelDistance * pixelDistance * 0.010) * activeY * 0.30;

	float branchSide = mix(-1.0, 1.0, hash(vec2(seed + 12.0, 0.4)));
	float branchX = fract(x + branchSide * 0.13 * smoothstep(0.48, 0.74, uv.y));
	float branchDistance = abs(uv.x - branchX);
	branchDistance = min(branchDistance, 1.0 - branchDistance);
	float branchMask = smoothstep(0.42, 0.58, uv.y) * (1.0 - smoothstep(0.72, 0.86, uv.y));
	float branch = exp(-pow(branchDistance * max(resolution.x, 1.0), 2.0) * 0.080) * branchMask * 0.50;

	return (core + glow + branch) * skyMask;
}

vec4 renderRepoStyleClouds(vec3 dir, vec3 skySunDir, vec3 skyMoonDir, vec3 cameraPos, float dayCloudVisibility, float nightAmount, float goldenHour, float rain)
{
	float horizonFade = smoothstep(-0.02, 0.16, dir.y);
	float zenithFade = 1.0 - smoothstep(0.94, 1.0, dir.y);
	float cloudBand = horizonFade * zenithFade;
	if (cloudBand <= 0.001)
		return vec4(0.0);

	float speed = max(cloudSpeed, 0.05);
	float crispiness = max(cloudCrispiness, 0.28);
	float curliness = max(cloudCurliness, 0.10);
	float densityBoost = max(cloudDensity, 0.20);
	vec2 windDir = normalize(vec2(0.92, 0.22));

	vec2 wind = windDir * time * (0.050 + 0.010 * rain) * speed;
	float coverage = mix(0.68, 0.30, clamp((cloudCoverage - 0.15) / 1.05, 0.0, 1.0));
	coverage = mix(coverage, 0.44, rain * 0.80);
	float softness = mix(0.26, 0.075, clamp(crispiness / 2.5, 0.0, 1.0));
	softness = mix(softness, 0.34, rain);
	float dayVis = max(dayCloudVisibility, rain * 0.90);
	float nightVis = nightAmount * (0.25 + rain * 0.40);
	float visibility = clamp(dayVis + nightVis, 0.0, 1.0);
	if (visibility <= 0.01)
		return vec4(0.0);

	float skyHeight = clamp((dir.y + 0.10) / 1.10, 0.0, 1.0);
	float layerBottom = max(cloudBottom, 900.0);
	float layerTop = max(layerBottom + 900.0, cloudTop);
	float primaryLayerHeight = mix(layerBottom, layerTop, 0.38);
	float rayUp = max(dir.y, 0.055);
	float t = max((primaryLayerHeight - cameraPos.y) / rayUp, 0.0);
	vec3 cloudHit = cameraPos + dir * t;
	vec2 worldUv = cloudHit.xz * (0.00018 + cloudFrequency * 0.00013);
	worldUv += dir.xz * mix(0.35, 0.06, skyHeight);
	vec2 skyUv = vec2(worldUv.x * 1.18 + worldUv.y * 0.18, worldUv.y * 0.88 - worldUv.x * 0.08);
	float bottomNorm = clamp(cloudBottom / max(cloudDomeRadius, 1.0), 0.0, 0.12);
	float topNorm = clamp(cloudTop / max(cloudDomeRadius, 1.0), bottomNorm + 0.005, 0.18);
	float lowerFade = smoothstep(0.00 + bottomNorm * 3.5, 0.13 + bottomNorm * 4.3, skyHeight);
	float upperFade = 1.0 - smoothstep(0.84 + topNorm * 1.1, 1.0, skyHeight);
	float domeBand = lowerFade * upperFade;

	vec2 drift = windDir * time * (0.038 + rain * 0.010) * speed;
	vec2 highLayerDrift = vec2(-windDir.y, windDir.x) * time * (0.010 + rain * 0.004) * speed;
	vec2 curlUv = skyUv * max(cloudFrequency, 0.05) + drift * 0.70 + highLayerDrift * 0.25;
	vec2 curl = vec2(
		fbm(curlUv * 1.18 + vec2(3.2, 8.7)),
		fbm(curlUv * 1.05 + vec2(-6.5, 1.3))
	) - 0.5;
	vec2 shapedUv = skyUv * (0.70 + cloudFrequency * 0.62) + drift + highLayerDrift + curl * (0.48 + curliness * 0.50);

	float largeShape = fbm(shapedUv * 0.66 + wind * 0.12);
	float mediumShape = fbm(shapedUv * (1.44 + crispiness * 0.22) + vec2(12.5, 4.0) + wind * 0.22);
	float puffShape = fbm(shapedUv * (3.05 + crispiness * 0.58) + vec2(-7.0, 14.0) + highLayerDrift * 0.40);
	float erode = noise(shapedUv * (5.8 + curliness * 3.4) + vec2(2.0, 19.0) + wind * 0.50);
	float billow = fbm(vec2(shapedUv.x * 2.10 + worldUv.x * 0.22, shapedUv.y * 3.80 + worldUv.y * 0.14));
	float cauliflower = fbm3(vec3(shapedUv * (2.2 + crispiness * 0.35) + wind * 0.18, skyHeight * 4.5 + time * 0.055 * speed));
	float verticalPuff = smoothstep(0.06, 0.48, skyHeight) * (1.0 - smoothstep(0.72, 0.98, skyHeight));

	vec2 stratusUv = skyUv * (0.34 + cloudFrequency * 0.22) + drift * 0.42 - highLayerDrift * 0.20;
	float stratusLarge = fbm(stratusUv * 0.72 + vec2(4.0, -8.0));
	float stratusVeil = fbm(stratusUv * 1.38 + vec2(-11.0, 2.0));
	float stratusGeneration = stratusLarge * 0.72 + stratusVeil * 0.28;

	vec2 stratocumulusUv = skyUv * (0.92 + cloudFrequency * 0.42) + drift * 0.80 + highLayerDrift * 0.36;
	float cellularBase = fbm(stratocumulusUv * 0.85 + vec2(21.0, 3.0));
	float cellularBreakup = fbm(stratocumulusUv * 2.45 + vec2(-5.0, 16.0));
	float stratocumulusGeneration = cellularBase * 0.66 + cellularBreakup * 0.34;

	vec2 cumulusUv = skyUv * (1.45 + cloudFrequency * 0.70) + drift * 1.16 + curl * (0.62 + curliness * 0.35);
	float cumulusMass = fbm(cumulusUv * 0.78 + vec2(-13.0, 9.0));
	float cumulusTowers = fbm3(vec3(cumulusUv * 1.75 + wind * 0.25, skyHeight * 6.5 + time * 0.040 * speed));
	float cumulusErosion = fbm(cumulusUv * (4.2 + crispiness * 0.72) + vec2(8.0, -18.0) + highLayerDrift * 0.50);
	float cumulusGeneration = cumulusMass * 0.52 + cumulusTowers * 0.36 + billow * 0.18 - cumulusErosion * (0.10 + crispiness * 0.020);

	vec2 cirroUv = skyUv * vec2(3.6, 0.72) + highLayerDrift * 1.20 + wind * 0.10;
	float cirroStreaks = fbm(cirroUv + vec2(31.0, -6.0));
	float cirroDetail = fbm(vec2(cirroUv.x * 2.8, cirroUv.y * 0.42) + vec2(-7.0, 19.0));
	float cirrusGeneration = cirroStreaks * 0.72 + cirroDetail * 0.28;

	float fairWeatherShape = largeShape * 0.50 + mediumShape * 0.23 + puffShape * 0.15 + billow * verticalPuff * 0.20 + cauliflower * 0.13 - erode * (0.075 + crispiness * 0.026);
	float stormSheetShape = largeShape * 0.68 + mediumShape * 0.24 + billow * 0.08;
	float lowCumulusMask = smoothstep(0.12, 0.56, skyHeight) * (1.0 - smoothstep(0.72, 0.98, skyHeight));
	float midDeckMask = smoothstep(0.18, 0.62, skyHeight) * (1.0 - smoothstep(0.86, 1.0, skyHeight));
	float highVeilMask = smoothstep(0.44, 0.76, skyHeight) * (1.0 - smoothstep(0.97, 1.0, skyHeight));

	float cumulusCoverage = coverage + mix(0.05, -0.02, clamp(cloudCoverage, 0.0, 1.0));
	float stratocumulusCoverage = coverage - 0.025;
	float stratusCoverage = mix(0.70, 0.42, rain);
	float cirrusCoverage = 0.64 - cloudCoverage * 0.12;

	float cumulusLayer = smoothstep(cumulusCoverage, cumulusCoverage + softness * 0.74, cumulusGeneration) * lowCumulusMask;
	float stratocumulusLayer = smoothstep(stratocumulusCoverage, stratocumulusCoverage + softness * 1.12, stratocumulusGeneration) * midDeckMask;
	float stratusLayer = smoothstep(stratusCoverage, stratusCoverage + 0.30, stratusGeneration) * cloudBand * mix(0.22, 1.0, rain);
	float cirrusLayer = smoothstep(cirrusCoverage, cirrusCoverage + 0.18, cirrusGeneration) * highVeilMask * (1.0 - rain * 0.55);

	float terrainStyleMix = clamp(cumulusLayer * 0.58 + stratocumulusLayer * 0.48 + stratusLayer * 0.36 + cirrusLayer * 0.18, 0.0, 1.0);
	float cloudShape = mix(fairWeatherShape, stormSheetShape, rain * 0.78);

	float cloudBody = smoothstep(coverage, coverage + softness, cloudShape);
	float cloudCore = smoothstep(coverage + 0.08, coverage + softness + 0.12, cloudShape);
	float underside = 1.0 - smoothstep(0.18, 0.58, skyHeight);
	float alpha = cloudBody * cloudBand * domeBand * visibility * densityBoost * (0.72 + rain * 0.24);
	alpha += cloudCore * cloudBand * domeBand * visibility * densityBoost * 0.18;
	alpha += terrainStyleMix * cloudBand * domeBand * visibility * densityBoost * mix(0.42, 0.24, rain);
	float overcastAlpha = smoothstep(0.24, 0.78, stormSheetShape) * cloudBand * domeBand * visibility * (0.44 + densityBoost * 0.16);
	overcastAlpha += stratusLayer * domeBand * visibility * 0.24;
	alpha = mix(alpha, overcastAlpha, rain * 0.82);
	alpha = clamp(alpha, 0.0, mix(0.96, 0.72, rain));
	if (alpha <= 0.001)
		return vec4(0.0);

	float lightFacing = smoothstep(-0.18, 0.55, dot(dir, skySunDir));
	float moonFacing = smoothstep(0.15, 0.86, dot(dir, skyMoonDir));
	float highCloud = smoothstep(0.20, 0.82, skyHeight);
	vec3 userCloudColor = max(cloudColor, vec3(0.02));
	vec3 topDay = mix(vec3(0.96, 0.99, 0.94), vec3(1.00, 0.84, 0.58), goldenHour * 0.72);
	vec3 bottomDay = mix(userCloudColor, vec3(0.58, 0.44, 0.37), goldenHour * 0.38);
	vec3 dayColor = mix(bottomDay, topDay, highCloud * 0.62 + lightFacing * 0.38);
	dayColor = mix(dayColor, bottomDay, cloudCore * clamp(cloudLightAbsorption, 0.0, 1.0) * (0.75 + underside * 0.45));
	dayColor += vec3(0.22, 0.28, 0.34) * (puffShape - erode) * 0.08 * dayVis;
	vec3 generationTint = vec3(0.0);
	generationTint += vec3(0.10, 0.13, 0.17) * cumulusLayer * underside * 0.18;
	generationTint += vec3(0.05, 0.07, 0.09) * stratocumulusLayer * 0.10;
	generationTint += vec3(0.18, 0.20, 0.22) * stratusLayer * (0.10 + rain * 0.18);
	generationTint += vec3(0.92, 0.96, 1.00) * cirrusLayer * 0.18;
	dayColor = mix(dayColor, dayColor + generationTint, clamp(terrainStyleMix + cirrusLayer, 0.0, 1.0));
	vec3 nightColor = mix(vec3(0.045, 0.065, 0.120), vec3(0.18, 0.24, 0.42), highCloud);
	nightColor += vec3(0.10, 0.13, 0.22) * moonFacing * 0.32;
	vec3 stormColor = mix(vec3(0.33, 0.38, 0.43), vec3(0.62, 0.67, 0.70), highCloud * 0.55 + lightFacing * 0.18);
	stormColor = mix(stormColor, vec3(0.46, 0.51, 0.56), cloudCore * 0.55);
	vec3 cloudColor = mix(dayColor, nightColor, clamp(blendFactor, 0.0, 1.0));
	cloudColor = mix(cloudColor, stormColor, rain * 0.88);

	float sunEdge = pow(max(dot(dir, skySunDir), 0.0), 18.0) * dayVis * (1.0 - rain * 0.65);
	float silverLining = smoothstep(0.10, 0.75, alpha) * (1.0 - smoothstep(0.82, 1.0, alpha)) * sunEdge;
	float broadSunBloom = pow(max(dot(dir, skySunDir), 0.0), 4.2) * dayVis * (1.0 - rain * 0.80);
	float moonLining = pow(max(dot(dir, skyMoonDir), 0.0), 22.0) * nightVis * (1.0 - rain * 0.55);
	cloudColor += mix(vec3(1.0, 0.76, 0.42), vec3(1.0), remap01(sunHeight, 0.20, 0.82)) * silverLining * 0.58 * (1.0 - rain * 0.90);
	cloudColor += vec3(1.0, 0.92, 0.66) * broadSunBloom * cloudBody * 0.055 * (1.0 - rain * 0.85);
	cloudColor += vec3(0.20, 0.28, 0.48) * moonLining * 0.24;

	return vec4(cloudColor, alpha);
}

void main()
{
	vec3 dir = normalize(TexCoords);
	vec3 skySunDir = normalize(sunDir);
	float rain = clamp(rainIntensity, 0.0, 1.0);
	float skyVertical = smoothstep(0.0, 1.0, clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
	vec4 dayColor = vec4(mix(vec3(0.70, 0.82, 0.96), vec3(0.32, 0.58, 0.92), skyVertical), 1.0);
	vec4 nightColor = vec4(mix(vec3(0.020, 0.030, 0.075), vec3(0.005, 0.010, 0.035), skyVertical), 1.0);
	vec4 proceduralDayColor = dayColor;
	vec4 proceduralNightColor = nightColor;
	float goldenHour = 0.0;

	if (useRepoInspiredDaySky)
	{
		float highSunBlend = remap01(sunHeight, 0.18, 0.82);
		vec3 highSunTop = vec3(0.27, 0.53, 0.90);
		vec3 highSunBottom = vec3(0.68, 0.82, 0.98);
		vec3 sunsetTop = vec3(133.0, 158.0, 214.0) / 255.0;
		vec3 sunsetBottom = vec3(250.0, 181.0, 118.0) / 255.0;

		vec3 skyTop = mix(sunsetTop, highSunTop, highSunBlend);
		vec3 skyBottom = mix(sunsetBottom, highSunBottom, highSunBlend);
		float gradient = smoothstep(0.0, 1.0, skyVertical);
		vec3 proceduralSky = mix(skyBottom, skyTop, gradient);
		float sunAlign = max(dot(dir, skySunDir), 0.0);
		float sunGlow = pow(sunAlign, 52.0) * smoothstep(-0.12, 0.45, sunHeight) * (1.0 - rain * 0.86);
		float horizonHaze = 1.0 - smoothstep(0.06, 0.38, skyVertical);
		float duskWarmth = (1.0 - highSunBlend) * smoothstep(-0.16, 0.24, sunHeight);
		goldenHour = (1.0 - highSunBlend) * smoothstep(0.10, 0.68, sunHeight) * (1.0 - rain * 0.65);
		float wideSunGlow = pow(sunAlign, 28.0) * smoothstep(-0.10, 0.55, sunHeight) * (1.0 - rain * 0.80);
		float dayPresence = smoothstep(-0.08, 0.38, sunHeight) * (1.0 - rain * 0.70);
		float sunCorona = pow(sunAlign, 128.0) * dayPresence * 0.25;
		float sunDisc = smoothstep(0.99925, 0.99978, sunAlign) * dayPresence;
		float sunCore = smoothstep(0.99978, 0.99996, sunAlign) * dayPresence;
		vec3 sunDiscColor = mix(vec3(1.0, 0.52, 0.15), vec3(1.0, 0.88, 0.48), highSunBlend);
		proceduralSky += mix(vec3(1.0, 0.58, 0.36), vec3(1.0, 0.80, 0.52), highSunBlend) * sunGlow * 0.32;
		proceduralSky += vec3(0.26, 0.12, 0.065) * horizonHaze * duskWarmth * 0.36;
		proceduralSky += vec3(1.0, 0.68, 0.32) * wideSunGlow * goldenHour * 0.10;
		proceduralSky += sunDiscColor * sunCorona;
		proceduralSky += sunDiscColor * sunDisc * 1.15 + vec3(1.0, 0.96, 0.72) * sunCore * 0.95;
		proceduralSky = mix(proceduralSky, vec3(0.95, 0.77, 0.52), horizonHaze * goldenHour * 0.48);
		proceduralSky = mix(proceduralSky, vec3(0.74, 0.86, 0.98), horizonHaze * 0.20 * highSunBlend);
		vec2 skyMapUv = vec2(
			atan(dir.z, dir.x) / 6.2831853 + 0.5,
			asin(clamp(dir.y, -1.0, 1.0)) / 3.1415926 + 0.5
		);
		float cirrusBand = smoothstep(0.28, 0.64, skyVertical) * (1.0 - smoothstep(0.94, 1.0, skyVertical));
		vec2 cirrusUv = vec2(skyMapUv.x * 8.6, skyMapUv.y * 2.8) + time * vec2(0.022, 0.0045) * max(cloudSpeed, 0.15);
		float cirrusShape = fbm(vec2(cirrusUv.x * 1.85, cirrusUv.y * 0.36));
		float cirrusDetail = fbm(vec2(cirrusUv.x * 5.20 + 17.0, cirrusUv.y * 0.82 - 6.0));
		float cirrus = smoothstep(0.50, 0.86, cirrusShape * 0.68 + cirrusDetail * 0.42);
		float cirrusOpacity = cirrus * cirrusBand * dayPresence * clamp(cloudDensity * 0.17, 0.12, 0.34);
		vec3 cirrusColor = mix(vec3(0.88, 0.94, 1.0), vec3(1.0, 0.82, 0.55), goldenHour * 0.75);
		proceduralSky = mix(proceduralSky, cirrusColor, cirrusOpacity);
		vec3 stormSky = mix(vec3(0.18, 0.21, 0.26), vec3(0.31, 0.36, 0.43), clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
		stormSky += vec3(0.05, 0.055, 0.06) * horizonHaze;
		proceduralSky = mix(proceduralSky, stormSky, rain * 0.76);
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

	float nightAmount = remap01(-sunHeight, 0.08, 0.92) * (1.0 - rain * 0.55);
	vec3 nightBase = mix(vec3(0.006, 0.012, 0.036), vec3(0.045, 0.070, 0.145), clamp(dir.y * 0.5 + 0.5, 0.0, 1.0));
	float nightNebula = fbm(starUv * 18.0 + vec2(0.0, time * 0.00008));
	float upperNight = smoothstep(0.36, 1.0, dir.y * 0.5 + 0.5);
	vec3 skyMoonDir = normalize(moonDir);
	float moonDot = max(dot(dir, skyMoonDir), 0.0);
	float moonHalo = pow(moonDot, 46.0) * nightAmount * (1.0 - rain * 0.65);
	float moonDisc = smoothstep(0.99910, 0.99972, moonDot) * nightAmount * (1.0 - rain * 0.40);
	float moonCore = smoothstep(0.99970, 0.99994, moonDot) * nightAmount * (1.0 - rain * 0.35);
	float moonSurface = fbm(starUv * 230.0 + vec2(19.0, 7.0));
	vec3 moonDiscColor = mix(vec3(0.58, 0.62, 0.72), vec3(0.94, 0.91, 0.78), moonSurface);
	float horizonNightGlow = (1.0 - smoothstep(0.0, 0.32, abs(dir.y))) * nightAmount;
	nightBase += vec3(0.030, 0.040, 0.075) * smoothstep(0.52, 0.88, nightNebula) * 0.60 * nightAmount;
	nightBase += vec3(0.070, 0.105, 0.220) * moonHalo * 0.34;
	nightBase += moonDiscColor * moonDisc * 0.95;
	nightBase += vec3(1.0, 0.97, 0.82) * moonCore * 0.72;
	nightBase += vec3(0.040, 0.055, 0.110) * horizonNightGlow;
	nightBase = mix(nightBase, nightBase * vec3(0.90, 0.98, 1.18), upperNight * nightAmount * 0.25);
	nightBase += vec3(1.0, 0.98, 0.96) * stars * starVisibility * 1.10;
	proceduralNightColor = vec4(nightBase, 1.0);

	vec4 skyColor = mix(proceduralDayColor, proceduralNightColor, clamp(blendFactor, 0.0, 1.0));
	float cityLightDome = (1.0 - smoothstep(0.0, 0.28, abs(dir.y))) * clamp(blendFactor, 0.0, 1.0);
	skyColor.rgb += vec3(0.16, 0.08, 0.045) * cityLightDome * 0.30;
	skyColor.rgb += vec3(0.025, 0.040, 0.090) * cityLightDome * nightAmount * 0.45;
	skyColor.rgb = mix(skyColor.rgb, vec3(0.10, 0.12, 0.16), rain * clamp(blendFactor, 0.0, 1.0) * 0.52);

	float dayCloudVisibility = (1.0 - clamp(blendFactor, 0.0, 1.0) * 0.75) * smoothstep(-0.26, 0.20, sunHeight);
	dayCloudVisibility = max(dayCloudVisibility, rain * 0.82);

	if (useProceduralClouds)
	{
		vec4 volumetricClouds = renderRepoStyleClouds(dir, skySunDir, skyMoonDir, cameraPosition, dayCloudVisibility, nightAmount, goldenHour, rain);
		skyColor.rgb = mix(skyColor.rgb, volumetricClouds.rgb, volumetricClouds.a);
	}

	float lightning = clamp(lightningIntensity, 0.0, 1.35);
	if (lightning > 0.001)
	{
		float vertical = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
		float skyFlash = lightning * smoothstep(0.08, 0.82, vertical);
		float bolt = SkyLightningBolt(starUv, lightningSeed) * lightning;
		skyColor.rgb = mix(skyColor.rgb, vec3(0.68, 0.78, 0.96), clamp(skyFlash * 0.55, 0.0, 1.0));
		skyColor.rgb += vec3(0.24, 0.31, 0.45) * skyFlash * 0.36;
		skyColor.rgb += vec3(0.90, 0.97, 1.0) * bolt * 1.15;
	}

	FragColor = vec4(clamp(skyColor.rgb, 0.0, 1.0), skyColor.a);
}
