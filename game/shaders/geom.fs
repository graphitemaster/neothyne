in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 bitangent0;

layout (location = 0) out vec3 diffuseOut;
layout (location = 1) out vec2 normalOut;

uniform sampler2D gColorMap;
uniform sampler2D gNormalMap;

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

vec3 calcBump() {
    vec3 bn = 2.0f * texture(gNormalMap, texCoord0).rgb - 1.0f;
    mat3 tbn = mat3(tangent0, bitangent0, normal0);
    return normalize(tbn * bn);
}

void main() {
    diffuseOut = texture(gColorMap, texCoord0).rgb;
    normalOut = encodeNormal(calcBump());
}
