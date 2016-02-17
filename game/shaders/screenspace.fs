#include <shaders/screenspace.h>

uniform neoSampler2D gColorMap;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcScreenTexCoord();
    fragColor = neoTexture2D(gColorMap, texCoord);
}
