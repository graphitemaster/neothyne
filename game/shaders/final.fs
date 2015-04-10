#include <shaders/screenspace.h>

uniform neoSampler2D gColorMap;
uniform sampler3D gColorGradingMap;

out vec3 fragColor;

#ifdef USE_FXAA
vec3 fxaa(neoSampler2D textureSampler, vec2 texCoord) {
    vec3 outColor;

    const float FXAA_SPAN_MAX = 8.0; // TODO: make console variable?
    const float FXAA_REDUCE_MUL = 1.0/FXAA_SPAN_MAX;
    const float FXAA_REDUCE_MIN = (1.0/128.0);

#ifdef HAS_TEXTURE_RECTANGLE
    vec3 rgbNW = neoTexture2D(textureSampler, texCoord.xy + (vec2(-1.0f, -1.0f))).xyz;
    vec3 rgbNE = neoTexture2D(textureSampler, texCoord.xy + (vec2(+1.0f, -1.0f))).xyz;
    vec3 rgbSW = neoTexture2D(textureSampler, texCoord.xy + (vec2(-1.0f, +1.0f))).xyz;
    vec3 rgbSE = neoTexture2D(textureSampler, texCoord.xy + (vec2(+1.0f, +1.0f))).xyz;
#else
    vec2 texCoordOffset = vec2(1.0f, 1.0f) / gScreenSize;
    vec3 rgbNW = neoTexture2D(textureSampler, texCoord.xy + (vec2(-1.0f, -1.0f) * texCoordOffset)).xyz;
    vec3 rgbNE = neoTexture2D(textureSampler, texCoord.xy + (vec2(+1.0f, -1.0f) * texCoordOffset)).xyz;
    vec3 rgbSW = neoTexture2D(textureSampler, texCoord.xy + (vec2(-1.0f, +1.0f) * texCoordOffset)).xyz;
    vec3 rgbSE = neoTexture2D(textureSampler, texCoord.xy + (vec2(+1.0f, +1.0f) * texCoordOffset)).xyz;
#endif
    vec3 rgbM  = neoTexture2D(textureSampler, texCoord.xy).xyz;

    vec3 luma = vec3(0.299f, 0.587f, 0.114f);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot( rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
        max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin));

#ifndef HAS_TEXTURE_RECTANGLE
    dir *= texCoordOffset;
#endif

    vec3 rgbA = (1.0/2.0) * (
              neoTexture2D(textureSampler, texCoord.xy + dir * (1.0/3.0 - 0.5)).xyz +
              neoTexture2D(textureSampler, texCoord.xy + dir * (2.0/3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
              neoTexture2D(textureSampler, texCoord.xy + dir * (0.0/3.0 - 0.5)).xyz +
              neoTexture2D(textureSampler, texCoord.xy + dir * (3.0/3.0 - 0.5)).xyz);
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) {
        outColor = rgbA;
    } else {
        outColor = rgbB;
    }

    return outColor;
}
#endif

void main() {
    vec2 texCoord = calcTexCoord();
#ifdef USE_FXAA
    fragColor = fxaa(gColorMap, texCoord);
#else
    fragColor = neoTexture2D(gColorMap, texCoord).rgb;
#endif
    // Apply color grading
    fragColor = texture(gColorGradingMap, fragColor.rgb).rbg;
}
