#include <string.h>

#include "util.h"

#define returnError() \
    do { \
        m_error = true; \
        return; \
    } while (0)

namespace u {
    // SHA 512
    const uint64_t sha512::K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
        0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
        0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
        0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
        0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
        0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
        0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
        0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
        0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
        0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
        0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
        0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
        0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
    };

    void sha512::init(void) {
        m_length = 0;
        m_currentLength = 0;

        m_state[0] = 0x6a09e667f3bcc908ULL;
        m_state[1] = 0xbb67ae8584caa73bULL;
        m_state[2] = 0x3c6ef372fe94f82bULL;
        m_state[3] = 0xa54ff53a5f1d36f1ULL;
        m_state[4] = 0x510e527fade682d1ULL;
        m_state[5] = 0x9b05688c2b3e6c1fULL;
        m_state[6] = 0x1f83d9abfb41bd6bULL;
        m_state[7] = 0x5be0cd19137e2179ULL;
    }

    sha512::sha512(void) {
        init();
    }

    sha512::sha512(const unsigned char *buf, size_t length) {
        init();
        process(buf, length);
        done();
    }

    void sha512::compress(const unsigned char *buf) {
        uint64_t S[8];
        uint64_t W[80];
        uint64_t t0;
        uint64_t t1;

        // Copy state size_to S
        for (size_t i = 0; i < 8; i++)
            S[i] = m_state[i];
        // Copy the state size_to 1024-bits size_to W[0..15]
        for (size_t i = 0; i < 16; i++)
            W[i] = load64(buf + (8*i));
        // Fill W[16..79]
        for (size_t i = 16; i < 80; i++)
            W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
        // Compress
        auto round = [&](uint64_t a, uint64_t b, uint64_t c, uint64_t& d,
                uint64_t e, uint64_t f, uint64_t g, uint64_t& h, uint64_t i) {
            t0 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];
            t1 = sigma0(a) + maj(a, b, c);
            d += t0;
            h = t0 + t1;
        };

        for (size_t i = 0; i < 80; i += 8) {
            round(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i+0);
            round(S[7], S[0], S[1], S[2], S[3], S[4], S[5], S[6], i+1);
            round(S[6], S[7], S[0], S[1], S[2], S[3], S[4], S[5], i+2);
            round(S[5], S[6], S[7], S[0], S[1], S[2], S[3], S[4], i+3);
            round(S[4], S[5], S[6], S[7], S[0], S[1], S[2], S[3], i+4);
            round(S[3], S[4], S[5], S[6], S[7], S[0], S[1], S[2], i+5);
            round(S[2], S[3], S[4], S[5], S[6], S[7], S[0], S[1], i+6);
            round(S[1], S[2], S[3], S[4], S[5], S[6], S[7], S[0], i+7);
        }

        // Feedback
        for (size_t i = 0; i < 8; i++)
            m_state[i] = m_state[i] + S[i];
    }

    void sha512::process(const unsigned char *in, size_t length) {
        const size_t block_size = sizeof(m_buffer);
        while (length > 0) {
            if (m_currentLength == 0 && length >= block_size) {
                compress(in);
                m_length += block_size * 8;
                in += block_size;
                length -= block_size;
            } else {
                size_t n = u::min(length, (block_size - m_currentLength));
                memcpy(m_buffer + m_currentLength, in, n);
                m_currentLength += n;
                in += n;
                length -= n;
                if (m_currentLength == block_size) {
                    compress(m_buffer);
                    m_length += 8 * block_size;
                    m_currentLength = 0;
                }
            }
        }
    }

    void sha512::done(void) {
        m_length += m_currentLength * 8ULL;
        m_buffer[m_currentLength++] = 0x80;

        if (m_currentLength > 112) {
            while (m_currentLength < 128)
                m_buffer[m_currentLength++] = 0;
            compress(m_buffer);
            m_currentLength = 0;
        }

        // Pad upto 120 bytes of zeroes
        // Note: Bytes 112 to 120 is the 64 MSBs of the length.
        while (m_currentLength < 120)
            m_buffer[m_currentLength++] = 0;

        store64(m_length, m_buffer + 120);
        compress(m_buffer);

        for (size_t i = 0; i < 8; i++)
            store64(m_state[i], m_out + (8*i));
    }

    u::string sha512::hex(void) {
        u::string thing;
        for (size_t i = 0; i != 512 / 8; ++i)
            thing += u::format("%02x", m_out[i]);
        return thing;
    }

    // ZLIB
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
