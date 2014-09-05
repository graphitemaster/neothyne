struct lightAttenuation {
    float constant;
    float linear;
    float exp;
};

struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct pointLight {
    baseLight base;
    vec3 position;
    lightAttenuation attenuation;
};

uniform sampler2D gPositionMap;
uniform sampler2D gColorMap;
uniform sampler2D gNormalMap;

uniform vec3 gEyeWorldPosition;
uniform float gMatSpecularIntensity;
uniform float gMatSpecularPower;

uniform pointLight gPointLight;

uniform vec2 gScreenSize;

out vec4 fragColor;

vec2 calcTexCoord(void) {
    return gl_FragCoord.xy / gScreenSize;
}

vec4 calcLightGeneric(baseLight light, vec3 lightDirection, vec3 worldPosition, vec3 normal) {
    vec4 ambientColor = vec4(light.color, 1.0f) * light.ambient;
    float diffuseFactor = dot(normal, -lightDirection);

    vec4 diffuseColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec4 specularColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (diffuseFactor > 0.0f) {
        diffuseColor = vec4(light.color, 1.0f) * light.diffuse * diffuseFactor;

        vec3 vertexToEye = normalize(gEyeWorldPosition - worldPosition);
        vec3 lightReflect = normalize(reflect(lightDirection, normal));
        float specularFactor = dot(vertexToEye, lightReflect);
        specularFactor = pow(specularFactor, gMatSpecularPower);
        if (specularFactor > 0.0f)
            specularColor = vec4(light.color, 1.0f) * gMatSpecularIntensity * specularFactor;
    }

    return ambientColor + diffuseColor + specularColor;
}

vec4 calcPointLight(vec3 worldPosition, vec3 normal) {
    vec3 lightDirection = worldPosition - gPointLight.position;
    float distance = length(lightDirection);
    lightDirection /= distance;

    vec4 color = calcLightGeneric(gPointLight.base, lightDirection, worldPosition, normal);
    float attenuation = gPointLight.attenuation.constant +
                        gPointLight.attenuation.linear * distance +
                        gPointLight.attenuation.exp * distance * distance;

    return color / max(1.0f, attenuation);
}

void main(void) {
    vec2 texCoord = calcTexCoord();
    vec3 worldPosition = texture(gPositionMap, texCoord).xyz;
    vec3 color = texture(gColorMap, texCoord).xyz;
    vec3 normal = normalize(texture(gNormalMap, texCoord).xyz);

    fragColor = vec4(color, 1.0f) * calcPointLight(worldPosition, normal);
}
