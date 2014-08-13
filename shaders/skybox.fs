in vec3 texCoord0;
in vec3 worldPos0;

out vec4 fragColor;

uniform samplerCube gCubemap;

void main(void) {
    vec3 color = texture(gCubemap, texCoord0).rgb;

    // vertical fog gradiant
    // TODO: only enable if map is uses fog
    vec3 cameraToWorld = texCoord0 - worldPos0;
    vec3 eyeDir = normalize(cameraToWorld);
    float vertFogGrad = 1.0f - clamp(dot(-eyeDir, vec3(0.0f, 1.0f, 0.0f)) - 0.1f, 0.0f, 0.25f) / 0.25f;
    vec3 fogColor = vec3(0.8f, 0.8f, 0.8f); // TODO: make map var skyFogColor
    vec3 verticalMixture = mix(color, fogColor, vertFogGrad);
    vec3 cloudSky = vec3(0.8f, 0.8f, 0.8f); // TODO: make map var skyFogCloudColor
    const float kSkyFogDensity = 0.45; // TODO: make map var skyFogDensity
    fragColor = vec4(mix(verticalMixture, cloudSky, kSkyFogDensity), 1.0f);
}
