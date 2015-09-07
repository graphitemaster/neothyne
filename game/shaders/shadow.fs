#include <shaders/screenspace.h>

uniform sampler2D gShadowMap;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    float depth = texture(gShadowMap, texCoord).x;
    float z = (2.0f * gScreenFrustum.x) / (gScreenFrustum.y + gScreenFrustum.x - depth * (gScreenFrustum.y - gScreenFrustum.x));
    fragColor = vec4(z);
#if 0 // Alternative visualization (much more hackier)
    float depth = texture(gShadowMap, texCoord).x;
    depth = 1.0f - (1.0f - depth) * 25.0f;
    fragColor= vec4(depth);
#endif

}
