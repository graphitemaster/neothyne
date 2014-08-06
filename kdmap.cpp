#include <string.h>

#include "kdmap.h"

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
static size_t mapUnserialize(T *dest, const u::vector<unsigned char> &data, size_t offset = 0) {
    const unsigned char *const beg = &*data.begin() + offset;
    memcpy(dest, beg, sizeof(T));
    return offset + sizeof(T);
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

    mapUnserialize(&*textures.begin(), data, textureEntry.offset);
    mapUnserialize(&*nodes.begin(), data, nodeEntry.offset);
    mapUnserialize(&*triangles.begin(), data, triangleEntry.offset);
    mapUnserialize(&*vertices.begin(), data, vertexEntry.offset);
    mapUnserialize(&*entities.begin(), data, entEntry.offset);

    // triangle indices of the leafs
    seek = leafEntry.offset;
    uint32_t triangleCount;
    uint32_t triangleIndex;
    for (size_t i = 0; i < leafEntry.length; i++) {
        seek = mapUnserialize(&triangleCount, data, seek);
        leafs[i].triangles.resize(triangleCount);
        for (size_t j = 0; j < triangleCount; j++) {
            seek = mapUnserialize(&triangleIndex, data, seek);
            leafs[i].triangles[j] = triangleIndex;
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
