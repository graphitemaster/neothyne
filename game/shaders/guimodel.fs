#include <shaders/light.h>
#include <shaders/utils.h>

in vec2 texCoord0;
in vec3 normal0;
in vec3 worldPosition0;

uniform sampler2D gColorMap;

#ifdef USE_SCROLL
uniform ivec2 gScrollRate;
uniform float gScrollMillis;
#endif

out vec4 fragColor;

void main() {
    pointLight light;

    PL_COLOR(light) = vec3(1.5f, 1.5f, 1.5f);
    PL_DIFFUSE(light) = 1.0f;
    PL_POSITION(light) = vec3(0.0f, 1.0f, 5.0f);
    PL_RADIUS(light) = 1000.0f;

    vec4 value = calcPointLight(light, worldPosition0, normalize(normal0), vec2(0.1f, 1.0f));
#ifdef USE_SCROLL
    fragColor = MASK_ALPHA(texture(gColorMap, texCoord0 + (gScrollMillis * gScrollRate)) * value);
#else
    fragColor = MASK_ALPHA(texture(gColorMap, texCoord0) * value);
#endif
}
