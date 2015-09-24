#include "shaders/screenspace.h"
#include "shaders/depth.h"

in vec2 texCoord0;
in vec4 color0;
in vec2 position0;

uniform sampler2D gColorMap;

out vec4 fragColor;

float kPower = 1.5f;

float contrast(float d) {
    float value = clamp(2.0f*((d > 0.5f) ? 1.0f-d : d), 0.0f, 1.0f);
    float alpha = 0.5f * pow(value, kPower);
    return (d > 0.5f) ? 1.0f - alpha : alpha;
}

void main() {
    vec4 color = texture(gColorMap, texCoord0);
    float depth = calcDepth(calcTexCoord());
    fragColor = color * vec4(1.0f, 1.0f, 1.0f, contrast(depth));
}
