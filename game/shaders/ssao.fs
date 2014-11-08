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

uniform vec2 gScreenSize;    // { width, height }
uniform vec2 gScreenFrustum; // { near, far }

uniform float gOccluderBias;
uniform float gSamplingRadius;
uniform vec2 gKernel[kKernelSize];
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

float calcLinearDepth(vec2 texCoord) {
    return (2.0f * gScreenFrustum.x)
        / (gScreenFrustum.y + gScreenFrustum.x -
            neoTexture2D(gDepthMap, texCoord).x * (gScreenFrustum.y - gScreenFrustum.x));
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
    float distance = length(position);
    float intensity = max(dot(normalize(position), srcNormal) - gOccluderBias, 0.0f);

    // Attenuate the occlusion. This is similar to attenuating a lighting source,
    // in that the further the distance between two points, the less effect
    // AO has on the fragment.
    float attenuation = step(distance, 16.0f) / (gAttenuation.x + (gAttenuation.y * distance));
    return intensity * attenuation;
}

void main() {
    vec2 texCoord = calcTexCoord();

    vec2 randomJitter = normalize(texture(gRandomMap, calcFragCoord(texCoord)).xy * 2.0f - 2.0f);
    vec3 srcNormal = neoTexture2D(gNormalMap, texCoord).rgb * 2.0f - 1.0f;

    float srcDepth = calcLinearDepth(texCoord);
    vec3 srcPosition = calcPosition(texCoord);

    float kernelRadius = gSamplingRadius * (1.0f - srcDepth);

    float occlusion = 0.0f;

    // Unrolled completely
    vec2 k10 = reflect(gKernel[0], randomJitter);
    vec2 k11 = reflect(gKernel[1], randomJitter);
    vec2 k12 = reflect(gKernel[2], randomJitter);
    vec2 k13 = reflect(gKernel[3], randomJitter);

    vec2 k20 = vec2(k10.x * kKernelFactor - k10.y * kKernelFactor,
                    k10.x * kKernelFactor + k10.y * kKernelFactor);
    vec2 k21 = vec2(k11.x * kKernelFactor - k11.y * kKernelFactor,
                    k11.x * kKernelFactor + k11.y * kKernelFactor);
    vec2 k22 = vec2(k12.x * kKernelFactor - k12.y * kKernelFactor,
                    k12.x * kKernelFactor + k12.y * kKernelFactor);
    vec2 k23 = vec2(k13.x * kKernelFactor - k13.y * kKernelFactor,
                    k13.x * kKernelFactor + k13.y * kKernelFactor);

    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k10 * kernelRadius);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k20 * kernelRadius * 0.75f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k10 * kernelRadius * 0.50f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k20 * kernelRadius * 0.25f);

    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k11 * kernelRadius);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k21 * kernelRadius * 0.75f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k11 * kernelRadius * 0.50f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k21 * kernelRadius * 0.25f);

    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k12 * kernelRadius);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k22 * kernelRadius * 0.75f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k12 * kernelRadius * 0.50f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k22 * kernelRadius * 0.25f);

    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k13 * kernelRadius);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k23 * kernelRadius * 0.75f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k13 * kernelRadius * 0.50f);
    occlusion += samplePixels(srcPosition, srcNormal, texCoord + k23 * kernelRadius * 0.25f);

    fragColor.r = 1.0f - clamp(occlusion / (kKernelSize * kKernelSize), 0.0f, 1.0f);
}
