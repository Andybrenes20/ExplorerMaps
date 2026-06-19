#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;

uniform sampler2D texture_diffuse1;
uniform sampler2D shadowMap;
uniform vec3 viewPos;
uniform float time;
uniform float dayFactor;
uniform float nightFactor;
uniform float rainIntensity;
uniform float fogIntensity;
uniform float sunHeight;
uniform float windowLightIntensity;
uniform float cloudCoverage;
uniform float cloudSpeed;
uniform float cloudDensity;
uniform vec3 celestialLightPosition;
uniform float shadowStrength;
uniform vec3 objectTint;
uniform float objectAlpha;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform float pointLightIntensity;

struct Headlight {
    vec3 position;
    vec3 direction;
};

#define NR_HEADLIGHTS 2
uniform Headlight headlights[NR_HEADLIGHTS];
uniform float headlightIntensity;

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float smoothNoise2D(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    local = local * local * (3.0 - 2.0 * local);
    float a = hash31(vec3(cell, 3.0));
    float b = hash31(vec3(cell + vec2(1.0, 0.0), 3.0));
    float c = hash31(vec3(cell + vec2(0.0, 1.0), 3.0));
    float d = hash31(vec3(cell + vec2(1.0, 1.0), 3.0));
    return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

float sampleDirectionalShadow(vec3 normal, vec3 lightDir) {
    if (shadowStrength <= 0.01) {
        return 0.0;
    }

    vec3 projected = FragPosLightSpace.xyz / max(FragPosLightSpace.w, 0.0001);
    projected = projected * 0.5 + 0.5;
    if (projected.z <= 0.0 || projected.z >= 1.0 ||
        projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0) {
        return 0.0;
    }

    float normalLight = max(dot(normal, lightDir), 0.0);
    float bias = mix(0.00105, 0.00018, normalLight);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    const vec2 offsets[4] = vec2[](
        vec2(-0.75, -0.75), vec2(0.75, -0.75),
        vec2(-0.75, 0.75), vec2(0.75, 0.75));
    for (int i = 0; i < 4; ++i) {
        float closestDepth = texture(shadowMap, projected.xy + offsets[i] * texel).r;
        shadow += projected.z - bias > closestDepth ? 1.0 : 0.0;
    }

    float edgeDistance = min(min(projected.x, projected.y), min(1.0 - projected.x, 1.0 - projected.y));
    float edgeFade = smoothstep(0.0, 0.035, edgeDistance);
    return clamp((shadow / 4.0) * shadowStrength * edgeFade, 0.0, 0.97);
}

vec3 tonemap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float saturatedMask(vec3 color) {
    float hi = max(color.r, max(color.g, color.b));
    float lo = min(color.r, min(color.g, color.b));
    return smoothstep(0.12, 0.48, hi - lo) * smoothstep(0.14, 0.62, hi);
}

vec3 pointLight(PointLight lamp, vec3 normal, vec3 albedo, float horizontalSurface) {
    vec3 toLamp = lamp.position - FragPos;
    float distanceToLamp = length(toLamp);
    vec3 lampDir = normalize(toLamp);
    float diffuse = max(dot(normal, lampDir), 0.0);
    float attenuation = 1.0 / (lamp.constant + lamp.linear * distanceToLamp + lamp.quadratic * distanceToLamp * distanceToLamp);

    float verticalDrop = lamp.position.y - FragPos.y;
    float horizontalDistance = length((FragPos - lamp.position).xz);
    float coneRadius = max(verticalDrop * (0.60 + rainIntensity * 0.20), 10.0);
    float cone = (1.0 - smoothstep(coneRadius * 0.42, coneRadius, horizontalDistance)) * smoothstep(0.0, 14.0, verticalDrop);
    float facadeMask = 1.0 - smoothstep(0.32, 0.68, horizontalSurface);
    vec3 color = albedo * lamp.diffuse * (diffuse * 0.85 + 0.12) * attenuation * cone * facadeMask;

    float halo = exp(-(horizontalDistance * horizontalDistance) / 620.0) * (1.0 - smoothstep(0.0, 55.0, distanceToLamp)) * facadeMask;
    color += lamp.diffuse * halo * 0.042;
    return color * pointLightIntensity;
}

vec3 vehicleHeadlight(Headlight lamp, vec3 normal, vec3 albedo) {
    vec3 toFragment = FragPos - lamp.position;
    float distanceToLight = length(toFragment);
    vec3 beamDirection = normalize(toFragment);
    float coneAngle = dot(beamDirection, normalize(lamp.direction));
    float cone = smoothstep(0.905, 0.978, coneAngle);
    float distanceFade = 1.0 - smoothstep(18.0, 52.0, distanceToLight);
    float attenuation = 1.0 / (1.0 + distanceToLight * 0.035 + distanceToLight * distanceToLight * 0.010);
    vec3 towardLight = normalize(lamp.position - FragPos);
    float diffuse = max(dot(normal, towardLight), 0.0);
    float forwardFloorFill = smoothstep(0.18, 0.92, normal.y) * cone * 0.16;
    vec3 warmWhite = mix(vec3(1.0, 0.78, 0.48), vec3(1.0, 0.94, 0.76), rainIntensity);
    return albedo * warmWhite * (diffuse + forwardFloorFill) * cone * distanceFade * attenuation * 3.8 * headlightIntensity;
}

void main() {
    vec4 texel = texture(texture_diffuse1, TexCoord);
    if (texel.a < 0.1) {
        discard;
    }

    vec3 base = texel.rgb * mix(vec3(1.0), objectTint, 0.72);
    vec3 smoothNormal = normalize(gl_FrontFacing ? Normal : -Normal);
    vec3 geometricNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    if (dot(geometricNormal, smoothNormal) < 0.0) {
        geometricNormal = -geometricNormal;
    }
    float architecturalSurface = 1.0 - smoothstep(0.78, 0.98, abs(smoothNormal.y));
    vec3 normal = normalize(mix(smoothNormal, geometricNormal, architecturalSurface * 0.58));
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 positionalLightDir = celestialLightPosition - FragPos;
    vec3 lightDir = length(positionalLightDir) > 0.001 ? normalize(positionalLightDir) : normalize(-light.direction);
    vec3 halfDir = normalize(lightDir + viewDir);
    float lambert = max(dot(normal, lightDir), 0.0);
    float hemi = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    float luma = dot(base, vec3(0.299, 0.587, 0.114));
    float rain = clamp(rainIntensity, 0.0, 1.0);
    float fogAmount = clamp(fogIntensity, 0.0, 1.0);
    float night = clamp(nightFactor, 0.0, 1.0);
    float day = clamp(dayFactor, 0.0, 1.0);
    bool proceduralCityLights = objectAlpha > 0.98;

    float horizontalSurface = smoothstep(0.62, 0.92, normal.y);
    float verticalSurface = 1.0 - horizontalSurface;
    float glassMask = smoothstep(0.03, 0.22, base.b - base.r) * smoothstep(0.12, 0.50, base.g) * smoothstep(0.16, 0.62, base.b) * verticalSurface;
    float darkMask = (1.0 - smoothstep(0.10, 0.34, luma)) * smoothstep(0.08, 0.42, hemi);
    float roadMask = darkMask * horizontalSurface * (1.0 - glassMask);
    float roadPaintMask = horizontalSurface * smoothstep(0.34, 0.72, luma);
    float facadeMask = verticalSurface;
    float textureEdge = clamp(length(vec2(dFdx(luma), dFdy(luma))) * 3.2, 0.0, 0.28);
    float broadSurface = 1.0 - smoothstep(0.12, 0.38, textureEdge);
    float macroVariation = hash31(floor(FragPos * vec3(0.035, 0.11, 0.035))) - 0.5;
    float buildingVariation = hash31(floor(FragPos * vec3(0.012, 0.0, 0.012)) + vec3(13.0, 4.0, 9.0));
    float colorSaturation = max(base.r, max(base.g, base.b)) - min(base.r, min(base.g, base.b));
    float metalMask = facadeMask * broadSurface * smoothstep(0.13, 0.42, colorSaturation) * smoothstep(0.16, 0.62, luma) * (1.0 - glassMask);
    float concreteMask = facadeMask * broadSurface * (1.0 - glassMask) * (1.0 - metalMask);

    vec3 glassTint = mix(vec3(0.16, 0.20, 0.28), vec3(0.42, 0.62, 0.84), day);
    vec3 albedo = mix(base, mix(base, glassTint, 0.48), glassMask * (0.38 + day * 0.36));
    albedo = mix(albedo, albedo * vec3(0.68, 0.71, 0.74), roadMask * (0.52 + rain * 0.30));
    albedo *= 1.0 + macroVariation * (facadeMask * 0.06 + roadMask * 0.10);
    albedo *= mix(0.94, 1.06, buildingVariation);
    albedo *= 1.0 - textureEdge * (facadeMask * 0.18 + horizontalSurface * 0.10);

    float twilight = 1.0 - smoothstep(0.04, 0.40, abs(sunHeight));
    float sunAboveHorizon = smoothstep(-0.12, 0.20, sunHeight);
    float golden = twilight * sunAboveHorizon * (1.0 - rain * 0.55);
    float noonStrength = smoothstep(0.12, 0.82, sunHeight);
    vec3 dryNightBounce = vec3(0.070, 0.085, 0.125);
    vec3 wetNightBounce = vec3(0.055, 0.20, 0.48);
    vec3 skyBounce = mix(mix(dryNightBounce, wetNightBounce, rain), vec3(0.42, 0.60, 0.88), day);
    skyBounce = mix(skyBounce, vec3(0.98, 0.48, 0.18), golden * 0.62);
    float roughness = mix(0.82, 0.22, glassMask);
    roughness = mix(roughness, 0.34, metalMask);
    roughness = mix(roughness, 0.16, roadMask * rain);
    float specularPower = mix(18.0, 112.0, 1.0 - roughness);
    float specular = pow(max(dot(normal, halfDir), 0.0), specularPower);
    float sideFacing = dot(normal.xz, lightDir.xz);
    float sunFacing = smoothstep(-0.12, 0.72, sideFacing) * facadeMask;
    float directionalShape = mix(pow(lambert, 0.82), lambert, noonStrength);
    directionalShape *= mix(0.72, 1.30, sunFacing * sunAboveHorizon);
    float groundBounce = clamp(-normal.y * 0.5 + 0.5, 0.0, 1.0);
    float lowerFacade = facadeMask * (1.0 - smoothstep(4.0, 44.0, FragPos.y));
    float downwardCrease = smoothstep(0.12, 0.82, -normal.y);
    float cheapOcclusion = clamp(1.0 - textureEdge * 0.68 - lowerFacade * 0.10 - downwardCrease * 0.14, 0.54, 1.0);
    float dayFacadeAmbient = mix(1.0, 0.58 + hemi * 0.20, facadeMask * day * (1.0 - rain * 0.45));
    vec3 ambientTerm = (light.ambient * (0.72 + hemi * 0.42) + skyBounce * (0.11 + hemi * 0.16)) * dayFacadeAmbient;
    ambientTerm += vec3(0.12, 0.10, 0.08) * groundBounce * (0.10 + day * 0.10);
    ambientTerm *= cheapOcclusion;
    vec2 cloudUv = (FragPos.xz + vec2(time * 5.2 * cloudSpeed, time * 1.7 * cloudSpeed)) * 0.008;
    float cloudNoise = smoothNoise2D(cloudUv);
    float cloudShadow = smoothstep(0.72 - cloudCoverage * 0.34, 0.92, cloudNoise) * day * (1.0 - rain * 0.72);
    cloudShadow *= clamp(cloudDensity * 0.22, 0.08, 0.36);
    float geometryShadow = sampleDirectionalShadow(normal, lightDir) * day * (1.0 - rain * 0.42);
    float streetShadow = smoothstep(0.04, 0.88, geometryShadow) * horizontalSurface;
    geometryShadow = clamp(geometryShadow + streetShadow * 0.12, 0.0, 0.98);
    float shadowAmbientLoss = geometryShadow * mix(0.52, 0.48, facadeMask) * (1.0 - rain * 0.40);
    ambientTerm *= 1.0 - shadowAmbientLoss;
    vec3 directTerm = light.diffuse * directionalShape * (0.52 + day * 0.46) * (1.0 - cloudShadow) * (1.0 - geometryShadow);
    vec3 lit = albedo * (ambientTerm + directTerm);
    vec3 orientationTint = mix(vec3(0.76, 0.84, 1.0), vec3(1.0, 0.88, 0.72), sunFacing);
    lit *= mix(vec3(1.0), orientationTint, facadeMask * day * (0.055 + golden * 0.10));
    lit += albedo * light.diffuse * concreteMask * sunFacing * textureEdge * 0.12;
    lit += albedo * mix(vec3(0.018, 0.025, 0.055), vec3(0.025, 0.12, 0.34), rain) * facadeMask * night * (0.55 + hemi * 0.45);
    lit += albedo * vec3(0.38, 0.13, 0.025) * directionalShape * golden * 0.34;
    float shadowSide = (1.0 - lambert) * facadeMask * sunAboveHorizon * day;
    float shadowContrast = mix(0.20, 0.34, 1.0 - noonStrength) * (1.0 - rain * 0.55);
    lit *= 1.0 - shadowSide * shadowContrast;
    lit += skyBounce * shadowSide * (0.035 + golden * 0.030);
    lit += skyBounce * geometryShadow * facadeMask * 0.018;
    float contactWeight = geometryShadow * (0.30 + facadeMask * 0.22 + horizontalSurface * 0.28) * (1.0 - rain * 0.35);
    lit *= 1.0 - contactWeight;
    lit += mix(vec3(0.008, 0.010, 0.016), skyBounce * 0.065, rain) * geometryShadow * (1.0 - horizontalSurface * 0.30);
    lit += albedo * vec3(0.34, 0.12, 0.025) * sunFacing * golden * 0.16;
    float dryRoadSpecular = roadMask * (1.0 - rain) * 0.018;
    float materialSpecular = dryRoadSpecular + glassMask * 0.72 + metalMask * (0.16 + day * 0.28) + roadMask * rain * 0.75;
    lit += light.specular * specular * materialSpecular;
    float fresnel = pow(1.0 - clamp(dot(normal, viewDir), 0.0, 1.0), 3.0);
    lit += skyBounce * fresnel * (glassMask * 0.48 + roadMask * rain * 0.34);
    lit += skyBounce * glassMask * (0.06 + hemi * 0.08);
    vec3 fakeReflection = mix(skyBounce, vec3(0.82, 0.88, 0.96), day * 0.30);
    float reflectionBreakup = 0.82 + hash31(floor(FragPos * vec3(0.08, 0.18, 0.08))) * 0.18;
    lit += fakeReflection * glassMask * fresnel * reflectionBreakup * (0.18 + day * 0.18 + rain * 0.22);
    lit += fakeReflection * metalMask * fresnel * (0.035 + day * 0.055);
    float edgeLight = pow(1.0 - abs(dot(normal, viewDir)), 2.2) * facadeMask * broadSurface;
    lit += skyBounce * edgeLight * (0.025 + day * 0.035 + rain * 0.045);
    float moonRim = pow(1.0 - clamp(dot(normal, viewDir), 0.0, 1.0), 2.4) * facadeMask * night;
    lit += mix(vec3(0.08, 0.11, 0.20), vec3(0.06, 0.28, 0.72), rain) * moonRim * 0.50;

    if (pointLightIntensity > 0.02) {
        for (int i = 0; i < NR_POINT_LIGHTS; ++i) {
            lit += pointLight(pointLights[i], normal, albedo, horizontalSurface);
        }
    }

    float cleanRoad = roadMask * (1.0 - roadPaintMask);
    if (proceduralCityLights && night > 0.02) {
        // Cheap city-wide lighting: warm pools on streets and soft bounce on nearby facades.
        const float urbanGridSize = 24.0;
        vec2 urbanCell = floor(FragPos.xz / urbanGridSize);
        vec2 urbanLocal = fract(FragPos.xz / urbanGridSize) - 0.5;
        float urbanSeed = hash31(vec3(urbanCell, 19.0));
        vec2 lampOffset = vec2(
            hash31(vec3(urbanCell, 31.0)) - 0.5,
            hash31(vec3(urbanCell, 47.0)) - 0.5) * 0.24;
        float lampDistance = length(urbanLocal - lampOffset);
        float lampPool = exp(-lampDistance * lampDistance * 15.0);
        lampPool *= smoothstep(0.24, 0.52, urbanSeed) * night * pointLightIntensity;
        float lampVariation = mix(0.76, 1.12, hash31(vec3(urbanCell, 73.0)));
        vec3 urbanWarm = mix(vec3(1.0, 0.42, 0.12), vec3(1.0, 0.76, 0.38), urbanSeed) * lampVariation;
        float roadPool = lampPool * cleanRoad;
        float facadeBounce = lampPool * facadeMask * (1.0 - smoothstep(3.0, 34.0, FragPos.y)) * 0.34;
        lit += urbanWarm * roadPool * (0.24 + rain * 0.48);
        lit += urbanWarm * albedo * facadeBounce * 0.24;
    }

    if (proceduralCityLights && (windowLightIntensity > 0.01 || night > 0.02)) {
        vec3 cell = floor(FragPos * vec3(0.055, 0.16, 0.055));
        vec3 local = fract(FragPos * vec3(0.055, 0.16, 0.055)) - 0.5;
        float seed = hash31(cell);
        float band = smoothstep(0.12, 0.30, fract(FragPos.y * 0.16)) * (1.0 - smoothstep(0.72, 0.92, fract(FragPos.y * 0.16)));
        float axisBlend = step(abs(normal.x), abs(normal.z));
        float across = mix(local.z, local.x, axisBlend);
        float column = 1.0 - smoothstep(0.23, 0.46, abs(across));
        float broadFacade = facadeMask * broadSurface;
        float pane = broadFacade * (1.0 - smoothstep(0.26, 0.66, abs(normal.y))) * max(glassMask, (1.0 - smoothstep(0.06, 0.24, luma)) * 0.58);
        float windowMask = pane * band * column * step(0.34, seed) * smoothstep(0.04, 0.68, windowLightIntensity);
        vec3 windowColor = mix(vec3(1.0, 0.56, 0.24), vec3(1.0, 0.88, 0.58), hash31(cell + 17.0));
        vec3 windowGlow = windowColor * windowMask * (0.92 + night * 1.55 + rain * 0.35);
        float neon = saturatedMask(base) * verticalSurface * night;
        vec3 neonColor = base * base * (1.4 + rain * 0.5);
        lit += windowGlow;
        lit += windowColor * pane * step(0.47, seed) * exp(-(local.y * local.y) * 20.0) * windowLightIntensity * 0.012;
        lit += neonColor * neon * (0.85 + rain * 0.45);
        lit += windowColor * cleanRoad * rain * night * (0.018 + windowLightIntensity * 0.028) *
            (0.55 + 0.45 * sin(FragPos.x * 0.20 + FragPos.z * 0.13));
        lit += (windowGlow + neonColor * neon) * cleanRoad * rain * (0.20 + pow(1.0 - clamp(dot(normal, viewDir), 0.0, 1.0), 3.0) * 0.45);
    }

    // Road markings are painted surfaces, not lamps or polished materials.
    vec3 mattePaint = base * (
        light.ambient * (0.88 + hemi * 0.18) +
        light.diffuse * (lambert * 0.24 + 0.035));
    mattePaint += base * vec3(0.045, 0.050, 0.060) * night;
    mattePaint = min(mattePaint, base * mix(0.86, 0.62, night));
    lit = mix(lit, mattePaint, roadPaintMask * mix(0.72, 0.92, night) * (1.0 - rain * 0.28));

    if (headlightIntensity > 0.01) {
        for (int i = 0; i < NR_HEADLIGHTS; ++i) {
            lit += vehicleHeadlight(headlights[i], normal, base);
        }
    }

    vec3 viewDelta = viewPos - FragPos;
    float viewDistance = length(viewDelta);
    float flatDistance = length(viewDelta.xz);
    float cutoffFog = smoothstep(0.62, 0.82, fogAmount);
    float fogDensity = mix(0.00022, 0.00042, night) + rain * 0.00050 + fogAmount * 0.00165 + cutoffFog * 0.00220;
    float distanceFog = 1.0 - exp(-viewDistance * fogDensity);
    distanceFog = clamp(distanceFog * distanceFog * mix(0.72, 1.05, night + rain * 0.35), 0.0, mix(0.30, 0.48, night) + rain * 0.10 + fogAmount * 0.22 + cutoffFog * 0.26);
    distanceFog = clamp(distanceFog + smoothstep(80.0, 520.0, flatDistance) * fogAmount * 0.30 + smoothstep(8.0, 120.0, flatDistance) * cutoffFog * 0.34, 0.0, 0.92);

    float lowAltitude = exp(-max(FragPos.y - 12.0, 0.0) * 0.030);
    float fogBank = fogAmount > 0.01
        ? smoothNoise2D(FragPos.xz * 0.006 + vec2(time * 0.018, time * 0.006))
        : 0.0;
    fogBank = smoothstep(0.24, 0.78, fogBank) * fogAmount;
    float streetHaze = smoothstep(18.0, 190.0, flatDistance) * lowAltitude * (0.025 + night * 0.035 + rain * 0.22 + fogBank * 0.58 + fogAmount * 0.24 + cutoffFog * 0.34);
    float hazeVariation = 0.84 + 0.16 * sin(FragPos.x * 0.018 + FragPos.z * 0.013 + time * 0.08);
    distanceFog = clamp(distanceFog + streetHaze * hazeVariation, 0.0, 0.94);
    vec3 fogDay = mix(vec3(0.76, 0.84, 0.93), vec3(0.96, 0.70, 0.45), golden * 0.55);
    vec3 fogNight = mix(vec3(0.045, 0.052, 0.072), vec3(0.025, 0.075, 0.18), rain);
    vec3 fogRain = vec3(0.29, 0.34, 0.40);
    vec3 generatedFog = mix(vec3(0.64, 0.70, 0.76), vec3(0.095, 0.115, 0.15), night);
    vec3 fog = mix(mix(fogDay, fogNight, night), fogRain, rain * 0.62);
    fog = mix(fog, generatedFog, fogAmount * 0.90);
    fog += mix(vec3(0.006, 0.008, 0.014), vec3(0.018, 0.055, 0.13), rain) * night;
    lit = mix(lit, fog, distanceFog);

    lit = tonemap(lit * mix(0.88, 1.08, night));
    float gradedLuma = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(vec3(gradedLuma), lit, mix(1.05, mix(1.12, 1.28, rain), night));
    lit = clamp((lit - 0.5) * 1.04 + 0.5, 0.0, 1.0);
    lit = pow(max(lit, vec3(0.0)), vec3(1.0 / 1.08));
    FragColor = vec4(clamp(lit, 0.0, 1.0), texel.a * clamp(objectAlpha, 0.0, 1.0));
}
