#ifndef MAP_HDR
#define MAP_HDR
#include "math.h"
#include "util.h"
#include "kdtree.h"

struct kdMap {
    kdMap(void);
    ~kdMap(void);

    bool load(const u::vector<unsigned char> &data);
    void unload(void);

    bool isLoaded(void) const;

    u::vector<m::plane>      planes;
    u::vector<kdBinTexture>  textures;
    u::vector<kdBinNode>     nodes;
    u::vector<kdBinTriangle> triangles;
    u::vector<kdBinVertex>   vertices;
    u::vector<kdBinEnt>      entities;
    u::vector<kdBinLeaf>     leafs;
};

#endif
