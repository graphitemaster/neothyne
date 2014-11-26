in vec2 texCoord0;
in vec4 color0;

#ifdef HAS_FONT
uniform sampler2D gColorMap;
#endif

out vec4 fragColor;

void main() {
#ifdef HAS_FONT
    float alpha = texture(gColorMap, texCoord0).r;
#else
    float alpha = 1.0f;
#endif
    fragColor = vec4(color0.rgb, color0.a * alpha);
}
