#include <shaders/screenspace.h>
#include <shaders/depth.h>
#include <shaders/light.h>
#include <shaders/fog.h>
#include <shaders/utils.h>

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;
uniform neoSampler2D gOcclusionMap;

uniform directionalLight gDirectionalLight;

out vec4 fragColor;

void main() {
#if defined(USE_DEBUG_DEPTH) || defined(USE_DEBUG_NORMAL) || defined(USE_DEBUG_POSITION) || defined(USE_DEBUG_SSAO)
#if defined(USE_DEBUG_DEPTH)
    // Depth buffer visualization
    fragColor.rgb = vec3(calcLinearDepth(calcTexCoord()));
#elif defined(USE_DEBUG_NORMAL)
    // Normal buffer visualization
    vec2 texCoord = calcTexCoord();
    vec4 normalDecode = neoTexture2D(gNormalMap, texCoord);
    vec3 normalMap = normalize(normalDecode.rgb * 2.0f - 1.0f);
    fragColor.rgb = normalMap * 0.5f + 0.5f;
#elif defined(USE_DEBUG_POSITION)
    // Position visualization
    vec2 texCoord = calcTexCoord();
    vec3 worldPosition = calcPosition(texCoord);
    fragColor.rgb = worldPosition;
#elif defined(USE_DEBUG_SSAO)
    // Ambient occlusion visualization
    vec2 texCoord = calcTexCoord();
    vec2 fragCoord = texCoord / 2.0f; // Half resolution adjustment
    float occlusionMap = neoTexture2D(gOcclusionMap, fragCoord).r;
    fragColor.rgb = vec3(occlusionMap);
#endif //! USE_DEBUG_DEPTH
#else //+ USE_DEBUG_{DEPTH|NORMAL|POSITION|SSAO}
    vec2 texCoord = calcTexCoord();
    vec4 colorMap = neoTexture2D(gColorMap, texCoord);
    vec4 normalDecode = neoTexture2D(gNormalMap, texCoord);
    vec3 normalMap = normalize(normalDecode.rgb * 2.0f - 1.0f);
    vec3 worldPosition = calcPosition(texCoord);
    vec2 specMap = vec2(colorMap.a * 2.0f, exp2(normalDecode.a * 8.0f));
#ifdef USE_SSAO
    vec2 fragCoord = texCoord / 2.0f; // Half resolution adjustment
    float occlusionMap = neoTexture2D(gOcclusionMap, fragCoord).r;

    fragColor = MASK_ALPHA(colorMap *
                           occlusionMap *
                           calcDirectionalLight(gDirectionalLight,
                                                worldPosition,
                                                normalMap,
                                                specMap));
#else //+ USE_SSAO
    fragColor = MASK_ALPHA(colorMap *
                           calcDirectionalLight(gDirectionalLight,
                                                worldPosition,
                                                normalMap,
                                                specMap));
#endif //! USE_SSAO

#ifdef USE_FOG
    float fogFactor = calcFogFactor(gFog, calcLinearDepth(texCoord));
    fragColor = vec4(mix(fragColor.rgb, gFog.color, fogFactor), fogFactor);
#endif //! USE_FOG

#endif //! USE_DEBUG{DEPTH|NORMAL|POSITION|SSAO}

}
