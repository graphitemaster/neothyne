in vec3 position;
in vec3 normal;
in vec2 texCoord;
in vec3 tangent;
in float w;

uniform mat4 gWVP;
uniform mat4 gWorld;

#ifdef USE_PARALLAX
uniform vec3 gEyeWorldPosition;
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
    gl_Position = gWVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
    normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
    tangent0 = (gWorld * vec4(tangent, 0.0f)).xyz;
    bitangent0 = w * cross(normal0, tangent0);

#ifdef USE_PARALLAX
    vec3 eyePosition = (gWorld * vec4(position, 1.0f)).xyz;
    vec3 eyeDirection = gEyeWorldPosition - position;
    mat3 eyeTBN = mat3(tangent0, bitangent0, normal0);
    eyePosition0 = eyeDirection * eyeTBN;
    vec3 eyeDotDirection = eyePosition0 - eyePosition;
    eyeDotDirection0 = ceil(dot(eyeDotDirection, eyeDotDirection));
#endif
}
