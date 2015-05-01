in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 bitangent0;

#ifdef USE_PARALLAX
in vec3 eyePosition0;
in float eyeDotDirection0;
#endif

out vec4 diffuseOut;
out vec4 normalOut;

#ifdef USE_DIFFUSE
uniform sampler2D gColorMap;
#endif

#ifdef USE_NORMALMAP
uniform sampler2D gNormalMap;
#endif

#ifdef USE_SPECMAP
uniform sampler2D gSpecMap;
#endif

#ifdef USE_PARALLAX
uniform sampler2D gDispMap;
#endif

#ifdef USE_SPECPARAMS
uniform float gSpecPower;
uniform float gSpecIntensity;
#endif

#ifdef USE_PARALLAX
uniform vec2 gParallax; // { scale, bias }
#endif

#ifdef USE_ANIMATION
uniform ivec2 gAnimOffset;
uniform vec2 gAnimFlip;
uniform vec2 gAnimScale;
#endif

#ifdef USE_NORMALMAP
vec3 calcBump(vec2 texCoord) {
    vec3 normal;
    normal.xy = texture(gNormalMap, texCoord).rg * 2.0f - vec2(1.0f, 1.0f);
    normal.z = sqrt(1.0f - dot(normal.xy, normal.xy));
    mat3 tbn = mat3(tangent0, bitangent0, normal0);
    return normalize(tbn * normal);
}
#endif

#ifdef USE_PARALLAX
const int minLayers = 15;
const int maxLayers = 20;
const int maxReliefSearches = 15;

vec2 parallax(vec2 texCoord) {
    float numLayers = clamp(eyeDotDirection0, minLayers, maxLayers);
    float layerHeight = 1.0f / numLayers;

    vec3 shift = vec3(0.1f * gParallax.x * layerHeight * eyePosition0.xy / eyePosition0.z, layerHeight);
    vec3 position = vec3(texCoord - shift.xy * numLayers * gParallax.y, 1.0f);

    // steep parallax part (forward stepping until the ray is below the heightmap)
    float height = texture(gDispMap, position.xy).r;
    for (int i = 0; i < numLayers; ++i) {
        position -= shift * step(height, position.z);
        height = texture(gDispMap, position.xy).r;
    }

    // Relief mapping part (binary search for the intersection)
    height = texture(gDispMap, position.xy).r;
    for (int i = 0; i < maxReliefSearches; i++) {
        position -= shift * (step(height, position.z) - 0.5f) * exp2(-i);
        height = texture(gDispMap, position.xy).r;
    }

    return position.xy;
}
#endif

void main() {
#if defined(USE_ANIMATION)
    vec2 texCoordBase = (texCoord0 * gAnimFlip + gAnimOffset) * gAnimScale;
#else
    vec2 texCoordBase = texCoord0;
#endif

#if defined(USE_PARALLAX)
    vec2 texCoord = parallax(texCoordBase);
#else
    vec2 texCoord = texCoordBase;
#endif

#ifdef USE_DIFFUSE
    diffuseOut.rgb = texture(gColorMap, texCoord).rgb;
#endif

#ifdef USE_NORMALMAP
    normalOut.rgb = calcBump(texCoord) * 0.5f + 0.5f;
#else
    normalOut.rgb = normal0 * 0.5 + 0.5f;
#endif

#ifdef USE_SPECMAP
    // R contains intensity, B contains power. We pack intensity in alpha of
    // diffuse while power in alpha of normal
    vec2 spec = texture(gSpecMap, texCoord).rg;
    diffuseOut.a = spec.r;
    normalOut.a = spec.g;
    // red channel contains intensity while green contains power
#elif defined(USE_SPECPARAMS)
    diffuseOut.a = gSpecIntensity;
    normalOut.a = gSpecPower;
#else
    diffuseOut.a = 0.0f;
    normalOut.a = 0.0f;
#endif
}
