#include <shaders/screenspace.h>
#include <shaders/depth.h>
#include <shaders/light.h>
#include <shaders/fog.h>

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;
uniform neoSampler2D gOcclusionMap;

uniform directionalLight gDirectionalLight;

out vec4 fragColor;

void main() {
#if defined(USE_DEBUG_DEPTH) || defined(USE_DEBUG_NORMAL) || defined(USE_DEBUG_POSITION) || defined(USE_DEBUG_SSAO)
#if defined(USE_DEBUG_DEPTH)
    // Depth buffer visualization
    vec2 texCoord = calcTexCoord();
    float z = neoTexture2D(gDepthMap, texCoord).r;
    float c = (2.0f * gScreenFrustum.x) / (gScreenFrustum.y +
        gScreenFrustum.x - z * (gScreenFrustum.y - gScreenFrustum.x));
    fragColor.rgb = vec3(c);
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

    fragColor = vec4(colorMap.rgb, 1.0f)
        * occlusionMap
        * calcDirectionalLight(gDirectionalLight, worldPosition, normalMap, specMap);
#else //+ USE_SSAO
    fragColor = vec4(colorMap.rgb, 1.0f)
        * calcDirectionalLight(gDirectionalLight, worldPosition, normalMap, specMap);
#endif //! USE_SSAO

#ifdef USE_FOG
    float z = neoTexture2D(gDepthMap, texCoord).r;
    float c = (2.0f * gScreenFrustum.x) / (gScreenFrustum.y +
        gScreenFrustum.x - z * (gScreenFrustum.y - gScreenFrustum.x));
    fragColor = mix(fragColor, vec4(gFog.color, 1.0f), calcFogFactor(gFog, c));
#endif //! USE_FOG

#endif //! USE_DEBUG{DEPTH|NORMAL|POSITION|SSAO}

}
