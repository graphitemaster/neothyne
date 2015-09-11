#include <shaders/screenspace.h>
#include <shaders/depth.h>

uniform sampler2D gShadowMap;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    fragColor = vec4(evalLinearDepth(texCoord, texture(gShadowMap, texCoord).x));
}
