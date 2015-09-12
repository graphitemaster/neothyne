#ifndef LIGHT_HDR
#define LIGHT_HDR

uniform vec3 gEyeWorldPosition;

#ifdef USE_SHADOWMAP
uniform sampler2DShadow gShadowMap;
#endif

struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct pointLight {
    baseLight base;
    vec3 position;
    float radius;
};

struct spotLight {
    pointLight base;
    vec3 direction;
    float cutOff;
};

struct directionalLight {
    baseLight base;
    vec3 direction;
};

#ifdef USE_SHADOWMAP
const float kShadowBias = 0.00001f;

uniform mat4 gLightWVP;

float calcShadowFactor(vec3 worldPosition) {
    vec4 position = gLightWVP * vec4(worldPosition, 1.0f);
    vec3 shadowCoord = 0.5f * (position.xyz / position.w) + 0.5f;

    vec2 offset = 1.0f / gScreenSize;
    float factor = 0.0f;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offsets = vec2(x, y) * offset;
            vec3 uvc = vec3(shadowCoord.xy + offsets, shadowCoord.z + kShadowBias);
            factor += texture(gShadowMap, uvc);
        }
    }

    return factor;
}
#endif

vec4 calcLight(baseLight light,
               vec3 lightDirection,
               vec3 worldPosition,
               vec3 normal,
               vec2 spec,
               float shadowFactor)
{
    float attenuation = light.ambient;

    float diffuseFactor = dot(normal, -lightDirection);
    if (diffuseFactor > 0.0f) {
        attenuation += light.diffuse * diffuseFactor * shadowFactor;
        vec3 vertexToEye = normalize(gEyeWorldPosition - worldPosition);
        vec3 lightReflect = reflect(lightDirection, normal);
        float specularFactor = dot(vertexToEye, lightReflect);
        if (specularFactor > 0.0f)
            attenuation += spec.x * pow(specularFactor, spec.y) * shadowFactor;
    }

    return vec4(light.color, 1.0f) * attenuation;
}

vec4 calcDirectionalLight(directionalLight light,
                          vec3 worldPosition,
                          vec3 normal,
                          vec2 spec)
{
    return calcLight(light.base, light.direction, worldPosition, normal, spec, 1.0f);
}

vec4 calcPointLight(pointLight light,
                    vec3 worldPosition,
                    vec3 normal,
                    vec2 spec)
{
    vec3 lightDirection = worldPosition - light.position;
    float distance = length(lightDirection);
    lightDirection = normalize(lightDirection);
#ifdef USE_SHADOWMAP
    float shadowFactor = calcShadowFactor(worldPosition);
#else
    const float shadowFactor = 1.0f;
#endif

    vec4 color = vec4(0.0f);
    if (distance < light.radius) {
        color = calcLight(light.base,
                          lightDirection,
                          worldPosition,
                          normal,
                          spec,
                          shadowFactor);
    }

    float attenuation = 1.0f - clamp(distance / light.radius, 0.0f, 1.0f);
    return color * attenuation;
}

vec4 calcSpotLight(spotLight light,
                   vec3 worldPosition,
                   vec3 normal,
                   vec2 spec)
{
    vec3 lightToPixel = normalize(worldPosition - light.base.position);
    float spotFactor = dot(lightToPixel, light.direction);

    if (spotFactor > light.cutOff) {
        vec4 color = calcPointLight(light.base,
                                    worldPosition,
                                    normal,
                                    spec);
        return color * (1.0f - (1.0f - spotFactor) * 1.0f / (1.0f - light.cutOff));
    }
    return vec4(0.0f);
}

#endif
