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

    void traceSphere(kdSphereTrace *trace) const;
    bool isSphereStuck(const m::vec3 &position, float radius) const;

    bool isLoaded(void) const;

    u::vector<m::plane>      planes;
    u::vector<kdBinTexture>  textures;
    u::vector<kdBinNode>     nodes;
    u::vector<kdBinTriangle> triangles;
    u::vector<kdBinVertex>   vertices;
    u::vector<kdBinEnt>      entities;
    u::vector<kdBinLeaf>     leafs;

    static constexpr float kDistEpsilon = 0.05f; // 5cm epsilon for triangle collisions
    static constexpr float kMinFraction = 0.005f; // no less than 0.5% movement along a direction vector
    static constexpr size_t kMaxClippingPlanes = 5; // maximum number of clipping planes to test
    static constexpr size_t kMaxBumps = 4; // Maximum collision bump iterations
    static constexpr float kFractionScale = 0.95f; // Collision response fractional scale
    static constexpr float kOverClip = 1.01f; // percentage * 100 of overclip allowed in collision detection against planes (lower values == more sticky)
    static constexpr float kStopEpsilon = 0.2f; // minimum velocity size for clipping

    // clips the velocity for collision handling
    static void clipVelocity(const m::vec3 &in, const m::vec3 &normal, m::vec3 &out, float overBounce);

private:
    // sweeping
    bool sphereTriangleIntersect(size_t triangleIndex, const m::vec3 &spherePosition,
        float sphereRadius, const m::vec3 &direction, float *fraction, m::vec3 *hitNormal, m::vec3 *hitPoint) const;

    // static
    bool sphereTriangleIntersectStatic(size_t triangleIndex, const m::vec3 &spherePosition, float sphereRadius) const;

    void traceSphere(kdSphereTrace *trace, int32_t node) const;
    bool isSphereStuck(const m::vec3 &position, float radius, int32_t node) const;
};

#endif
