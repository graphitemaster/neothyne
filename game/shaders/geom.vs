layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in float w;

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
out vec3 eye0;
out vec3 position0;
#endif

void main() {
    gl_Position = gWVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
    normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
    tangent0 = (gWorld * vec4(tangent, 0.0f)).xyz;
    bitangent0 = w * cross(normal0, tangent0);

#ifdef USE_PARALLAX
    position0 = (gWorld * vec4(position0, 1.0f)).xyz;

    vec3 eyeDir = gEyeWorldPosition - position;
    mat3 tbn = mat3(tangent0, bitangent0, normal0);
    eye0 = eyeDir * tbn;
#endif
}
