#include <shaders/screenspace.h>
#include <shaders/depth.h>
#include <shaders/light.h>

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;

uniform pointLight gPointLight;

out vec4 fragColor;

void main() {
    vec2 texCoord = calcTexCoord();
    vec4 colorMap = neoTexture2D(gColorMap, texCoord);
    vec4 normalDecode = neoTexture2D(gNormalMap, texCoord);
    vec3 normalMap = normalize(normalDecode.rgb * 2.0f - 1.0f);
    vec3 worldPosition = calcPosition(texCoord);
    vec2 specMap = vec2(colorMap.a * 2.0f, exp2(normalDecode.a * 8.0f));

    fragColor = vec4(colorMap.rgb, 1.0f)
        * calcPointLight(gPointLight, worldPosition, normalMap, specMap);
}
