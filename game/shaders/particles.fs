#include <shaders/screenspace.h>
#include <shaders/depth.h>

in vec2 texCoord0;
in vec4 color0;
in vec2 position0;
in float depth0;

uniform sampler2D gColorMap;
uniform float gPower;

out vec4 fragColor;

float contrast(float d) {
    bool half = d > 0.5f;
    float value = clamp(2.0f * (half ? 1.0f - d : d), 0.0f, 1.0f);
    float alpha = 0.5f * pow(value, gPower);
    return half ? 1.0f - alpha : alpha;
}

void main() {
    vec4 color = texture(gColorMap, texCoord0);
    float depth = calcDepth(calcDepthCoord(calcTexCoord()));
    fragColor = color * vec4(1.0f, 1.0f, 1.0f, contrast(depth - gl_FragDepth));
}
