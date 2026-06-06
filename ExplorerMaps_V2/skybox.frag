#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform float blendFactor;
uniform float time;
uniform float sunHeight;
uniform vec3 sunDir;
uniform vec3 moonDir;
uniform float rainIntensity;
uniform float fogIntensity;
uniform float lightningAmount;
uniform float lightningSeed;
uniform vec3 cameraPosition;
uniform vec2 resolution;
uniform float cloudCoverage;
uniform float cloudSpeed;
uniform float cloudDensity;
uniform float cloudCrispiness;
uniform vec3 cloudColor;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; ++i) {
        v += noise(p) * a;
        p = p * 2.03 + vec2(11.7, 5.3);
        a *= 0.5;
    }
    return v;
}

float remap(float v, float a, float b) {
    return clamp((v - a) / (b - a), 0.0, 1.0);
}

float starField(vec2 uv, float scale, float threshold) {
    vec2 cell = floor(uv * scale);
    vec2 local = fract(uv * scale) - 0.5;
    float seed = hash(cell);
    float core = 1.0 - smoothstep(0.0, 0.055, length(local));
    return core * smoothstep(threshold, 1.0, seed);
}

vec4 renderClouds(vec3 dir, vec3 sunDirection, float nightAmount, float rain) {
    float band = smoothstep(-0.02, 0.16, dir.y) * (1.0 - smoothstep(0.92, 1.0, dir.y));
    if (band <= 0.001) {
        return vec4(0.0);
    }

    vec2 wind = normalize(vec2(0.92, 0.22)) * time * (0.026 + rain * 0.018) * max(cloudSpeed, 0.05);
    float layerHeight = 440.0;
    float rayUp = max(dir.y, 0.045);
    vec3 hit = cameraPosition + dir * max((layerHeight - cameraPosition.y) / rayUp, 0.0);
    vec2 uv = hit.xz * 0.0021 + dir.xz * 0.12 + wind;
    vec2 curl = vec2(fbm(uv * 1.6 + 7.0), fbm(uv * 1.3 - 5.0)) - 0.5;
    uv += curl * mix(0.12, 0.34, cloudCrispiness);

    float large = fbm(uv * 0.62);
    float medium = fbm(uv * 1.45 + 8.0);
    float detail = fbm(uv * 3.25 - 4.0);
    float erosion = fbm(uv * 5.10 + vec2(-7.0, 13.0));
    float shape = large * 0.54 + medium * 0.34 + detail * 0.18 - erosion * 0.10;
    float threshold = mix(0.72, 0.34, clamp(cloudCoverage, 0.0, 1.0));
    float softness = mix(0.20, 0.065, clamp(cloudCrispiness, 0.0, 1.0));
    float body = smoothstep(threshold, threshold + softness, shape);
    float core = smoothstep(threshold + 0.08, threshold + softness + 0.13, shape);
    float alpha = clamp((body + core * 0.22) * band * cloudDensity, 0.0, mix(0.92, 0.76, rain));

    float high = remap(dir.y, 0.0, 0.82);
    float facingSun = smoothstep(-0.20, 0.78, dot(dir, sunDirection));
    float lightSample = fbm((uv - sunDirection.xz * 0.18) * 1.15 + 3.0);
    float selfShadow = smoothstep(0.42, 0.76, lightSample) * core;
    float edge = clamp(body - core, 0.0, 1.0);
    vec3 cloudBase = mix(vec3(0.52, 0.56, 0.62), vec3(0.76, 0.80, 0.84), high);
    vec3 cloudLight = mix(vec3(1.0, 0.72, 0.42), vec3(1.0, 0.98, 0.90), remap(sunDirection.y, 0.08, 0.72));
    vec3 dayCloud = mix(cloudBase, cloudLight, high * 0.34 + facingSun * 0.26 + edge * facingSun * 0.42);
    dayCloud *= 1.0 - selfShadow * 0.22;
    dayCloud = mix(dayCloud, vec3(0.40, 0.45, 0.52), rain * 0.72);
    float moonFacing = smoothstep(-0.35, 0.72, dot(dir, normalize(moonDir)));
    vec3 clearNightCloud = mix(vec3(0.025, 0.035, 0.065), vec3(0.10, 0.13, 0.20), high);
    vec3 rainyNightCloud = mix(vec3(0.018, 0.075, 0.18), vec3(0.08, 0.42, 0.78), high);
    vec3 nightCloud = mix(clearNightCloud, rainyNightCloud, rain * 0.78);
    nightCloud += mix(vec3(0.035, 0.045, 0.075), vec3(0.03, 0.20, 0.48), rain) * (edge * 0.55 + moonFacing * edge * 0.45);
    nightCloud *= 0.72 + core * 0.52;
    vec3 stormCloud = mix(vec3(0.28, 0.32, 0.38), vec3(0.55, 0.59, 0.63), high * 0.65 + facingSun * 0.18);
    vec3 clouds = mix(dayCloud, nightCloud, nightAmount);
    clouds = mix(clouds, stormCloud, rain * 0.84);

    return vec4(clouds, alpha);
}

