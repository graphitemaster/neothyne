#include <shaders/screenspace.h>
#include <shaders/depth.h>

#define M_PI 3.1415926535897932384626433832795

uniform neoSampler2D gNormalMap;
uniform neoSampler2D gRandomMap;

uniform float gOccluderBias;
uniform float gSamplingRadius;
uniform vec2 gKernel[kKernelSize];
uniform vec2 gAttenuation; // { constant, linear }

out vec4 fragColor;

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
    vec2 texCoord = calcTexCoord() * 2.0f; // Half resultion adjustment

    float depth = calcDepth(texCoord);
    if (depth == 1.0f) {
        fragColor.r = 1.0f;
    } else {
        vec2 randomJitter = normalize(neoTexture2D(gRandomMap, texCoord).xy * 2.0f - 2.0f);
        vec3 srcNormal = neoTexture2D(gNormalMap, texCoord).rgb * 2.0f - 1.0f;
        float srcDepth = evalLinearDepth(texCoord, depth);
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

#ifndef HAS_TEXTURE_RECTANGLE
        vec2 texelSize = 1.0f / (gScreenSize / 2.0f); // Half resolution adjustment

        k10 *= texelSize;
        k11 *= texelSize;
        k12 *= texelSize;
        k13 *= texelSize;
        k20 *= texelSize;
        k21 *= texelSize;
        k22 *= texelSize;
        k23 *= texelSize;
#endif

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
}
