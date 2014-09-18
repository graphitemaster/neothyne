#include "u_zlib.h"

#define returnError() \
    do { \
        m_error = true; \
        return; \
    } while (0)

namespace u {

static constexpr size_t kLengthBases[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static constexpr size_t kLengthExtras[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
    3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static constexpr size_t kDistanceBases[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};

static constexpr size_t kDistanceExtras[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
    8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static constexpr size_t kCodeLengthCodeLengths[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// zlib decompression
size_t zlib::readBitFromStream(size_t &bitp, const unsigned char *bits) {
    size_t r = (bits[bitp >> 3] >> (bitp & 0x7)) & 1;
    bitp++;
    return r;
}

size_t zlib::readBitsFromStream(size_t &bitp, const unsigned char *bits, size_t nbits) {
    size_t r = 0;
    for(size_t i = 0; i < nbits; i++)
        r += (readBitFromStream(bitp, bits)) << i;
    return r;
}

bool zlib::huffmanTree::make(const u::vector<size_t> &bitlen, size_t maxbitlen) {
    //make tree given the lengths
    size_t numcodes = (size_t)(bitlen.size());
    size_t treepos = 0;
    size_t nodefilled = 0;

    u::vector<size_t> tree1D(numcodes);
    u::vector<size_t> blcount(maxbitlen + 1, 0);
    u::vector<size_t> nextcode(maxbitlen + 1, 0);

    // count number of instances of each code length
    for(size_t bits = 0; bits < numcodes; bits++)
        blcount[bitlen[bits]]++;
    for(size_t bits = 1; bits <= maxbitlen; bits++)
        nextcode[bits] = (nextcode[bits - 1] + blcount[bits - 1]) << 1;
    for (size_t n = 0; n < numcodes; n++)
        if (bitlen[n] != 0)
            tree1D[n] = nextcode[bitlen[n]]++;

    m_tree2D.clear();
    m_tree2D.resize(numcodes * 2, 32767); // 32767 here means the tree2d isn't filled there yet

    for (size_t n = 0; n < numcodes; n++) {
        for (size_t i = 0; i < bitlen[n]; i++) {
            size_t bit = (tree1D[n] >> (bitlen[n] - i - 1)) & 1;
            if (treepos > numcodes - 2)
                return false;
            if (m_tree2D[2 * treepos + bit] == 32767) {
                if (i + 1 == bitlen[n]) {
                    m_tree2D[2 * treepos + bit] = n;
                    treepos = 0;
                } else {
                    m_tree2D[2 * treepos + bit] = ++nodefilled + numcodes;
                    treepos = nodefilled;
                }
            } else {
                treepos = m_tree2D[2 * treepos + bit] - numcodes;
            }
        }
    }
    return true;
}

// Decodes a symbol from the tree
bool zlib::huffmanTree::decode(bool &decoded, size_t &r, size_t &treepos, size_t bit) const {
    size_t numcodes = m_tree2D.size() / 2;
    if (treepos >= numcodes)
        return false;

    r = m_tree2D[2 * treepos + bit];
    decoded = (r < numcodes);
    treepos = decoded ? 0 : r - numcodes;
    return true;
}

void zlib::inflator::inflate(u::vector<unsigned char> &out, const u::vector<unsigned char> &in, size_t inpos) {
    size_t bp = 0;
    size_t pos = 0;
    size_t bfinal = 0;
    while (!bfinal && !m_error) {
        if (bp >> 3 >= in.size())
            returnError();

        bfinal = readBitFromStream(bp, &in[inpos]);
        size_t btype = readBitFromStream(bp, &in[inpos]);
        btype += 2 * readBitFromStream(bp, &in[inpos]);

        if (btype == 3)
            returnError();
        else if (btype == 0)
            inflateNoCompression(out, &in[inpos], bp, pos, in.size());
        else
            inflateHuffmanBlock(out, &in[inpos], bp, pos, in.size(), btype);
    }
    if (!m_error)
        out.resize(pos);
}

// get the tree of a deflated block with fixed tree
void zlib::inflator::generateFixedTrees(huffmanTree& tree, huffmanTree& treeD) {
    u::vector<size_t> bitlen(288, 8);
    u::vector<size_t> bitlenD(32, 5);

    for(size_t i = 144; i <= 255; i++)
        bitlen[i] = 9;
    for(size_t i = 256; i <= 279; i++)
        bitlen[i] = 7;

    tree.make(bitlen, 15);
    treeD.make(bitlenD, 15);
}

size_t zlib::inflator::huffmanDecodeSymbol(const unsigned char *in, size_t &bp, const huffmanTree &m_codeTree, size_t inlength) {
    bool decoded;
    size_t ct;
    for (size_t treepos = 0;;) {
        // error: end reached without endcode
        if ((bp & 0x07) == 0 && (bp >> 3) > inlength) {
            m_error = true;
            return 0;
        }
        if (!m_codeTree.decode(decoded, ct, treepos, readBitFromStream(bp, in))) {
            m_error = true;
            return 0;
        }
        if (decoded)
            return ct;
    }
    return 0;
}

// get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree
void zlib::inflator::getTreeInflateDynamic(huffmanTree &tree, huffmanTree &treeD,
    const unsigned char *in, size_t &bp, size_t inlength)
{
    u::vector<size_t> bitlen(288, 0);
    u::vector<size_t> bitlenD(32, 0);

    if (bp >> 3 >= inlength - 2)
        returnError();

    const size_t literals = readBitsFromStream(bp, in, 5) + 257; // number of literal/length codes + 257
    const size_t distance = readBitsFromStream(bp, in, 5) + 1; // number of dist codes + 1
    const size_t codeLengths = readBitsFromStream(bp, in, 4) + 4; // number of code length codes + 4

    u::vector<size_t> codelengthcode(19);
    for (size_t i = 0; i < 19; i++)
        codelengthcode[kCodeLengthCodeLengths[i]] = (i < codeLengths)
            ? readBitsFromStream(bp, in, 3) : 0;

    if (!m_codeLengthCodeTree.make(codelengthcode, 7))
        returnError();

    size_t i = 0;
    size_t replength;

    while (i < literals + distance) {
        size_t code = huffmanDecodeSymbol(in, bp, m_codeLengthCodeTree, inlength);
        if (m_error)
            return;

        // length code
        if (code <= 15) {
            if (i < literals)
                bitlen[i++] = code;
            else
                bitlenD[i++ - literals] = code;
        }

        // repeat previous
        else if (code == 16) {
            if (bp >> 3 >= inlength)
                returnError();
            replength = 3 + readBitsFromStream(bp, in, 2);
            size_t value; // set value to the previous code
            if((i - 1) < literals)
                value = bitlen[i - 1];
            else
                value = bitlenD[i - literals - 1];

            for (size_t n = 0; n < replength; n++) {
                if (i >= literals + distance)
                    returnError();
                if (i < literals)
                    bitlen[i++] = value;
                else
                    bitlenD[i++ - literals] = value;
            }
        }

        // repeat "0" 3-10 times
        else if(code == 17) {
            if (bp >> 3 >= inlength)
                returnError();
            replength = 3 + readBitsFromStream(bp, in, 3);
            for (size_t n = 0; n < replength; n++) {
                if (i >= literals + distance)
                    returnError();
                if (i < literals)
                    bitlen[i++] = 0;
                else
                    bitlenD[i++ - literals] = 0;
            }
        }

        // repeat "0" 11-138 times
        else if (code == 18) {
            if (bp >> 3 >= inlength) {
                m_error = true;
                return;
            }
            replength = 11 + readBitsFromStream(bp, in, 7);
            for (size_t n = 0; n < replength; n++) {
                if (i >= literals + distance) {
                    m_error = true;
                    return;
                }
                if (i < literals)
                    bitlen[i++] = 0;
                else
                    bitlenD[i++ - literals] = 0;
            }
        } else {
            m_error = true;
            return;
        }
    }
    // the length of the end code 256 must be larger than 0
    if (bitlen[256] == 0)
        returnError();
    if (!tree.make(bitlen, 15))
        returnError();
    if (!treeD.make(bitlenD, 15))
        returnError();
}

void zlib::inflator::inflateHuffmanBlock(u::vector<unsigned char> &out, const unsigned char *in,
    size_t &bp, size_t &pos, size_t inlength, size_t btype)
{
    if(btype == 1)
        generateFixedTrees(m_codeTree, m_codeTreeDistance);
    else if(btype == 2) {
        getTreeInflateDynamic(m_codeTree, m_codeTreeDistance, in, bp, inlength);
        if (m_error)
            return;
    }
    for (;;) {
        size_t code = huffmanDecodeSymbol(in, bp, m_codeTree, inlength);
        if (m_error)
            return;

        // end code
        if (code == 256)
            return;

        // literal symbol
        else if(code <= 255) {
            if (pos >= out.size())
                out.resize((pos + 1) * 2);
            out[pos++] = (unsigned char)(code);
        }

        // length code
        else if(code >= 257 && code <= 285) {
            size_t length = kLengthBases[code - 257];
            size_t numextrabits = kLengthExtras[code - 257];

            if ((bp >> 3) >= inlength)
                returnError();

            length += readBitsFromStream(bp, in, numextrabits);
            size_t codeD = huffmanDecodeSymbol(in, bp, m_codeTreeDistance, inlength);
            if (m_error)
                return;

            if (codeD > 29)
                returnError();

            size_t dist = kDistanceBases[codeD];
            size_t numextrabitsD = kDistanceExtras[codeD];

            if ((bp >> 3) >= inlength)
                returnError();

            dist += readBitsFromStream(bp, in, numextrabitsD);
            size_t start = pos;
            size_t back = start - dist; // backwards

            if (pos + length >= out.size())
                out.resize((pos + length) * 2);
            for (size_t i = 0; i < length; i++) {
                out[pos++] = out[back++];
                if(back >= start)
                    back = start - dist;
            }
        }
    }
}

void zlib::inflator::inflateNoCompression(u::vector<unsigned char> &out,
    const unsigned char *in, size_t &bp, size_t &pos, size_t inlength)
{
    // go to first boundary of byte
    while ((bp & 0x7) != 0)
        bp++;

    size_t p = bp / 8;
    // error: bit pointer will jump past memory
    if (p >= inlength - 4)
        returnError();

    size_t length = in[p] + 256 * in[p + 1];
    size_t numberOfLengths = in[p + 2] + 256 * in[p + 3];

    p += 4;
    if (length + numberOfLengths != 65535)
        returnError();

    if (pos + length >= out.size())
        out.resize(pos + length);
    if (p + length > inlength)
        returnError();
    for (size_t n = 0; n < length; n++)
        out[pos++] = in[p++];
    bp = p * 8;
}

bool zlib::decompress(u::vector<unsigned char> &out, const u::vector<unsigned char> &in) {
    inflator inflator;
    if (in.size() < 2)
        return false;
    // 256 * in[0] + in[1] must be a multiple of 31, the FCHECK value is supposed to be made that way
    if ((in[0] * 256 + in[1]) % 31 != 0)
        return false;
    size_t cm = in[0] & 15;
    size_t cinfo = (in[0] >> 4) & 15;
    size_t fdict = (in[1] >> 5) & 1;
    // only compression method 8 is supported: `inflate with sliding window of 32k is supported'
    if (cm != 8 || cinfo > 7)
        return false;
    // "The additional flags shall not specify a preset dictionary."
    if (fdict != 0)
        return false;
    inflator.inflate(out, in, 2);
    return inflator.m_error == false;
}

}