float lightningBolt(vec2 uv, float seed) {
    float baseX = mix(0.14, 0.86, hash(vec2(seed, seed + 4.0)));
    float yMask = smoothstep(0.30, 0.44, uv.y) * (1.0 - smoothstep(0.88, 0.98, uv.y));
    float x = baseX + sin(uv.y * 42.0 + seed * 19.0) * 0.026 + sin(uv.y * 91.0) * 0.010;
    float d = abs(uv.x - x);
    d = min(d, 1.0 - d) * max(resolution.x, 1.0);
    return (exp(-d * d * 0.08) + exp(-d * d * 0.012) * 0.30) * yMask;
}

void main() {
    vec3 dir = normalize(TexCoords);
    vec3 sDir = normalize(sunDir);
    vec3 mDir = normalize(moonDir);
    float rain = clamp(rainIntensity, 0.0, 1.0);
    float fog = clamp(fogIntensity, 0.0, 1.0);
    float night = clamp(blendFactor, 0.0, 1.0);
    float vertical = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    float noon = remap(sunHeight, 0.15, 0.82);
    float dusk = (1.0 - noon) * smoothstep(-0.18, 0.28, sunHeight);
    vec3 dayBottom = mix(vec3(0.96, 0.65, 0.36), vec3(0.70, 0.84, 0.98), noon);
    vec3 dayTop = mix(vec3(0.50, 0.58, 0.78), vec3(0.25, 0.52, 0.92), noon);
    vec3 daySky = mix(dayBottom, dayTop, smoothstep(0.0, 1.0, vertical));
    float sunDot = max(dot(dir, sDir), 0.0);
    float sunPresence = smoothstep(-0.08, 0.42, sunHeight) * (1.0 - rain * 0.80);
    daySky += vec3(1.0, 0.65, 0.32) * pow(sunDot, 30.0) * dusk * 0.26;
    daySky += mix(vec3(1.0, 0.52, 0.18), vec3(1.0, 0.88, 0.48), noon) * smoothstep(0.9992, 0.99986, sunDot) * sunPresence * 1.25;
    daySky = mix(daySky, vec3(0.23, 0.27, 0.32), rain * 0.70);

    vec2 skyUv = vec2(atan(dir.z, dir.x) / 6.2831853 + 0.5, asin(clamp(dir.y, -1.0, 1.0)) / 3.1415926 + 0.5);
    float nightAmount = remap(-sunHeight, 0.05, 0.90) * (1.0 - rain * 0.50);
    vec3 clearNightSky = mix(vec3(0.004, 0.008, 0.022), vec3(0.020, 0.032, 0.070), vertical);
    vec3 rainyNightSky = mix(vec3(0.003, 0.016, 0.060), vec3(0.018, 0.105, 0.265), vertical);
    vec3 nightSky = mix(clearNightSky, rainyNightSky, rain * 0.82);
    float galacticBand = fbm(skyUv * vec2(4.0, 11.0) + vec2(time * 0.0015, 3.2));
    galacticBand = smoothstep(0.50, 0.78, galacticBand) * smoothstep(0.02, 0.62, dir.y);
    nightSky += mix(vec3(0.018, 0.025, 0.055), vec3(0.015, 0.12, 0.34), rain) * galacticBand * nightAmount;
    float twinkle = 0.78 + 0.22 * sin(time * 0.65 + dir.x * 43.0 + dir.z * 31.0);
    float stars = starField(skyUv, 180.0, 0.962) + starField(skyUv + 0.17, 290.0, 0.978) * 0.9 + starField(skyUv - 0.21, 430.0, 0.988) * 0.65;
    nightSky += vec3(0.92, 0.97, 1.0) * stars * twinkle * nightAmount * 1.35 * smoothstep(-0.22, 0.18, dir.y);
    float moonDot = max(dot(dir, mDir), 0.0);
    nightSky += vec3(0.20, 0.30, 0.55) * pow(moonDot, 42.0) * nightAmount;
    nightSky += vec3(0.95, 0.90, 0.76) * smoothstep(0.9990, 0.99978, moonDot) * nightAmount;
    nightSky += vec3(0.13, 0.07, 0.035) * (1.0 - smoothstep(0.0, 0.22, abs(dir.y))) * night;
    nightSky = mix(nightSky, vec3(0.08, 0.10, 0.14), rain * night * 0.56);

    vec3 sky = mix(daySky, nightSky, night);
    vec4 clouds = renderClouds(dir, sDir, nightAmount, rain);
    sky = mix(sky, clouds.rgb, clouds.a);

    float horizonHaze = 1.0 - smoothstep(0.015, 0.34, abs(dir.y));
    float hazeNoise = fbm(vec2(atan(dir.z, dir.x) * 2.2 + time * 0.003, dir.y * 9.0));
    vec3 dayHaze = mix(vec3(0.62, 0.76, 0.92), vec3(0.98, 0.55, 0.28), dusk * 0.72);
    vec3 nightHaze = mix(vec3(0.035, 0.045, 0.075), vec3(0.025, 0.12, 0.32), rain);
    vec3 atmosphericHaze = mix(dayHaze, nightHaze, night);
    float hazeAmount = horizonHaze * (0.08 + night * 0.07 + rain * 0.34 + fog * 0.34) * (0.82 + hazeNoise * 0.18);
    vec3 generatedFog = mix(vec3(0.64, 0.71, 0.79), vec3(0.07, 0.09, 0.13), night);
    atmosphericHaze = mix(atmosphericHaze, generatedFog, fog * 0.64);
    sky = mix(sky, atmosphericHaze, clamp(hazeAmount, 0.0, 0.54));

    // Keep the celestial bodies readable through the procedural cloud layer.
    float highSun = remap(sunHeight, 0.18, 0.82);
    float sunVisibility = smoothstep(-0.10, 0.34, sunHeight) * (1.0 - rain * 0.78);
    float sunCorona = pow(sunDot, 34.0) * sunVisibility;
    float sunDisc = smoothstep(0.99845, 0.99955, sunDot) * sunVisibility;
    float sunCore = smoothstep(0.99955, 0.99991, sunDot) * sunVisibility;
    vec3 sunColor = mix(vec3(1.0, 0.40, 0.10), vec3(1.0, 0.88, 0.48), highSun);
    sky += sunColor * sunCorona * (0.18 + dusk * 0.18);
    sky += sunColor * sunDisc * 1.18 + vec3(1.0, 0.97, 0.78) * sunCore * 0.85;

    float moonVisibility = nightAmount * (1.0 - rain * 0.58);
    float moonHalo = pow(moonDot, 32.0) * moonVisibility;
    float moonDisc = smoothstep(0.99820, 0.99948, moonDot) * moonVisibility;
    float moonCore = smoothstep(0.99948, 0.99988, moonDot) * moonVisibility;
    float moonSurface = fbm(skyUv * 210.0 + vec2(19.0, 7.0));
    vec3 moonColor = mix(vec3(0.58, 0.64, 0.76), vec3(0.98, 0.92, 0.76), moonSurface);
    sky += vec3(0.12, 0.20, 0.42) * moonHalo * 0.48;
    sky += moonColor * moonDisc * 0.96 + vec3(1.0, 0.97, 0.84) * moonCore * 0.62;

    float lightning = clamp(lightningAmount, 0.0, 1.35);
    if (lightning > 0.001) {
        float flash = lightning * smoothstep(0.08, 0.82, vertical);
        sky = mix(sky, vec3(0.70, 0.80, 0.98), clamp(flash * 0.55, 0.0, 1.0));
        sky += vec3(0.90, 0.97, 1.0) * lightningBolt(skyUv, lightningSeed) * lightning;
    }

    FragColor = vec4(clamp(sky, 0.0, 1.0), 1.0);
}
