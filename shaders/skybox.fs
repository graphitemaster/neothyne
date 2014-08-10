#version 330 core

in vec3 texCoord0;

out vec4 fragColor;

uniform samplerCube gCubemap;

void main(void) {
    fragColor = texture(gCubemap, texCoord0);
}
