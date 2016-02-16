#ifndef SCREENSPACE_HDR
#define SCREENSPACE_HDR

#ifdef HAS_TEXTURE_RECTANGLE
#  extension GL_ARB_texture_rectangle : enable
#  define neoSampler2D sampler2DRect
#  define neoTexture2D texture2DRect
#else
#  define neoSampler2D sampler2D
#  define neoTexture2D texture
#endif

uniform vec2 gScreenSize; // { width, height }
uniform vec2 gScreenFrustum; // { near, far }

// Full screen quad-aligned effects that utilize default.vs as their
// vertex program will emit the fragment coordinate here. For those
// shaders utilize this to calculate coordinates.
in vec2 fragCoord;
vec2 calcScreenTexCoord() {
#ifdef HAS_TEXTURE_RECTANGLE
    return fragCoord * gScreenSize;
#else
    return fragCoord;
#endif
}

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
