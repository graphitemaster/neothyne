#ifndef DEPTH_HDR
#define DEPTH_HDR

uniform neoSampler2D gDepthMap;

uniform mat4 gInverse;

uniform vec2 gScreenSize; // { width, height }
uniform vec2 gScreenFrustum; // { near, far }

vec2 calcDepthCoord(vec2 texCoord) {
#ifdef HAS_TEXTURE_RECTANGLE
    return texCoord / gScreenSize;
#else
    return texCoord;
#endif
}

vec3 calcPosition(vec2 texCoord) {
    vec2 fragCoord = calcDepthCoord(texCoord);
    float depth = neoTexture2D(gDepthMap, texCoord).r * 2.0f - 1.0f;
    vec4 pos = vec4(fragCoord * 2.0f - 1.0f, depth, 1.0f);
    pos = gInverse * pos;
    return pos.xyz / pos.w;
}

#ifdef USE_SHADOWMAP
vec3 calcShadowPosition(mat4 lightWVP, vec2 texCoord) {
    vec2 fragCoord = calcDepthCoord(texCoord);
    float depth = neoTexture2D(gDepthMap, texCoord).r * 2.0f - 1.0f;
    vec4 position = vec4(fragCoord * 2.0f - 1.0f, depth, 1.0f);
    position = gInverse * (lightWVP * position);
    return position.xyz / position.w;
}
#endif

float evalLinearDepth(vec2 texCoord, float depth) {
    return (2.0f * gScreenFrustum.x)
        / (gScreenFrustum.y + gScreenFrustum.x -
            depth * (gScreenFrustum.y - gScreenFrustum.x));
}

float calcLinearDepth(vec2 texCoord) {
    return evalLinearDepth(texCoord, neoTexture2D(gDepthMap, texCoord).x);
}

#endif
