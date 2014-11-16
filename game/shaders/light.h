#ifndef LIGHT_HDR
#define LIGHT_HDR

uniform vec3 gEyeWorldPosition;

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

vec4 calcLight(baseLight light, vec3 lightDirection, vec3 worldPosition, vec3 normal, vec2 spec) {
    vec4 ambientColor = vec4(light.color, 1.0f) * light.ambient;
    float diffuseFactor = dot(normal, -lightDirection);

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

vec4 calcDirectionalLight(directionalLight light, vec3 worldPosition, vec3 normal, vec2 spec) {
    return calcLight(light.base, light.direction, worldPosition, normal, spec);
}

vec4 calcPointLight(pointLight light, vec3 worldPosition, vec3 normal, vec2 spec) {
    vec3 lightDirection = worldPosition - light.position;
    float distance = length(lightDirection);
    lightDirection = normalize(lightDirection);

    vec4 color = distance < light.radius
        ? calcLight(light.base, lightDirection, worldPosition, normal, spec) : vec4(0.0);

    float attenuation = 1.0f - clamp(distance / light.radius, 0.0f, 1.0f);
    return color * attenuation;
}

vec4 calcSpotLight(spotLight light, vec3 worldPosition, vec3 normal, vec2 spec) {
    vec3 lightToPixel = normalize(worldPosition - light.base.position);
    float spotFactor = dot(lightToPixel, light.direction);

    if (spotFactor > light.cutOff) {
        vec4 color = calcPointLight(light.base, worldPosition, normal, spec);
        return color * (1.0f - (1.0f - spotFactor) * 1.0f / (1.0f - light.cutOff));
    }
    return vec4(0.0f);
}

#endif
