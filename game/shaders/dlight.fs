#ifdef HAS_TEXTURE_RECTANGLE
#  extension GL_ARB_texture_rectangle : enable
#  define neoSampler2D sampler2DRect
#  define neoTexture2D texture2DRect
#else
#  define neoSampler2D sampler2D
#  define neoTexture2D texture2D
#endif

struct baseLight {
    vec3 color;
    float ambient;
    float diffuse;
};

struct directionalLight {
    baseLight base;
    vec3 direction;
};

uniform neoSampler2D gColorMap;
uniform neoSampler2D gNormalMap;
uniform neoSampler2D gDepthMap;

uniform vec3 gEyeWorldPosition;
uniform float gMatSpecularIntensity;
uniform float gMatSpecularPower;

uniform vec2 gScreenSize;
uniform vec2 gScreenFrustum;

uniform mat4 gInverse;

uniform directionalLight gDirectionalLight;

out vec4 fragColor;

vec2 calcTexCoord() {
#ifdef HAS_TEXTURE_RECTANGLE
    return gl_FragCoord.xy;
#else
    return gl_FragCoord.xy / gScreenSize;
#endif
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

vec3 calcPosition(vec2 texCoord) {
#ifdef HAS_TEXTURE_RECTANGLE
    vec2 fragCoord = texCoord / gScreenSize;
#else
    vec2 fragCoord = texCoord;
#endif
    float depth = neoTexture2D(gDepthMap, fragCoord).r * 2.0f - 1.0f;
    vec4 pos = vec4(fragCoord * 2.0f - 1.0f, depth, 1.0f);
    pos = gInverse * pos;
    return pos.xyz / pos.w;
}

void main() {
    vec2 texCoord = calcTexCoord();
    vec3 worldPosition = calcPosition(texCoord);

    // Uncomment to visualize depth buffer
    //float z = neoTexture2D(gDepthMap, texCoord).r;
    //float c = (2.0f * gScreenFrustum.x) / (gScreenFrustum.y +
    //    gScreenFrustum.x - z * (gScreenFrustum.y - gScreenFrustum.x));
    //fragColor.rgb = vec3(c);

    vec3 color = neoTexture2D(gColorMap, texCoord).xyz;
    vec3 normal = decodeNormal(neoTexture2D(gNormalMap, texCoord).xy);

    fragColor = vec4(color, 1.0f) * calcDirectionalLight(worldPosition, normal);
}
