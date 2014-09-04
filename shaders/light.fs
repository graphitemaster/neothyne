in vec3 normal0;
in vec2 texCoord0;
in vec3 tangent0;
in vec3 worldPos0;

const int kFogNone = 0;
const int kFogLinear = 1;
const int kFogExp = 2;
const int kFogExp2 = 3;

struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct directionalLight {
    baseLight base;
    vec3 direction;
};

struct lightAttenuation {
    float constant;
    float linear;
    float exp;
};

struct pointLight {
    baseLight base;
    vec3 position;
    lightAttenuation attenuation;
};

struct spotLight {
    pointLight base;
    vec3 direction;
    float cutOff;
};

struct fog {
    float density;
    vec4 color;
    float start;
    float end;
    int method;
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
uniform int gNumSpotLights;
uniform directionalLight gDirectionalLight;
uniform pointLight gPointLights[kMaxPointLights];
uniform spotLight gSpotLights[kMaxSpotLights];

// fog
uniform fog gFog;

// output
out vec4 fragColor;

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

vec4 calcLightPoint(pointLight light, vec3 normal) {
    vec3 lightDirection = worldPos0 - light.position;
    float distance = length(lightDirection);
    lightDirection /= distance;

    vec4 color = calcLightGeneric(light.base, lightDirection, normal, 1.0f);
    float attenuation = light.attenuation.constant +
                        light.attenuation.linear * distance +
                        light.attenuation.exp * distance * distance;

    return color / attenuation;
}

vec4 calcLightSpot(spotLight light, vec3 normal) {
    vec3 lightToPixel = normalize(worldPos0 - light.base.position);
    float spotFactor = dot(lightToPixel, light.direction);

    if (spotFactor > light.cutOff)
        return calcLightPoint(light.base, normal) * (1.0f - (1.0f - spotFactor) * 1.0f / (1.0f - light.cutOff));
    return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

vec3 calcBump(void) {
    vec3 tangent = normalize(tangent0 - dot(tangent0, normal0) * normal0);
    vec3 bitangent = cross(tangent, normal0);
    vec3 bumpMapNormal = texture(gNormalMap, texCoord0).xyz;
    bumpMapNormal = 2.0f * bumpMapNormal - vec3(1.0f, 1.0f, 1.0f);
    mat3 tbn = mat3(tangent, bitangent, normal0);
    return normalize(tbn * bumpMapNormal);
}

float calcFog(vec4 fragColor) {
    float result;
    float fogCoord = gl_FragCoord.z / gl_FragCoord.w;
    switch (gFog.method) {
        case kFogLinear:
            result = (gFog.end - fogCoord) / (gFog.end - gFog.start);
            break;
        case kFogExp:
            result = exp(-gFog.density * fogCoord);
            break;
        case kFogExp2:
            result = exp(-pow(gFog.density * fogCoord, 2.0f));
            break;
    }
    result = 1.0f - clamp(result, 0.0f, 1.0f);
    return result;
}

const vec2 maskAlpha = vec2(1.0f, 0.0f);

void main() {
    vec3 normal = calcBump();
    vec4 totalLight = calcLightDirectional(normal);
    vec4 sampledColor = texture2D(gSampler, texCoord0.xy);

    for (int i = 0; i < gNumPointLights; i++)
        totalLight += calcLightPoint(gPointLights[i], normal);

    for (int i = 0; i < gNumSpotLights; i++)
        totalLight += calcLightSpot(gSpotLights[i], normal);

    vec4 transposedColor = sampledColor * totalLight;
    if (gFog.method != kFogNone)
        transposedColor = mix(transposedColor, gFog.color, calcFog(transposedColor));

    fragColor = transposedColor.xyzw * maskAlpha.xxxy + maskAlpha.yyyx;
}
