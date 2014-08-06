#include "math.h"
#include "kdtree.h"
#include "kdmap.h"

int main(int argc, char **argv) {
    argc--;
    argv++;
    kdTree tree;
    fprintf(stderr, "Reading geometry ...\n");
    tree.load(*argv);
    fprintf(stderr, "Built tree from %zu vertices in %zu triangles\n",
        tree.vertices.size(),
        tree.triangles.size());

    fprintf(stderr, "Serializing tree ...\n");
    u::vector<unsigned char> data = tree.serialize();
    fprintf(stderr, "Complete\n");

    fprintf(stderr, "Loading tree...\n");
    kdMap m;
    if (m.load(data)) {
        fprintf(stderr, "Completed with %zu nodes and %zu leafs\n",
            m.nodes.size(), m.leafs.size());
    }
}
