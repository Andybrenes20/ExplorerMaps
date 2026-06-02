#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform float time;
uniform float rainIntensity;
uniform float nightFactor;
uniform float lightningIntensity;
uniform float lightningSeed;
uniform vec2 resolution;

float Hash21(vec2 p)
{
	p = fract(p * vec2(123.34, 345.45));
	p += dot(p, p + 34.345);
	return fract(p.x * p.y);
}

float FineRain(vec2 uv, float density, float speed, float slant, float lengthScale)
{
	float aspect = resolution.x / max(resolution.y, 1.0);
	vec2 p = vec2(uv.x * aspect, uv.y);
	p.x += p.y * slant;
	p.y += time * speed;

	vec2 grid = p * vec2(density, density * 0.58);
	vec2 cell = floor(grid);
	vec2 local = fract(grid) - 0.5;
	float seed = Hash21(cell);
	local.x += (seed - 0.5) * 0.62;

	float diagonal = abs(local.x + local.y * 0.30);
	float line = 1.0 - smoothstep(0.0, 0.018, diagonal);
	float dash = 1.0 - smoothstep(0.10, 0.42 * lengthScale, abs(local.y));
	float visible = smoothstep(0.42, 0.92, seed);
	return line * dash * visible;
}

vec3 UrbanGlow(vec2 uv)
{
	float horizonBand = smoothstep(0.12, 0.56, uv.y) * (1.0 - smoothstep(0.88, 1.0, uv.y));
	vec2 aspect = vec2(resolution.x / max(resolution.y, 1.0), 1.0);
	vec3 glow = vec3(0.0);

	vec2 p0 = (uv - vec2(0.18, 0.36)) * aspect * vec2(2.8, 1.3);
	vec2 p1 = (uv - vec2(0.36, 0.48)) * aspect * vec2(3.2, 1.2);
	vec2 p2 = (uv - vec2(0.62, 0.46)) * aspect * vec2(3.0, 1.1);
	vec2 p3 = (uv - vec2(0.82, 0.40)) * aspect * vec2(2.7, 1.2);

	glow += vec3(1.0, 0.58, 0.24) * exp(-dot(p0, p0) * 2.1);
	glow += vec3(1.0, 0.72, 0.32) * exp(-dot(p1, p1) * 2.4);
	glow += vec3(0.20, 0.58, 1.0) * exp(-dot(p2, p2) * 2.6);
	glow += vec3(1.0, 0.35, 0.16) * exp(-dot(p3, p3) * 2.2);

	return glow * horizonBand;
}

float LightningBolt(vec2 uv, float seed)
{
	float skyMask = smoothstep(0.18, 0.58, uv.y);
	float baseX = mix(0.18, 0.82, Hash21(vec2(seed, seed + 2.1)));
	float yStart = 0.96;
	float yEnd = mix(0.36, 0.68, Hash21(vec2(seed + 7.0, seed + 3.0)));
	float activeY = smoothstep(yEnd, yEnd + 0.04, uv.y) * (1.0 - smoothstep(yStart - 0.03, yStart, uv.y));

	float x = baseX;
	x += sin((1.0 - uv.y) * 22.0 + seed * 18.0) * 0.035;
	x += sin((1.0 - uv.y) * 61.0 + seed * 7.0) * 0.014;

	float pixelDistance = abs(uv.x - x) * max(resolution.x, 1.0);
	float core = exp(-pixelDistance * pixelDistance * 0.075) * activeY;
	float glow = exp(-pixelDistance * pixelDistance * 0.010) * activeY * 0.34;

	float branchSide = mix(-1.0, 1.0, Hash21(vec2(seed + 12.0, 0.4)));
	float branchProgress = smoothstep(0.48, 0.74, uv.y);
	float branchX = x + branchSide * 0.16 * branchProgress;
	float branchMask = smoothstep(0.42, 0.58, uv.y) * (1.0 - smoothstep(0.72, 0.86, uv.y));
	float branchDistance = abs(uv.x - branchX) * max(resolution.x, 1.0);
	float branch = exp(-branchDistance * branchDistance * 0.080) * branchMask * 0.55;

	return (core + glow + branch) * skyMask;
}

void main()
{
	float rain = clamp(rainIntensity, 0.0, 1.0);
	float night = clamp(nightFactor, 0.0, 1.0);
	float lightning = clamp(lightningIntensity, 0.0, 1.35);
	vec2 uv = TexCoords;

	float fineRain = FineRain(uv, 145.0, 1.90, 0.42, 0.72);
	float midRain = FineRain(uv + vec2(0.19, 0.11), 88.0, 2.35, 0.48, 0.88);
	float nearRain = FineRain(uv + vec2(-0.27, 0.23), 52.0, 2.85, 0.54, 1.0);
	float streaks = fineRain * 0.38 + midRain * 0.46 + nearRain * 0.34 * rain;

	float vignette = smoothstep(0.92, 0.18, length(uv - 0.5));
	float mist = ((1.0 - vignette) * 0.08 + smoothstep(0.0, 0.70, uv.y) * 0.025) * rain;
	vec3 urbanGlow = UrbanGlow(uv) * (0.08 + rain * 0.18) * night;

	vec3 rainColor = vec3(0.78, 0.86, 0.96) * streaks;
	vec3 hazeColor = vec3(0.58, 0.66, 0.74) * mist;
	float bolt = LightningBolt(uv, lightningSeed) * lightning;
	float skyFlash = lightning * smoothstep(0.08, 0.82, uv.y);
	vec3 lightningColor =
		vec3(0.72, 0.86, 1.0) * skyFlash * 0.42 +
		vec3(0.92, 0.97, 1.0) * bolt * 1.70;

	float alpha = clamp(streaks * 0.20 * rain + mist * 0.10 + length(urbanGlow) * 0.08, 0.0, 0.34);
	alpha = clamp(alpha + skyFlash * 0.20 + bolt * 0.72, 0.0, 0.78);
	FragColor = vec4(rainColor + hazeColor + urbanGlow + lightningColor, alpha);
}
