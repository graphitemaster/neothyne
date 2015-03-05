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

vec2 calcTexCoord() {
#ifdef HAS_TEXTURE_RECTANGLE
    return gl_FragCoord.xy;
#else
    return gl_FragCoord.xy / gScreenSize;
#endif
}

vec2 calcFragCoord(vec2 texCoord) {
#ifdef HAS_TEXTURE_RECTANGLE
    return texCoord / gScreenSize;
#else
    return texCoord;
#endif
}

#endif
