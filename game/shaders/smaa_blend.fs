#include <shaders/smaa.h>

uniform sampler2D gEdgeMap;
uniform sampler2D gAreaMap;
uniform sampler2D gSearchMap;

in vec2 texCoord0;
in vec2 pixCoord0;

in vec4 offset[3];

out vec4 fragColor;

void main() {
    fragColor = vec4(1, 0, 0, 1);
#if 0
    fragColor = SMAABlendingWeightCalculationPS(texCoord0,
                                                pixCoord0,
                                                offset,
                                                gEdgeMap,
                                                gAreaMap,
                                                gSearchMap,
                                                ivec4(0));
#endif
}
