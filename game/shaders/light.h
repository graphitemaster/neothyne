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

#endif
