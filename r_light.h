#ifndef R_LIGHT_HDR
#define R_LIGHT_HDR
#include "math.h"

namespace r {

struct baseLight {
    m::vec3 color;
    float ambient;
    float diffuse;
};

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

}

#endif
