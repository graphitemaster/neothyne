#include <shaders/screenspace.h>
#include <shaders/utils.h>

uniform neoSampler2D gColorMap;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();

    const float FXAA_SPAN_MAX = 8.0; // TODO: make console variable?
    const float FXAA_REDUCE_MUL = 1.0/FXAA_SPAN_MAX;
    const float FXAA_REDUCE_MIN = (1.0/128.0);

#ifdef HAS_TEXTURE_RECTANGLE
    vec4 rgbNW = neoTexture2D(gColorMap, texCoord.xy + (vec2(-1.0f, -1.0f)));
    vec4 rgbNE = neoTexture2D(gColorMap, texCoord.xy + (vec2(+1.0f, -1.0f)));
    vec4 rgbSW = neoTexture2D(gColorMap, texCoord.xy + (vec2(-1.0f, +1.0f)));
    vec4 rgbSE = neoTexture2D(gColorMap, texCoord.xy + (vec2(+1.0f, +1.0f)));
#else
    vec2 texCoordOffset = vec2(1.0f, 1.0f) / gScreenSize;
    vec4 rgbNW = neoTexture2D(gColorMap, texCoord.xy + (vec2(-1.0f, -1.0f) * texCoordOffset));
    vec4 rgbNE = neoTexture2D(gColorMap, texCoord.xy + (vec2(+1.0f, -1.0f) * texCoordOffset));
    vec4 rgbSW = neoTexture2D(gColorMap, texCoord.xy + (vec2(-1.0f, +1.0f) * texCoordOffset));
    vec4 rgbSE = neoTexture2D(gColorMap, texCoord.xy + (vec2(+1.0f, +1.0f) * texCoordOffset));
#endif
    vec4 rgbM  = neoTexture2D(gColorMap, texCoord.xy);

    vec4 luma = vec4(0.299f, 0.587f, 0.114f, 1.0f);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot( rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin));

#ifndef HAS_TEXTURE_RECTANGLE
    dir *= texCoordOffset;
#endif

    vec4 rgbA = 0.5f * (neoTexture2D(gColorMap, texCoord.xy + dir * (1.0f/3.0f - 0.5f)) +
                        neoTexture2D(gColorMap, texCoord.xy + dir * (2.0f/3.0f - 0.5f)));
    vec4 rgbB = rgbA * 0.5f + 0.25f * (neoTexture2D(gColorMap, texCoord.xy + dir * (3.0f/3.0f - 0.5f)) +
                                       neoTexture2D(gColorMap, texCoord.xy + dir * (4.0f/3.0f - 0.5f)));
    float lumaB = dot(rgbB, luma);

    if (lumaB < lumaMin || lumaB > lumaMax) {
        fragColor = MASK_ALPHA(rgbA);
    } else {
        fragColor = MASK_ALPHA(rgbB);
    }
}
