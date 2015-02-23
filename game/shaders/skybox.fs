#include <shaders/fog.h>

in vec3 texCoord0;
in vec3 worldPos0;

out vec4 fragColor;

uniform samplerCube gCubemap;

void main() {
#ifdef USE_FOG
    vec3 color = texture(gCubemap, texCoord0).rgb;
    vec3 cameraToWorld = texCoord0 - worldPos0;
    vec3 eyeDir = normalize(cameraToWorld);

    // Vertical gradient below certain height so that world fog "reaches" into
    // the sky. TODO: Should the reaching distance be a map variable?
    float vertFogGrad = 1.0f - clamp(dot(-eyeDir, vec3(0.0f, 1.0f, 0.0f)) - 0.1f, 0.0f, 0.25f) / 0.25f;
    vec3 verticalMixture = mix(color, gFog.color, vertFogGrad);
    vec3 cloudSky = vec3(0.5f, 0.5f, 0.5f);
    fragColor = vec4(mix(verticalMixture, cloudSky, gFog.density), 1.0f);
#else
    fragColor = vec4(texture(gCubemap, texCoord0).rgb, 1.0f);
#endif
}
