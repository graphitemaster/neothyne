#ifndef LIGHT_HDR
#define LIGHT_HDR

uniform vec3 gEyeWorldPosition;

#ifdef USE_SHADOWMAP
uniform sampler2DShadow gShadowMap;
#endif

// { { r, g, b, diffuse }, { pos.x, pos.y, pos.z, radius } }
#define pointLight       vec4[2]
// { { r, g, b, diffuse }, { pos.x, pos.y, pos.z, radius }, { dir.x, dir.y, dir.z, cutoff } }
#define spotLight        vec4[3]
// { { r, g, b, ambient }, { dir.x, dir.y, dir.z, diffuse } }
#define directionalLight vec4[2]

// point light utilities
#define PL_COLOR(PL)     (PL)[0].xyz
#define PL_DIFFUSE(PL)   (PL)[0].w
#define PL_POSITION(PL)  (PL)[1].xyz
#define PL_RADIUS(PL)    (PL)[1].w

// spot light utilities
#define SL_COLOR(SL)     (SL)[0].xyz
#define SL_DIFFUSE(SL)   (SL)[0].w
#define SL_POSITION(SL)  (SL)[1].xyz
#define SL_RADIUS(SL)    (SL)[1].w
#define SL_DIRECTION(SL) (SL)[2].xyz
#define SL_CUTOFF(SL)    (SL)[2].w

// directional light utilities
#define DL_COLOR(DL)     (DL)[0].xyz
#define DL_AMBIENT(DL)   (DL)[0].w
#define DL_DIRECTION(DL) (DL)[1].xyz
#define DL_DIFFUSE(DL)   (DL)[1].w

#ifdef USE_SHADOWMAP
uniform mat4 gLightWVP;

float calcShadowFactor(vec3 shadowCoord) {
    vec2 scale = 1.0f / textureSize(gShadowMap, 0);
    shadowCoord.xy *= textureSize(gShadowMap, 0);
    vec2 offset = fract(shadowCoord.xy - 0.5f);
    shadowCoord.xy -= offset*0.5f;
    vec4 size = vec4(offset + 1.0f, 2.0f - offset);
    return (1.0f / 9.0f) * dot(size.zxzx*size.wwyy,
        vec4(texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(-0.5f, -0.5f)), shadowCoord.z)),
             texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(1.0f, -0.5f)), shadowCoord.z)),
             texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(-0.5f, 1.0f)), shadowCoord.z)),
             texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(1.0f, 1.0f)), shadowCoord.z))));
}
#endif

float calcLightFactor(float facing,
                      vec3 lightDirection,
                      vec3 worldPosition,
                      vec3 normal,
                      float diffuse,
                      vec2 spec)
{
    float attenuation = diffuse * facing;

    vec3 vertexToEye = gEyeWorldPosition - worldPosition;
    vec3 lightReflect = lightDirection + 2.0f * facing * normal;
    float specularFactor = dot(vertexToEye, lightReflect);
    if (specularFactor > 0.0f) {
        specularFactor *= inversesqrt(dot(vertexToEye, vertexToEye));
        attenuation += spec.x * pow(specularFactor, spec.y);
    }

    return attenuation;
}

vec4 calcDirectionalLight(directionalLight light,
                          vec3 worldPosition,
                          vec3 normal,
                          vec2 spec)
{
    float attenuation = DL_AMBIENT(light);
    float facing = dot(normal, -DL_DIRECTION(light));
    if (facing > 0.0f) {
      attenuation += calcLightFactor(facing,
                                     DL_DIRECTION(light),
                                     worldPosition,
                                     normal,
                                     DL_DIFFUSE(light),
                                     spec);
    }
    return vec4(DL_COLOR(light), 1.0f) * attenuation;
}

vec4 calcPointLight(pointLight light,
                    vec3 worldPosition,
                    vec3 normal,
                    vec2 spec)
{
    vec4 color = vec4(0.0f);

    vec3 lightDirection = worldPosition - PL_POSITION(light);
    float distanceSq = dot(lightDirection, lightDirection);
    if (distanceSq < PL_RADIUS(light) * PL_RADIUS(light)) {
        float attenuation = 0.0f;
        float facing = -dot(normal, lightDirection);
        if (facing > 0.0f) {
            float invDistance = inversesqrt(distanceSq);
            float attenuation = 1.0f - clamp(distanceSq * invDistance / PL_RADIUS(light), 0.0f, 1.0f);
#ifdef USE_SHADOWMAP
            vec3 absDir = abs(lightDirection);
            vec4 project =
                max(absDir.x, absDir.y) > absDir.z ?
                    (absDir.x > absDir.y ?
                        vec4(lightDirection.zyx, 0.0f / 3.0f) :
                        vec4(lightDirection.xzy, 1.0f / 3.0f)) :
                    vec4(lightDirection, 2.0f / 3.0f);
            vec4 shadowCoord = gLightWVP * vec4(project.xy, abs(project.z), 1.0f);
            attenuation *= calcShadowFactor(shadowCoord.xyz / shadowCoord.w + vec3(project.w, 0.5f * step(0.0f, project.z), 0.0f));
#endif
            attenuation *= calcLightFactor(facing * invDistance,
                                           lightDirection * invDistance,
                                           worldPosition,
                                           normal,
                                           PL_DIFFUSE(light),
                                           spec);
            color = vec4(PL_COLOR(light), 1.0f) * attenuation;
        }
    }

    return color;
}

vec4 calcSpotLight(spotLight light,
                   vec3 worldPosition,
                   vec3 normal,
                   vec2 spec)
{
    vec4 color = vec4(0.0f);

    vec3 lightDirection = worldPosition - SL_POSITION(light);
    float distanceSq = dot(lightDirection, lightDirection);
    if (distanceSq < SL_RADIUS(light) * SL_RADIUS(light)) {
        float facing = -dot(normal, lightDirection);
        if (facing > 0.0f) {
            float invDistance = inversesqrt(distanceSq);
            float spotFactor = dot(lightDirection, SL_DIRECTION(light)) * invDistance;
            if (spotFactor > SL_CUTOFF(light)) {
                float attenuation = 1.0f - clamp(distanceSq * invDistance / SL_RADIUS(light), 0.0f, 1.0f);
                attenuation *= 1.0f - (1.0f - spotFactor) / (1.0f - SL_CUTOFF(light));
#ifdef USE_SHADOWMAP
                vec4 shadowCoord = gLightWVP * vec4(worldPosition, 1.0f);
                attenuation *= calcShadowFactor(shadowCoord.xyz / shadowCoord.w);
#endif
                attenuation *= calcLightFactor(facing * invDistance,
                                               lightDirection * invDistance,
                                               worldPosition,
                                               normal,
                                               SL_DIFFUSE(light),
                                               spec);
                color = vec4(SL_COLOR(light), 1.0f) * attenuation;
            }
        }
    }

    return color;
}

#endif
