struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct directionalLight {
    baseLight base;
    vec3 direction;
};

uniform sampler2D gPositionMap;
uniform sampler2D gColorMap;
uniform sampler2D gNormalMap;

uniform vec3 gEyeWorldPosition;
uniform float gMatSpecularIntensity;
uniform float gMatSpecularPower;

uniform vec2 gScreenSize;

uniform directionalLight gDirectionalLight;

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

vec4 calcDirectionalLight(vec3 worldPosition, vec3 normal) {
    return calcLightGeneric(gDirectionalLight.base, gDirectionalLight.direction,
        worldPosition, normal);
}

#define EPSILON 0.00001f

vec3 decodeNormal(vec2 encodedNormal) {
    vec2 p = encodedNormal;

    // Find z sign
    float zsign = sign(1.0f - abs(p.x) - abs(p.y));

    // Map outer triangles to center if encoded z is negative
    float z_is_negative = max(-zsign, 0.0f);
    vec2 p_sign = sign(p);
    p_sign = sign(p_sign + vec2(0.5f, 0.5f));

    // Reflection
    p -= z_is_negative * (dot(p, p_sign) - 1.0f) * p_sign;

    // Convert square to unit circle
    float r = abs(p.x) + abs(p.y);
    float d = length(p) + EPSILON;
    vec2 q = p * r / d;

    // Deproject unit circle to sphere
    float den = 2.0f / (dot(q, q) + 1.0f);
    vec3 v = vec3(den * q, zsign * (den - 1.0f));
    return v;
}

void main(void) {
    vec2 texCoord = calcTexCoord();
    vec3 worldPosition = texture(gPositionMap, texCoord).xyz;
    vec3 color = texture(gColorMap, texCoord).xyz;
    vec3 normal = decodeNormal(texture(gNormalMap, texCoord).xy);

    fragColor = vec4(color, 1.0f) * calcDirectionalLight(worldPosition, normal);
    //fragColor = calcDirectionalLight(worldPosition, normal);
}
