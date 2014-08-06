#include "math.h"
#include "kdtree.h"

int main(int argc, char **argv) {
    argc--;
    argv++;
    kdTree tree;
    tree.load(*argv);
    tree.serialize();
}
