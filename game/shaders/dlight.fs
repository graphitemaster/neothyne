#include <shaders/screenspace.h>
#include <shaders/depth.h>
#include <shaders/light.h>

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;
#ifdef USE_SSAO
uniform neoSampler2D gOcclusionMap;
#endif

uniform directionalLight gDirectionalLight;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    vec4 colorMap = neoTexture2D(gColorMap, texCoord);
    vec4 normalDecode = neoTexture2D(gNormalMap, texCoord);
    vec3 normalMap = normalDecode.rgb * 2.0f - 1.0f;
    vec3 worldPosition = calcPosition(texCoord);
    vec2 specMap = vec2(colorMap.a * 2.0f, exp2(normalDecode.a * 8.0f));

    // Uncomment to visualize depth buffer
    //float z = neoTexture2D(gDepthMap, texCoord).r;
    //float c = (2.0f * gScreenFrustum.x) / (gScreenFrustum.y +
    //    gScreenFrustum.x - z * (gScreenFrustum.y - gScreenFrustum.x));
    //fragColor.rgb = vec3(c);

    // Uncomment to visualize normal buffer
    //fragColor.rgb = normalMap * 0.5 + 0.5;

#ifdef USE_SSAO
    vec2 fragCoord = texCoord / 2.0f; // Half resolution adjustment
    float occlusionMap = neoTexture2D(gOcclusionMap, fragCoord).r;

    // Uncomment to visualize occlusion
    //fragColor.rgb = vec3(occlusionMap);

    fragColor = vec4(colorMap.rgb, 1.0f)
        * occlusionMap
        * calcDirectionalLight(gDirectionalLight, worldPosition, normalMap, specMap);
#else
    fragColor = vec4(colorMap.rgb, 1.0f)
        * calcDirectionalLight(gDirectionalLight, worldPosition, normalMap, specMap);
#endif
}
