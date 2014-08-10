#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec3 tangent;

uniform mat4 gWVP;
uniform mat4 gWorld;

out vec3 normal0;
out vec2 texCoord0;
out vec3 tangent0;
out vec3 worldPos0;

void main() {
    gl_Position = gWVP * vec4(position, 1.0f);

    texCoord0 = texCoord;
    normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
    worldPos0 = (gWorld * vec4(position, 1.0f)).xyz;
    tangent0 = (gWorld * vec4(tangent, 0.0f)).xyz;
}
