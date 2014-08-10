#version 330

in vec3 texCoord0;

out vec4 fragColor;

uniform samplerCube gCubemap;

void main(void) {
    fragColor = texture(gCubemap, texCoord0);
    //fragColor = texCoord0.xxyy; //texture(gCubemap, vec3(1.0, 0.0, 0.0));
}
