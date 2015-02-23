#ifndef FOG_HDR
#define FOG_HDR

struct fogParameters {
    vec3 color;
    float density; // Exp and Exp2 (sky fog gradient as well)
    vec2 range; // Linear only
    int equation; // Linear, Exp, Exp2
};

float calcFogFactor(fogParameters fog, float fogCoord) {
    if (fog.equation == 0)
        return 1.0f - clamp((fog.range.y - fogCoord) / (fog.range.y - fog.range.x), 0.0f, 1.0f);
    else if (fog.equation == 1)
        return 1.0f - clamp(exp(-fog.density * fogCoord), 0.0f, 1.0f);
    else if (fog.equation == 2)
        return 1.0f - clamp(exp(-pow(fog.density * fogCoord, 2.0f)), 0.0f, 1.0f);
    return 0.0f;
}

uniform fogParameters gFog;

#endif
