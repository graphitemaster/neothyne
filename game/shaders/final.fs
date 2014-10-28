#ifdef HAS_TEXTURE_RECTANGLE
#  extension GL_ARB_texture_rectangle : enable
#  define neoSampler2D sampler2DRect
#  define neoTexture2D texture2DRect
#else
#  define neoSampler2D sampler2D
#  define neoTexture2D texture2D
#endif

uniform neoSampler2D gColorMap;

uniform vec2 gScreenSize;

out vec4 fragColor;

vec2 calcTexCoord() {
#ifdef HAS_TEXTURE_RECTANGLE
    return gl_FragCoord.xy;
#else
    return gl_FragCoord.xy / gScreenSize;
#endif
}

void main() {
    fragColor = neoTexture2D(gColorMap, calcTexCoord());
}
