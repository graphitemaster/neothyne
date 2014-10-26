in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 bitangent0;

layout (location = 0) out vec3 diffuseOut;
layout (location = 1) out vec2 normalOut;
layout (location = 2) out vec2 specularOut;

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

#define EPSILON 0.00001f

vec2 encodeNormal(vec3 normal) {
    // Project normal positive hemisphere to unit circle
    vec2 p = normal.xy / (abs(normal.z) + 1.0f);

    // Convert unit circle to square
    float d = abs(p.x) + abs(p.y) + EPSILON;
    float r = length (p);
    vec2 q = p * r / d;

    // Mirror triangles to outer edge if z is negative
    float z_is_negative = max (-sign (normal.z), 0.0f);
    vec2 q_sign = sign (q);
    q_sign = sign(q_sign + vec2(0.5f, 0.5f));

    // Reflection
    q -= z_is_negative * (dot(q, q_sign) - 1.0f) * q_sign;
    return q;
}

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
    diffuseOut = texture(gColorMap, texCoord0).rgb;
#endif
#ifdef USE_NORMALMAP
    normalOut = encodeNormal(calcBump());
#else
    normalOut = encodeNormal(normal0);
#endif
#ifdef USE_SPECMAP
    // red channel contains intensity while green contains power
    specularOut = texture(gSpecMap, texCoord0).rg;
#elif defined(USE_SPECPARAMS)
    specularOut.x = gSpecIntensity;
    specularOut.y = gSpecPower;
#else
    specularOut = vec2(0.0f, 0.0f);
#endif
}
