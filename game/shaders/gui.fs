in vec2 texCoord0;
in vec4 color0;

uniform sampler2D gColorMap;

out vec4 fragColor;

void main() {
    float alpha = texture(gColorMap, texCoord0).r;
    fragColor = vec4(color0.rgb, color0.a * alpha);
}
