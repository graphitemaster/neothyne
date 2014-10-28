in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 bitangent0;
#ifdef USE_PARALLAX
in vec3 position0;
in vec3 eye0;
#endif

layout (location = 0) out vec4 diffuseOut;
layout (location = 1) out vec4 normalOut;

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

#define EPSILON 0.00001f

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
vec2 parallax(vec2 texCoord) {
    if (abs(eye0.z) < 0.001)
        return texCoord;

    vec3 eyeDir = eye0 - position0;
    float numLayers = clamp(ceil(10000.0/dot(eyeDir,eyeDir)),3,10);
    float layerHeight = 1.0 / numLayers;

    vec3 shift = vec3(0.1 * gParallax.x * layerHeight * eye0.xy / eye0.z, layerHeight);
    vec3 position = vec3(texCoord - shift.xy * numLayers * gParallax.y, 1);

    // steep parallax part (forward stepping until the ray is below the heightmap)
    float height = texture(gDispMap, position.xy).r;
    for (float i = 0; i < numLayers; ++i) {
      position -= shift * step(height, position.z);
      height = texture(gDispMap, position.xy).r;
    }

    // Relief mapping part (binary search for the intersection)
    float f = 1;
    for (float i = 0; i < 6; ++i, f *= 0.5) {
      position -= shift * (step(height, position.z) - 0.5) * f;
      height = texture(gDispMap, position.xy).r;
    }

    return position.xy;
}
# endif

void main() {
    vec2 texCoord = texCoord0;
#ifdef USE_PARALLAX
    texCoord = parallax(texCoord);
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
