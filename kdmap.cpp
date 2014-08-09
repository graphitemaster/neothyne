#include <string.h>

#include "kdmap.h"

///!kdMap
kdMap::kdMap(void) {
    // nothing
}

kdMap::~kdMap(void) {
    unload();
}

void kdMap::unload(void) {
    planes.clear();
    textures.clear();
    nodes.clear();
    leafs.clear();
    triangles.clear();
    vertices.clear();
    entities.clear();
}

template <typename T>
static size_t mapUnserialize(T *dest, const u::vector<unsigned char> &data, size_t offset = 0, size_t count = 1) {
    const unsigned char *const beg = &*data.begin() + offset;
    memcpy(dest, beg, sizeof(T) * count);
    return offset + sizeof(T);
}

bool kdMap::isLoaded(void) const {
    return nodes.size();
}

bool kdMap::load(const u::vector<unsigned char> &data) {
    size_t seek;
    kdBinHeader header;
    seek = mapUnserialize(&header, data);
    if (header.magic != kdBinHeader::kMagic)
        return false;
    if (header.version != kdBinHeader::kVersion)
        return false;

    // read entries
    kdBinEntry planeEntry;
    kdBinEntry textureEntry;
    kdBinEntry nodeEntry;
    kdBinEntry triangleEntry;
    kdBinEntry vertexEntry;
    kdBinEntry entEntry;
    kdBinEntry leafEntry;

    seek = mapUnserialize(&planeEntry, data, seek);
    seek = mapUnserialize(&textureEntry, data, seek);
    seek = mapUnserialize(&nodeEntry, data, seek);
    seek = mapUnserialize(&triangleEntry, data, seek);
    seek = mapUnserialize(&vertexEntry, data, seek);
    seek = mapUnserialize(&entEntry, data, seek);
    seek = mapUnserialize(&leafEntry, data, seek);

    if (seek != sizeof(kdBinHeader) + 7*sizeof(kdBinEntry))
        return false;

    planes.resize(planeEntry.length / sizeof(kdBinPlane));
    textures.resize(textureEntry.length / sizeof(kdBinTexture));
    nodes.resize(nodeEntry.length / sizeof(kdBinNode));
    triangles.resize(triangleEntry.length / sizeof(kdBinTriangle));
    vertices.resize(vertexEntry.length / sizeof(kdBinVertex));
    entities.resize(entEntry.length / sizeof(kdBinEnt));
    leafs.resize(leafEntry.length);

    // read all planes
    kdBinPlane plane;
    seek = planeEntry.offset;
    for (size_t i = 0; i < planes.size(); i++) {
        seek = mapUnserialize(&plane, data, seek);
        if (plane.type > 2) {
            // The only valid planes are 0, 1, 2 (x, y, z)
            unload();
            return false;
        }
        planes[i].d = plane.d;
        planes[i].n = m::vec3::getAxis((m::axis)plane.type);
    }

    mapUnserialize(&*textures.begin(), data, textureEntry.offset, textures.size());
    mapUnserialize(&*nodes.begin(), data, nodeEntry.offset, nodes.size());
    mapUnserialize(&*triangles.begin(), data, triangleEntry.offset, triangles.size());
    mapUnserialize(&*vertices.begin(), data, vertexEntry.offset, vertices.size());
    mapUnserialize(&*entities.begin(), data, entEntry.offset, entities.size());

    // triangle indices of the leafs
    seek = leafEntry.offset;
    uint32_t triangleCount;
    uint32_t triangleIndex;
    for (size_t i = 0; i < leafEntry.length; i++) {
        seek = mapUnserialize(&triangleCount, data, seek);
        leafs[i].triangles.reserve(triangleCount);
        for (size_t j = 0; j < triangleCount; j++) {
            seek = mapUnserialize(&triangleIndex, data, seek);
            leafs[i].triangles.push_back(triangleIndex);
        }
    }

    // integrity check
    uint32_t endMark;
    mapUnserialize(&endMark, data, seek);
    if (endMark != kdBinHeader::kMagic)
        return false;

    // verify the indices are within a valid range
    for (size_t i = 0; i < nodes.size(); i++) {
        for (size_t k = 0; k < 2; k++) {
            if (nodes[i].children[k] < 0) {
                // leaf index
                if (-nodes[i].children[k]-1 >= (int32_t)leafs.size()) {
                    // invalid leaf pointer
                    unload();
                    return false;
                }
            } else {
                if (nodes[i].children[k] >= (int32_t)nodes.size()) {
                    // invalid node pointer
                    unload();
                    return false;
                }
            }
        }
    }
    return true;
}

