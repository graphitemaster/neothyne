#ifndef R_MODEL_HDR
#define R_MODEL_HDR
#include "model.h"

#include "r_geom.h"

#include "u_string.h"
#include "u_map.h"

namespace r {

struct texture2D;

struct material {
    material();

    int permute; // Geometry pass permutation for this material
    texture2D *diffuse;
    texture2D *normal;
    texture2D *spec;
    texture2D *displacement;
    bool specParams;
    float specPower;
    float specIntensity;
    float dispScale;
    float dispBias;

    bool load(u::map<u::string, texture2D*> &textures, const u::string &file, const u::string &basePath);
    bool upload();
};

struct model : geom {
    model();

    bool load(u::map<u::string, texture2D*> &textures, const u::string &file);
    bool upload();
    void render();

    // The material for this model (TODO: many materials)
    material mat;
    m::vec3 scale;
    m::vec3 rotate;

    m::bbox bounds() const;

private:
    size_t m_indices;
    ::model m_model;
};

inline m::bbox model::bounds() const {
    return m_model.bounds();
}

}
#endif
