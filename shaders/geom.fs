in vec2 texCoord0;
in vec3 normal0;
in vec3 worldPosition0;

layout (location = 0) out vec3 worldPositionOut;
layout (location = 1) out vec3 diffuseOut;
layout (location = 2) out vec3 normalOut;
layout (location = 3) out vec3 texCoordOut;

uniform sampler2D gColorMap;

void main(void) {
    worldPositionOut = worldPosition0;
    diffuseOut = texture(gColorMap, texCoord0).xyz;
    normalOut = normalize(normal0);
    texCoordOut = vec3(texCoord0, 0.0f);
}
