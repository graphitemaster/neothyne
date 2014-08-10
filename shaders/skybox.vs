#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 gWVP;

out vec3 texCoord0; // 3d texel fetch

void main(void) {
    texCoord0 = normalize(position);
    gl_Position = (gWVP * vec4(position, 1.0f)).xyww;
}

