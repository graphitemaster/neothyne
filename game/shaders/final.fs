#include <shaders/screenspace.h>
#include <shaders/utils.h>

uniform neoSampler2D gColorMap;
uniform sampler3D gColorGradingMap;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcScreenTexCoord();
    vec4 color = neoTexture2D(gColorMap, texCoord);
    vec4 graded = texture(gColorGradingMap, color.rgb);
    fragColor = MASK_ALPHA(graded);
}
