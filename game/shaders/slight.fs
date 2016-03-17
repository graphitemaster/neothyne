#include <shaders/screen.h>
#include <shaders/depth.h>
#include <shaders/light.h>
#include <shaders/utils.h>

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;

uniform spotLight gSpotLight;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    vec4 colorMap = neoTexture2D(gColorMap, texCoord);
    vec4 normalDecode = neoTexture2D(gNormalMap, texCoord);
    vec3 normalMap = normalDecode.rgb * 2.0f - 1.0f;
    vec3 worldPosition = calcPosition(texCoord);
    vec2 specMap = vec2(colorMap.a * 2.0f, exp2(normalDecode.a * 8.0f));
    fragColor = MASK_ALPHA(colorMap *
                           calcSpotLight(gSpotLight,
                                         worldPosition,
                                         normalMap,
                                         specMap));
}
