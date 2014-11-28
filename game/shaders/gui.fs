in vec2 texCoord0;
in vec4 color0;

#if defined(HAS_FONT) || defined(HAS_IMAGE)
uniform sampler2D gColorMap;
#endif

out vec4 fragColor;

void main() {
#ifdef HAS_FONT
    fragColor = vec4(color0.rgb, color0.a * texture(gColorMap, texCoord0).r);
#elif defined(HAS_IMAGE)
    fragColor = texture2D(gColorMap, texCoord0);
#else
    fragColor = color0;
#endif
}
