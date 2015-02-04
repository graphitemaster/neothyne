#include "shaders/light.h"

in vec2 texCoord0;
in vec3 normal0;
in vec3 worldPosition0;

uniform sampler2D gColorMap;

out vec4 fragColor;

void main() {
    pointLight light;

    light.base.color = vec3(0.85f, 0.85f, 0.85f);
    light.base.ambient = 0.90f;
    light.base.diffuse = 0.10f;
    light.position = vec3(0.0f, 1.0f, 0.0f);
    light.radius = 25.0f;

    vec4 value = calcPointLight(light, worldPosition0, normalize(normal0), vec2(0.1f, 1.0f));
    fragColor = texture2D(gColorMap, texCoord0) * value;
    fragColor.a = 1.0f;
}
