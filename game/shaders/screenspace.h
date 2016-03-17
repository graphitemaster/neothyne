#ifndef SCREENSPACE_HDR
#define SCREENSPACE_HDR
#include <shaders/screen.h>

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

#endif
