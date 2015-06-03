#include <shaders/smaa.h>

uniform neoSampler2D gAlbedoMap;

#if SMAA_PREDICATION == 1
#   include <shaders/depth.h>
#endif

in vec4 offset[3];

out vec4 fragColor;

void main() {
    vec2 texCoord0 = calcTexCoord();
#if SMAA_PREDICATION == 1
    fragColor = SMAAColorEdgeDetectionPS(texCoord0, offset, gAlbedoMap, gDepthMap);
#else
    fragColor = SMAAColorEdgeDetectionPS(texCoord0, offset, gAlbedoMap);
#endif
}
