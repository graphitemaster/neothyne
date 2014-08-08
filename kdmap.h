#ifndef MAP_HDR
#define MAP_HDR
#include "math.h"
#include "util.h"
#include "kdtree.h"

struct kdSphereTrace {
    m::vec3 start;
    m::vec3 dir;
    float radius;
    float fraction;
    m::plane plane;
};

struct kdMap {
    kdMap(void);
    ~kdMap(void);

    bool load(const u::vector<unsigned char> &data);
    void unload(void);

    void traceSphere(kdSphereTrace *trace, int32_t node) const;
    void traceSphere(kdSphereTrace *trace) const;

    bool isLoaded(void) const;

    u::vector<m::plane>      planes;
    u::vector<kdBinTexture>  textures;
    u::vector<kdBinNode>     nodes;
    u::vector<kdBinTriangle> triangles;
    u::vector<kdBinVertex>   vertices;
    u::vector<kdBinEnt>      entities;
    u::vector<kdBinLeaf>     leafs;

    static constexpr float kDistEpsilon = 0.02f; // 2cm epsilon for triangle collisions
    static constexpr float kMinFraction = 0.005f; // no less than 0.5% movement along a direction vector

private:
    bool sphereTriangleIntersect(size_t triangleIndex, const m::vec3 &spherePosition,
        float sphereRadius, const m::vec3 &direction, float *fraction, m::vec3 *hitNormal, m::vec3 *hitPoint) const;
};

#endif
