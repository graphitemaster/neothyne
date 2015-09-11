#ifndef LIGHT_HDR
#define LIGHT_HDR

uniform vec3 gEyeWorldPosition;

#ifdef USE_SHADOWMAP
uniform sampler2D gShadowMap;

in vec4 lightSpacePosition0;
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
const float kShadowEpsilon = 0.00001f;

uniform mat4 gLightWVP;

float calcShadowFactor() {
    vec3 shadowCoord = 0.5f * calcShadowPosition(gLightWVP, calcTexCoord()) + 0.5f;
    float depth = texture(gShadowMap, shadowCoord.xy).x;
    return (depth < (shadowCoord.z + kShadowEpsilon)) ? 0.5f : 1.0f;
}
#endif

vec4 calcLight(baseLight light,
               vec3 lightDirection,
               vec3 worldPosition,
               vec3 normal,
               vec2 spec,
               float shadowFactor)
{
    vec4 ambientColor = vec4(light.color, 1.0f) * light.ambient;
    float diffuseFactor = shadowFactor * dot(normal, -lightDirection);

    vec4 diffuseColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec4 specularColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (diffuseFactor > 0.0f) {
        diffuseColor = vec4(light.color, 1.0f) * light.diffuse * diffuseFactor;

        vec3 vertexToEye = normalize(gEyeWorldPosition - worldPosition);

        vec3 lightReflect = reflect(lightDirection, normal);
        float specularFactor = dot(vertexToEye, lightReflect);
        specularFactor = pow(specularFactor, spec.y);
        if (specularFactor > 0.0f)
            specularColor = vec4(light.color, 1.0f) * spec.x * specularFactor;
    }

    return ambientColor + diffuseColor + specularColor;
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
    float shadowFactor = calcShadowFactor();
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
