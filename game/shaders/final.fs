#include <shaders/screenspace.h>

uniform neoSampler2D gColorMap;
uniform sampler3D gColorGradingMap;

out vec3 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    fragColor = neoTexture2D(gColorMap, texCoord).rgb;
    fragColor = texture(gColorGradingMap, fragColor).rgb;
}
