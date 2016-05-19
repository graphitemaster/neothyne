#ifndef FOG_HDR
#define FOG_HDR

#define FOG_COLOR   gFog[0]
#define FOG_RANGE   gFog[1].xy
#define FOG_DENSITY gFog[1].z

// { { r, g, b }, { range.x, range.y, density } }
uniform vec3[2] gFog;
// { 0 = Linear, 1 = Exp, 2 = Exp2 }
uniform int gFogEquation;

float calcFogFactor(float fogCoord) {
    vec2 range = FOG_RANGE;
    float density = FOG_DENSITY;
    if (gFogEquation == 0)
        return 1.0f - clamp((range.y - fogCoord) / (range.y - range.x), 0.0f, 1.0f);
    else if (gFogEquation == 1)
        return 1.0f - clamp(exp(-density * fogCoord), 0.0f, 1.0f);
    else if (gFogEquation == 2)
        return 1.0f - clamp(exp(-pow(density * fogCoord, 2.0f)), 0.0f, 1.0f);
    return 0.0f;
}
#endif