bool kdMap::sphereTriangleIntersect(size_t triangleIndex, const m::vec3 &spherePosition,
    float sphereRadius, const m::vec3 &direction, float *fraction, m::vec3 *hitNormal, m::vec3 *hitPoint) const
{
    // sweeping collision check

    const size_t v1 = triangles[triangleIndex].v[0];
    const size_t v2 = triangles[triangleIndex].v[1];
    const size_t v3 = triangles[triangleIndex].v[2];
    const m::vec3 &p0 = vertices[v1].vertex;
    const m::vec3 &p1 = vertices[v2].vertex;
    const m::vec3 &p2 = vertices[v3].vertex;

    m::plane plane(p0, p1, p2); // triangle plane
    plane.d -= sphereRadius;

    *fraction = kdTree::kMaxTraceDistance;

    // triangle face check
    float fractional;
    const bool notParallel = plane.getIntersection(&fractional, spherePosition, direction);
    if (notParallel && fractional >= 0.0f) {
        // calculate hit point
        const m::vec3 checkHitPoint = spherePosition + direction * fractional - plane.n * sphereRadius;

        // check if inside the triangle using barycentric coordinates
        const m::vec3 r = checkHitPoint - p0;
        const m::vec3 q1 = p1 - p0;
        const m::vec3 q2 = p2 - p0;
        const float q1q2 = q1 * q2;
        const float q1Squared = q1.absSquared();
        const float q2Squared = q2.absSquared();
        const float invertDet = 1.0f / (q1Squared * q2Squared - q1q2 * q1q2);
        const float rq1 = r * q1;
        const float rq2 = r * q2;
        const float w1 = invertDet * (q2Squared * rq1 - q1q2 * rq2);
        const float w2 = invertDet * (-q1q2 * rq1 + q1Squared * rq2);

        if (w1 >= 0.0f && w2 >= 0.0f && (w1 + w2 <= 1.0f)) {
            *fraction = fractional;
            *hitNormal = plane.n;
            *hitPoint = checkHitPoint;
            return true;
        }
    }

    // edge detection (for all edges of a triangle)
    for (size_t i = 0; i < 3; i++) {
        const m::vec3 &from = vertices[triangles[triangleIndex].v[i]].vertex;
        const size_t nextIndex = (i + 1) % 3;
        const m::vec3 &to = vertices[triangles[triangleIndex].v[nextIndex]].vertex;

        if (!m::vec3::rayCylinderIntersect(spherePosition, direction, from, to, sphereRadius, &fractional))
            continue;

        if (fractional < *fraction && fractional >= 0.0f) {
            *fraction = fractional;
            *hitPoint = spherePosition + direction * fractional;

            // calculate the normal
            const m::vec3 normal = (from - *hitPoint) ^ (to - *hitPoint);
            *hitNormal = (normal ^ (to - from)).normalized();
        }
    }

    // vertex detection
    for (size_t i = 0; i < 3; i++) {
        const m::vec3 &vertex = vertices[triangles[triangleIndex].v[i]].vertex;
        if (!m::vec3::raySphereIntersect(spherePosition, direction, vertex, sphereRadius, &fractional))
            continue;

        if (fractional < *fraction && fractional >= 0.0f) {
            *fraction = fractional;
            *hitPoint = spherePosition + direction * fractional;
            *hitNormal = *hitPoint - vertex;
            hitNormal->normalize();
        }
    }
    return *fraction != kdTree::kMaxTraceDistance;
}

void kdMap::traceSphere(kdSphereTrace *trace) const {
    trace->fraction = kdTree::kMaxTraceDistance;
    traceSphere(trace, 0);
}

void kdMap::traceSphere(kdSphereTrace *trace, int32_t node) const {
    if (node < 0) {
        // leaf node
        const size_t leafIndex = -node - 1;
        const size_t triangleCount = leafs[leafIndex].triangles.size();

        float fraction;
        float minFraction = trace->fraction;
        m::plane hitPlane;
        m::vec3 hitNormal;
        m::vec3 hitPoint;
        m::vec3 normal;

        // check every triangle in the leaf
        for (size_t i = 0; i < triangleCount; i++) {
            const size_t triangleIndex = leafs[leafIndex].triangles[i];
            // did we collide against a triangle in this leaf?
            if (sphereTriangleIntersect(triangleIndex, trace->start, trace->radius, trace->dir, &fraction, &hitNormal, &hitPoint)) {
                // safely shift along the traced path, keeping the sphere kDistEpsilon
                // away from the plane along the planes normal.
                fraction += kDistEpsilon / (hitNormal * trace->dir);
                if (fraction <= kMinFraction)
                    fraction = 0.0f; // prevent small noise
                if (fraction < minFraction) {
                    hitPlane.setupPlane(hitPoint, hitNormal);
                    trace->plane = hitPlane;
                    trace->fraction = fraction;
                    minFraction = fraction;
                }
            }
        }
        return;
    }
    // not a leaf node
    m::pointPlane start;
    m::pointPlane end;

    // check if everything is infront of the splitting plane
    m::plane checkPlane = planes[nodes[node].plane];
    checkPlane.d -= trace->radius;
    start = checkPlane.classify(trace->start, kdTree::kEpsilon);
    end = checkPlane.classify(trace->start + trace->dir, kdTree::kEpsilon);
    if (start >= m::kPointPlaneOnPlane && end > m::kPointPlaneOnPlane) {
        traceSphere(trace, nodes[node].children[0]);
        return;
    }

    // check if everything is behind of the splitting plane
    checkPlane.d = planes[nodes[node].plane].d + trace->radius;
    start = checkPlane.classify(trace->start, kdTree::kEpsilon);
    end = checkPlane.classify(trace->start + trace->dir, kdTree::kEpsilon);
    if (start < m::kPointPlaneOnPlane && end < m::kPointPlaneOnPlane) {
        traceSphere(trace, nodes[node].children[1]);
        return;
    }

    kdSphereTrace traceFront = *trace;
    kdSphereTrace traceBack = *trace;

    traceSphere(&traceFront, nodes[node].children[0]);
    traceSphere(&traceBack, nodes[node].children[1]);

    *trace = (traceFront.fraction < traceBack.fraction) ? traceFront : traceBack;
}

void kdMap::clipVelocity(const m::vec3 &in, const m::vec3 &normal, m::vec3 &out, float overBounce) {
    // determine how far along the plane we have to slde based on the incoming direction
    // this is scaled by `overBounce'
    float backOff = in * normal * overBounce;

    // against all axis
    for (size_t i = 0; i < 3; i++) {
        float change = normal[i] * backOff;
        out[i] = in[i] - change;

        // if the velocity gets too small cancel it out to prevent noise in the
        // response
        if (fabsf(out[i]) < kStopEpsilon)
            out[i] = 0.0f;
    }
}
