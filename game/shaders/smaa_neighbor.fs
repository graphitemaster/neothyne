#include <shaders/smaa.h>

uniform sampler2D gAlbedoMap;
uniform sampler2D gBlendMap;

in vec2 texCoord0;
in vec4 offset[2];

out vec4 fragColor;

void main() {
    fragColor = SMAANeighborhoodBlendingPS(texCoord0, offset, gAlbedoMap, gBlendMap);
}
