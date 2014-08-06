#include "math.h"
#include "kdtree.h"

int main(int argc, char **argv) {
    argc--;
    argv++;
    kdTree tree;
    tree.load(*argv);
    u::vector<unsigned char> data = tree.serialize();
    FILE *fp = fopen("out.kd", "w");
    fwrite(&*data.begin(), data.size(), 1, fp);
    fclose(fp);
}
