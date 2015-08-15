#ifndef U_ZLIB_HDR
#define U_ZLIB_HDR
#include <stddef.h>
#include "u_vector.h"

namespace u {

struct zlib {
    static bool decompress(u::vector<unsigned char> &out, const u::vector<unsigned char> &in);
    static bool decompress(u::vector<unsigned char> &out, const unsigned char *in, size_t length);
    static bool compress(u::vector<unsigned char> &out, const u::vector<unsigned char> &in, int quality = 5);
    static bool compress(u::vector<unsigned char> &out, const unsigned char *in, size_t length, int quality = 5);

private:
    static size_t readBitFromStream(size_t &bitp, const unsigned char *bits);
    static size_t readBitsFromStream(size_t &bitp, const unsigned char *bits, size_t nbits);

    struct huffmanTree {
        bool make(const u::vector<size_t> &bitlen, size_t maxbitlen);
        // Decodes a symbol from the tree
        bool decode(bool& decoded, size_t &r, size_t &treepos, size_t bit) const;

    private:
        // 2D representation of a huffman tree: The one dimension is "0" or "1",
        // the other contains all nodes and leaves of the tree.
        u::vector<size_t> m_tree2D;
    };

    struct deflator {
        deflator()
            : m_data(nullptr)
            , m_bitBuffer(0)
            , m_bitCount(0)
        {
        }

        void deflate(u::vector<unsigned char> &out, const unsigned char *in, size_t length, int quality = 5);

    protected:
        int bitReverse(int code, int codeBits);
        static int countMatches(const unsigned char *a, const unsigned char *b, int limit);
        unsigned int hash(const unsigned char *data);

        void flush();
        void add(int code, int codeBits);

        void huffA(int b, int c);
        void huffB(int n);

        template <size_t E>
        void huffSwitch(int n);

        void huff(int n);

    private:
        u::vector<unsigned char> *m_data;
        unsigned int m_bitBuffer;
        int m_bitCount;
    };

    struct inflator {
        inflator() :
            m_error(false)
        {
        }

        bool inflate(u::vector<unsigned char> &out, const unsigned char *in, size_t length, size_t inpos = 0);

        // get the tree of a deflated block with fixed tree
        void generateFixedTrees(huffmanTree& tree, huffmanTree& treeD);

        // decode a single symbol from given list of bits with given code tree. return value is the symbol
        size_t huffmanDecodeSymbol(const unsigned char *in, size_t &bp, const huffmanTree &codetree, size_t inlength);

        // get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree
        void getTreeInflateDynamic(huffmanTree &tree, huffmanTree &treeD, const unsigned char *in, size_t &bp, size_t inlength);

        void inflateHuffmanBlock(u::vector<unsigned char> &out, const unsigned char *in, size_t &bp, size_t &pos, size_t inlength, size_t btype);
        void inflateNoCompression(u::vector<unsigned char> &out, const unsigned char *in, size_t &bp, size_t &pos, size_t inlength);

    private:
        friend struct zlib;

        bool m_error;

        // the code tree for Huffman codes, dist codes, and code length codes
        huffmanTree m_codeTree;
        huffmanTree m_codeTreeDistance;
        huffmanTree m_codeLengthCodeTree;
    };
};

}
#endif
