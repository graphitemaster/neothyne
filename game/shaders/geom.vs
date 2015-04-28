in vec3 position;
in vec3 normal;
in vec2 texCoord;
in vec4 tangent;

#ifdef USE_SKELETAL
in vec4 weights;
in vec4 bones;
#endif

uniform mat4 gWVP;
uniform mat4 gWorld;

#ifdef USE_PARALLAX
uniform vec3 gEyeWorldPosition;
#endif

#ifdef USE_SKELETAL
uniform mat3x4 gBoneMats[80];
#endif

out vec3 normal0;
out vec2 texCoord0;
out vec3 tangent0;
out vec3 bitangent0;

#ifdef USE_PARALLAX
out vec3 eyePosition0;
out float eyeDotDirection0;
#endif

void main() {
#ifdef USE_SKELETAL
    mat3x4 m = gBoneMats[int(bones.x)] * weights.x;
    m += gBoneMats[int(bones.y)] * weights.y;
    m += gBoneMats[int(bones.z)] * weights.z;
    m += gBoneMats[int(bones.w)] * weights.w;
    vec4 pos = vec4(vec4(position, 1.0f) * m, 1.0f);
    gl_Position = gWVP * pos;
    texCoord0 = texCoord;
    mat3 trans = mat3(cross(m[1].xyz, m[2].xyz),
                      cross(m[2].xyz, m[0].xyz),
                      cross(m[0].xyz, m[1].xyz));
    normal0 = (gWorld * vec4(normal * trans, 0.0f)).xyz;
    tangent0 = (gWorld * vec4(tangent.xyz * trans, 0.0f)).xyz;
    bitangent0 = tangent.w * cross(normal0, tangent0);
#else
    gl_Position = gWVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
    normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
    tangent0 = (gWorld * vec4(tangent.xyz, 0.0f)).xyz;
    bitangent0 = tangent.w * cross(normal0, tangent0);
#endif

#ifdef USE_PARALLAX
    vec3 eyePosition = (gWorld * vec4(position, 1.0f)).xyz;
    vec3 eyeDirection = gEyeWorldPosition - position;
    mat3 eyeTBN = mat3(tangent0, bitangent0, normal0);
    eyePosition0 = eyeDirection * eyeTBN;
    vec3 eyeDotDirection = eyePosition0 - eyePosition;
    eyeDotDirection0 = ceil(dot(eyeDotDirection, eyeDotDirection));
#endif
}
