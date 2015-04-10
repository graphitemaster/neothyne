#ifndef UTILS_HDR
#define UTILS_HDR

const vec2 maskAlpha = vec2(1.0f, 0.0f);

#define MASK_ALPHA(COLOR) \
    ((COLOR).xyzw * maskAlpha.xxxy + maskAlpha.yyyx)

#endif
