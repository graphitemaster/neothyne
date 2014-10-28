in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 bitangent0;

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

#ifdef USE_SPECPARAMS
uniform float gSpecPower;
uniform float gSpecIntensity;
#endif

#ifdef USE_NORMALMAP
vec3 calcBump() {
    vec3 normal;
    normal.xy = texture(gNormalMap, texCoord0).rg * 2.0f - vec2(1.0f, 1.0f);
    normal.z = sqrt(1.0f - dot(normal.xy, normal.xy));
    mat3 tbn = mat3(tangent0, bitangent0, normal0);
    return normalize(tbn * normal);
}
#endif

void main() {
#ifdef USE_DIFFUSE
    diffuseOut.rgb = texture(gColorMap, texCoord0).rgb;
#endif
#ifdef USE_NORMALMAP
    normalOut.rgb = calcBump() * 0.5f + 0.5f;
#else
    normalOut.rgb = normal0 * 0.5 + 0.5f;
#endif
#ifdef USE_SPECMAP
    // R contains intensity, B contains power. We pack intensity in alpha of
    // diffuse while power in alpha of normal
    vec2 spec = texture(gSpecMap, texCoord0).rg;
    diffuseOut.a = spec.r;
    normalOut.a = spec.g;
#elif defined(USE_SPECPARAMS)
    diffuseOut.a = gSpecIntensity;
    normalOut.a = gSpecPower;
#else
    diffuseOut.a = 0.0f;
    normalOut.a = 0.0f;
#endif
}
