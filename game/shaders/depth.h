#ifndef DEPTH_HDR
#define DEPTH_HDR

uniform neoSampler2D gDepthMap;

uniform mat4 gInverse;

uniform vec2 gScreenSize; // { width, height }
uniform vec2 gScreenFrustum; // { near, far }

vec3 calcPosition(vec2 texCoord) {
#ifdef HAS_TEXTURE_RECTANGLE
    vec2 fragCoord = texCoord / gScreenSize;
#else
    vec2 fragCoord = texCoord;
#endif
    float depth = neoTexture2D(gDepthMap, texCoord).r * 2.0f - 1.0f;
    vec4 pos = vec4(fragCoord * 2.0f - 1.0f, depth, 1.0f);
    pos = gInverse * pos;
    return pos.xyz / pos.w;
}

float calcLinearDepth(vec2 texCoord) {
    return (2.0f * gScreenFrustum.x)
        / (gScreenFrustum.y + gScreenFrustum.x -
            neoTexture2D(gDepthMap, texCoord).x * (gScreenFrustum.y - gScreenFrustum.x));
}

#endif
