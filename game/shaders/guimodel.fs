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

    light.base.color = vec3(0.5f, 0.5f, 0.5f);
    light.base.ambient = 1.0f;
    light.base.diffuse = 1.0f;
    light.position = vec3(0.0f, 1.0f, 10.0f);
    light.radius = 1000.0f;

    vec4 value = calcPointLight(light, worldPosition0, normalize(normal0), vec2(0.1f, 1.0f));
#ifdef USE_SCROLL
    fragColor = MASK_ALPHA(texture(gColorMap, texCoord0 + (gScrollMillis * gScrollRate)) * value);
#else
    fragColor = MASK_ALPHA(texture(gColorMap, texCoord0) * value);
#endif
}
