#include <shaders/screenspace.h>
#include <shaders/utils.h>

uniform neoSampler2D gColorMap;

uniform vec2 gProperties;

out vec4 fragColor;

const vec2 kCenter = vec2(0.5f);

void main() {
    vec2 texCoord = calcTexCoord();
    vec4 texColor = neoTexture2D(gColorMap, texCoord);

#ifdef HAS_TEXTURE_RECTANGLE
    vec2 position = texCoord / gScreenSize - kCenter;
#else
    vec2 position = texCoord - kCenter;
#endif

    // Correct for aspect ratio
    position.x *= gScreenSize.x / gScreenSize.y;

    float vignette = smoothstep(gProperties.x,
                                gProperties.x-gProperties.y,
                                length(position));

    // Apply vignette
    fragColor = MASK_ALPHA(texColor * vignette);
}
