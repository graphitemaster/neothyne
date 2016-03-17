#ifndef SCREEN_HDR
#define SCREEN_HDR

#ifdef HAS_TEXTURE_RECTANGLE
#  extension GL_ARB_texture_rectangle : enable
#  define neoSampler2D sampler2DRect
#  define neoTexture2D texture2DRect
#else
#  define neoSampler2D sampler2D
#  define neoTexture2D texture
#endif

uniform vec2 gScreenSize; // { width, height }

// If it's not a screen quad-aligned effect but is a screen-space effect
// then utilize this to calculate coordinates.
vec2 calcTexCoord() {
#ifdef HAS_TEXTURE_RECTANGLE
    return gl_FragCoord.xy;
#else
    return gl_FragCoord.xy / gScreenSize;
#endif
}

#endif
