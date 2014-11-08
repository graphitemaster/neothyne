#ifdef HAS_TEXTURE_RECTANGLE
#  extension GL_ARB_texture_rectangle : enable
#  define neoSampler2D sampler2DRect
#  define neoTexture2D texture2DRect
#else
#  define neoSampler2D sampler2D
#  define neoTexture2D texture2D
#endif

#define M_PI 3.1415926535897932384626433832795

uniform neoSampler2D gNormalMap;
uniform neoSampler2D gDepthMap;
uniform sampler2D gRandomMap;

uniform vec2 gScreenSize;
uniform vec2 gScreenFrustum;

uniform float gOccluderBias;
uniform float gSamplingRadius;
uniform vec2 gAttenuation; // { constant, linear }
uniform mat4 gInverse;

out vec4 fragColor;

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

vec3 calcPosition(vec2 texCoord) {
    float depth = neoTexture2D(gDepthMap, texCoord).r * 2.0f - 1.0f;
    vec4 pos = vec4(calcFragCoord(texCoord) * 2.0f - 1.0f, depth, 1.0f);
    pos = gInverse * pos;
    return pos.xyz / pos.w;
}

float samplePixels(vec3 srcPosition, vec3 srcNormal, vec2 texCoord) {
    // Calculate destination position
    vec3 dstPosition = calcPosition(texCoord);

    // Calculate the amount of ambient occlusion between two points.
    // This is similar to diffuse lighting in that objects directly above the
    // fragment cast the hardest shadow and objects closer to the horizon have
    // very minimal effect.
    vec3 position = dstPosition - srcPosition;
    float intensity = max(dot(normalize(position), srcNormal) - gOccluderBias, 0.0f);

    // Attenuate the occlusion. This is similar to attenuating a lighting source,
    // in that the further the distance between two points, the less effect
    // AO has on the fragment.
    float distance = length(position);
    float attenuation = 1.0f / (gAttenuation.x + (gAttenuation.y * distance));

    return intensity * attenuation;
}

void main() {
    vec2 texCoord = calcTexCoord();

    vec2 randomJitter = normalize(texture(gRandomMap, calcFragCoord(texCoord)).xy * 2.0f - 2.0f);
    vec3 srcNormal = neoTexture2D(gNormalMap, texCoord).rgb * 2.0f - 1.0f;

    vec3 srcPosition = calcPosition(texCoord);

    float depth = neoTexture2D(gDepthMap, texCoord).r * 2.0f - 1.0f * 0.5 + 0.5;
    float kernelRadius = gSamplingRadius * (1.0f - depth);

    float sin45 = sin(M_PI / 4.0f);

    float occlusion = 0.0f;

    vec2 kernel[4];
    kernel[0] = vec2(0.0f, 1.0f);
    kernel[1] = vec2(1.0f, 0.0f);
    kernel[2] = vec2(0.0f, -1.0f);
    kernel[3] = vec2(-1.0f, 0.0f);

    for (int i = 0; i < 4; ++i) {
        vec2 k1 = reflect(kernel[i], randomJitter);
        vec2 k2 = vec2(k1.x * sin45 - k1.y * sin45,
                       k1.x * sin45 + k1.y * sin45);

        occlusion += samplePixels(srcPosition, srcNormal, texCoord + k1 * kernelRadius);
        occlusion += samplePixels(srcPosition, srcNormal, texCoord + k2 * kernelRadius * 0.75f);
        occlusion += samplePixels(srcPosition, srcNormal, texCoord + k1 * kernelRadius * 0.50f);
        occlusion += samplePixels(srcPosition, srcNormal, texCoord + k2 * kernelRadius * 0.25f);
    }

    fragColor.r = 1.0f - clamp(occlusion / 16.0f, 0.0f, 1.0f);
}
