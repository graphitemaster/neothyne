#version 330

in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 worldPos0;

const int kMaxPointLights = 20;

struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct directionalLight {
    struct baseLight base;
    vec3 direction;
};

struct lightAttenuation {
    float constant;
    float linear;
    float exp;
};

struct pointLight {
    struct baseLight base;
    vec3 position;
    lightAttenuation attenuation;
};

// textures
uniform sampler2D gSampler;     // diffuse
uniform sampler2D gNormalMap;   // normal


// specularity
uniform vec3 gEyeWorldPos;
uniform float gMatSpecIntensity;
uniform float gMatSpecPower;

// lights
uniform int gNumPointLights;
uniform directionalLight gDirectionalLight;
uniform pointLight gPointLights[kMaxPointLights];

// output
smooth out vec4 fragColor;

vec4 calcLightGeneric(baseLight light, vec3 lightDirection, vec3 normal, float shadowFactor) {
    vec4 ambientColor = vec4(light.color, 1.0f) * light.ambient;
    float diffuseFactor = dot(normal, -lightDirection);

    vec4 diffuseColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec4 specularColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (diffuseFactor > 0.0f) {
        diffuseColor = vec4(light.color, 1.0f) * light.diffuse * diffuseFactor;

        vec3 vertexToEye = normalize(gEyeWorldPos - worldPos0);
        vec3 lightReflect = normalize(reflect(lightDirection, normal));

        float specularFactor = dot(vertexToEye, lightReflect);
        specularFactor = pow(specularFactor, gMatSpecPower);

        if (specularFactor > 0.0f) {
            specularColor = vec4(light.color, 1.0f) * gMatSpecIntensity * specularFactor;
        }
    }

    return ambientColor + shadowFactor * (diffuseColor + specularColor);
}

vec4 calcLightDirectional(vec3 normal) {
    return calcLightGeneric(gDirectionalLight.base, gDirectionalLight.direction, normal, 1.0f);
}

vec4 calcLightPoint(int index, vec3 normal) {
    vec3 lightDirection = worldPos0 - gPointLights[index].position;
    float distance = length(lightDirection);
    lightDirection = normalize(lightDirection);

    vec4 color = calcLightGeneric(gPointLights[index].base, lightDirection, normal, 1.0f);
    float attenuation = gPointLights[index].attenuation.constant +
                        gPointLights[index].attenuation.linear * distance +
                        gPointLights[index].attenuation.exp * distance * distance;

    return color / attenuation;
}

vec3 calcBump(void) {
    vec3 normal = normalize(normal0);
    vec3 tangent = normalize(tangent0);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = cross(tangent, normal);
    vec3 bumpMapNormal = texture(gNormalMap, texCoord0).xyz;
    bumpMapNormal = 2.0f * bumpMapNormal - vec3(1.0f, 1.0f, 1.0f);
    vec3 newNormal;
    mat3 tbn = mat3(tangent, bitangent, normal);
    newNormal = tbn * bumpMapNormal;
    newNormal = normalize(newNormal);
    return newNormal;
}

void main() {
    vec3 normal = calcBump();
    vec4 totalLight = calcLightDirectional(normal);
    vec4 sampledColor = texture2D(gSampler, texCoord0.xy);

    for (int i = 0; i < gNumPointLights; i++)
        totalLight += calcLightPoint(i, normal);

    fragColor = sampledColor * totalLight;
    fragColor.a = 1.0f;
}
