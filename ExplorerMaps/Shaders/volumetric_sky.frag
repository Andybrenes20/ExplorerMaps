#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform vec3 skyColorBottom;
uniform vec3 skyColorTop;
uniform vec3 lightDirection;
uniform vec2 resolution;
uniform float time;
uniform float sunHeight;
uniform float rainIntensity;
uniform mat4 inv_proj;
uniform mat4 inv_view;

#define SUN_DIR lightDirection

void raySphereintersectionSkyMap(vec3 rd, float radius, out vec3 startPos)
{
	float radius2 = radius * radius;
	vec3 L = vec3(0.0);
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius2;
	float discr = b * b - 4.0 * a * c;
	float t = max(0.0, (-b + sqrt(discr)) / 2.0);
	startPos = rd * t;
}

vec3 computeClipSpaceCoord(ivec2 fragCoord)
{
	vec2 ray_nds = 2.0 * vec2(fragCoord.xy) / resolution.xy - 1.0;
	return vec3(ray_nds, 1.0);
}

vec3 getSun(const vec3 d, float powExp)
{
	float sun = clamp(dot(SUN_DIR, d), 0.0, 1.0);
	return 0.8 * vec3(1.0, 0.6, 0.1) * pow(sun, powExp);
}

float hash21(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float noise2(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm2(vec2 p)
{
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 3; ++i)
	{
		v += noise2(p) * a;
		p = p * 2.07 + vec2(9.2, -4.7);
		a *= 0.5;
	}
	return v;
}

float starLayer(vec2 p, float scale, float threshold, float sizeBoost)
{
	vec2 cell = floor(p * scale);
	vec2 local = fract(p * scale) - 0.5;
	float seed = hash21(cell);
	float active = smoothstep(threshold, 1.0, seed);
	float radius = length(local);
	return active * smoothstep(0.052 * sizeBoost, 0.0, radius);
}

vec4 colorCubeMap(vec3 endPos, const vec3 d)
{
	float skyBlend = clamp(1.0 - exp(8.5 - 17.0 * clamp(normalize(d).y * 0.5 + 0.5, 0.0, 1.0)), 0.0, 1.0);
	vec3 col = mix(skyColorBottom, skyColorTop, skyBlend);
	col += getSun(d, 350.0);

	vec2 starUv = vec2(atan(d.z, d.x) / 6.2831853 + 0.5, asin(clamp(d.y, -1.0, 1.0)) / 3.1415926 + 0.5);
	float twinkle = 0.76 + 0.24 * sin(time * 0.70 + d.x * 43.0 + d.z * 31.0);
	float stars =
		starLayer(starUv + vec2(0.0, time * 0.0002), 180.0, 0.962, 1.10) * 1.08 +
		starLayer(starUv + vec2(0.19, -0.13), 265.0, 0.974, 0.92) +
		starLayer(starUv + vec2(-0.27, 0.08), 380.0, 0.985, 0.70) * 0.82;
	float hazeStars = smoothstep(0.66, 0.95, fbm2(starUv * 90.0 + vec2(d.y * 12.0, d.x * 9.0))) * 0.22;
	float nightAmount = 1.0 - smoothstep(-0.45, -0.02, sunHeight);
	float starVisibility = nightAmount * smoothstep(-0.22, 0.24, d.y) * (1.0 - clamp(rainIntensity, 0.0, 1.0) * 0.72);
	col += vec3(1.0, 0.98, 0.92) * (stars + hazeStars) * twinkle * starVisibility;

	return vec4(col, 1.0);
}

void main()
{
	ivec2 fragCoord = ivec2(gl_FragCoord.xy);

	vec4 ray_clip = vec4(computeClipSpaceCoord(fragCoord), 1.0);
	vec4 ray_view = inv_proj * ray_clip;
	ray_view = vec4(ray_view.xy, -1.0, 0.0);
	vec3 worldDir = normalize((inv_view * ray_view).xyz);

	vec3 cubeMapEndPos;
	raySphereintersectionSkyMap(worldDir, 0.5, cubeMapEndPos);

	FragColor = vec4(colorCubeMap(cubeMapEndPos, worldDir).rgb, 1.0);
}
