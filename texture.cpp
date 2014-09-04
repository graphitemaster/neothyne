#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include "texture.h"
#include "math.h"

#define returnResult(E) \
    do { \
        m_error = (E); \
        return; \
    } while (0)

struct decoder {
    decoder() :
        m_error(kSuccess),
        m_width(0),
        m_height(0),
        m_bpp(0)
    {

    }

    enum result {
        kSuccess,
        kInvalid,
        kUnsupported,
        kInternalError,
        kMalformatted,
        kFinished,
    };

    const char *error(void) const {
        switch (m_error) {
            case kSuccess:
                return "success";
            case kInvalid:
                return "invalid";
            case kUnsupported:
                return "unsupported";
            case kMalformatted:
                return "malformatted";
            default:
                break;
        }
        return "internal error";
    }

    size_t width(void) const {
        return m_width;
    }

    size_t height(void) const {
        return m_height;
    }

    size_t bpp(void) const {
        return m_bpp;
    }

    result status(void) const {
        return m_error;
    }

protected:
    result m_error;
    size_t m_width;
    size_t m_height;
    size_t m_bpp;
};

///
/// Baseline JPEG decoder
///  * Doesn't support progressive or lossless JPEG
///  * Doesn't support CMYK, RGB, or any other color-space jpeg, only supports
///    8-bit greyscale or YCbCr.
///
///  * Supports chroma subsampling ratio (any POT)
///  * Supports restart markers
///
///  Decoder itself decodes to either 8-bit greyscale compatible with GL_LUMINANCE8
///  or 24-bit RGB compatible with GL_RGB8.
///
struct jpeg : decoder {
    enum chromaFilter {
        kBicubic,
        kPixelRepetition
    };

    jpeg(const u::vector<unsigned char> &data, chromaFilter filter = kBicubic) :
        m_rstinterval(0),
        m_size(0),
        m_length(0),
        m_mbwidth(0),
        m_mbheight(0),
        m_mbsizex(0),
        m_mbsizey(0),
        m_buf(0),
        m_bufbits(0),
        m_exifLittleEndian(false),
        m_coSitedChroma(false)
    {
        memset(m_comp, 0, sizeof(m_comp));
        memset(m_vlctab, 0, sizeof(m_vlctab));
        memset(m_qtab, 0, sizeof(m_qtab));
        memset(m_block, 0, sizeof(m_block));

        decode(data, filter);
    }

    u::vector<unsigned char> data(void) {
        if (m_bpp == 1)
            return u::move(m_comp[0].pixels);
        return u::move(m_rgb);
    }

private:
    struct vlcCode {
        unsigned char bits;
        unsigned char code;
    };

    struct component {
        int cid;
        int ssx;
        int ssy;

        size_t width;
        size_t height;
        size_t stride;
        size_t qtsel;
        size_t actabsel;
        size_t dctabsel;
        size_t dcpred;

        u::vector<unsigned char> pixels;
    };

    unsigned char clip(const int x) {
        return (x < 0) ? 0 : ((x > 0xFF) ? 0xFF : (unsigned char)x);
    }

    static constexpr int kW1 = 2841;
    static constexpr int kW2 = 2676;
    static constexpr int kW3 = 2408;
    static constexpr int kW5 = 1609;
    static constexpr int kW6 = 1108;
    static constexpr int kW7 = 565;

    // fast integer discrete cosine transform
    void rowIDCT(int* blk) {
        int x0, x1, x2, x3, x4, x5, x6, x7, x8;
        if (!((x1 = blk[4] << 11)
            | (x2 = blk[6]) | (x3 = blk[2])
            | (x4 = blk[1]) | (x5 = blk[7])
            | (x6 = blk[5]) | (x7 = blk[3])))
        {
            int value = blk[0] << 3;
            for (size_t i = 0; i < 8; i++)
                blk[i] = value;
            return;
        }
        x0 = (blk[0] << 11) + 128;
        x8 = kW7 * (x4 + x5);
        x4 = x8 + (kW1 - kW7) * x4;
        x5 = x8 - (kW1 + kW7) * x5;
        x8 = kW3 * (x6 + x7);
        x6 = x8 - (kW3 - kW5) * x6;
        x7 = x8 - (kW3 + kW5) * x7;
        x8 = x0 + x1;
        x0 -= x1;
        x1 = kW6 * (x3 + x2);
        x2 = x1 - (kW2 + kW6) * x2;
        x3 = x1 + (kW2 - kW6) * x3;
        x1 = x4 + x6;
        x4 -= x6;
        x6 = x5 + x7;
        x5 -= x7;
        x7 = x8 + x3;
        x8 -= x3;
        x3 = x0 + x2;
        x0 -= x2;
        x2 = (181 * (x4 + x5) + 128) >> 8;
        x4 = (181 * (x4 - x5) + 128) >> 8;
        blk[0] = (x7 + x1) >> 8;
        blk[1] = (x3 + x2) >> 8;
        blk[2] = (x0 + x4) >> 8;
        blk[3] = (x8 + x6) >> 8;
        blk[4] = (x8 - x6) >> 8;
        blk[5] = (x0 - x4) >> 8;
        blk[6] = (x3 - x2) >> 8;
        blk[7] = (x7 - x1) >> 8;
    }

    void columnIDCT(const int* blk, unsigned char *out, int stride) {
        int x0, x1, x2, x3, x4, x5, x6, x7, x8;
        if (!((x1 = blk[8*4] << 8)
            | (x2 = blk[8*6]) | (x3 = blk[8*2])
            | (x4 = blk[8*1]) | (x5 = blk[8*7])
            | (x6 = blk[8*5]) | (x7 = blk[8*3])))
        {
            x1 = clip(((blk[0] + 32) >> 6) + 128);
            for (x0 = 8; x0; --x0) {
                *out = (unsigned char)x1;
                out += stride;
            }
            return;
        }
        x0 = (blk[0] << 8) + 8192;
        x8 = kW7 * (x4 + x5) + 4;
        x4 = (x8 + (kW1 - kW7) * x4) >> 3;
        x5 = (x8 - (kW1 + kW7) * x5) >> 3;
        x8 = kW3 * (x6 + x7) + 4;
        x6 = (x8 - (kW3 - kW5) * x6) >> 3;
        x7 = (x8 - (kW3 + kW5) * x7) >> 3;
        x8 = x0 + x1;
        x0 -= x1;
        x1 = kW6 * (x3 + x2) + 4;
        x2 = (x1 - (kW2 + kW6) * x2) >> 3;
        x3 = (x1 + (kW2 - kW6) * x3) >> 3;
        x1 = x4 + x6;
        x4 -= x6;
        x6 = x5 + x7;
        x5 -= x7;
        x7 = x8 + x3;
        x8 -= x3;
        x3 = x0 + x2;
        x0 -= x2;
        x2 = (181 * (x4 + x5) + 128) >> 8;
        x4 = (181 * (x4 - x5) + 128) >> 8;
        *out = clip(((x7 + x1) >> 14) + 128); out += stride;
        *out = clip(((x3 + x2) >> 14) + 128); out += stride;
        *out = clip(((x0 + x4) >> 14) + 128); out += stride;
        *out = clip(((x8 + x6) >> 14) + 128); out += stride;
        *out = clip(((x8 - x6) >> 14) + 128); out += stride;
        *out = clip(((x0 - x4) >> 14) + 128); out += stride;
        *out = clip(((x3 - x2) >> 14) + 128); out += stride;
        *out = clip(((x7 - x1) >> 14) + 128);
    }

    int viewBits(int bits) {
        unsigned char newbyte;
        if (!bits)
            return 0;
        while (m_bufbits < bits) {
            if (m_size <= 0) {
                m_buf = (m_buf << 8) | 0xFF;
                m_bufbits += 8;
                continue;
            }
            newbyte = *m_position++;
            m_size--;
            m_bufbits += 8;
            m_buf = (m_buf << 8) | newbyte;
            if (newbyte == 0xFF) {
                if (m_size) {
                    unsigned char marker = *m_position++;
                    m_size--;
                    switch (marker) {
                        case 0:
                            break;
                        case 0xD9:
                            m_size = 0;
                            break;
                        default:
                            if ((marker & 0xF8) != 0xD0)
                                m_error = kMalformatted;
                            else {
                                m_buf = (m_buf << 8) | marker;
                                m_bufbits += 8;
                            }
                    }
                } else
                    m_error = kMalformatted;
            }
        }
        return (m_buf >> (m_bufbits - bits)) & ((1 << bits) - 1);
    }

    void skipBits(int bits) {
        if (m_bufbits < bits)
            (void)viewBits(bits);
        m_bufbits -= bits;
    }

    int getBits(int bits) {
        int res = viewBits(bits);
        skipBits(bits);
        return res;
    }

    void alignBits(void) {
        m_bufbits &= 0xF8;
    }

    void skip(int count) {
        m_position += count;
        m_size -= count;
        m_length -= count;
        if (m_size < 0)
            m_error = kMalformatted;
    }

    unsigned short decode16(const unsigned char *m_position) {
        return (m_position[0] << 8) | m_position[1];
    }

    void decodeLength(void) {
        if (m_size < 2)
            returnResult(kMalformatted);
        m_length = decode16(m_position);
        if (m_length > m_size)
            returnResult(kMalformatted);
        skip(2);
    }

    void skipMarker(void) {
        decodeLength();
        skip(m_length);
    }

    void decodeSOF(void) {
        int ssxmax = 0;
        int ssymax = 0;
        component* c;

        decodeLength();

        if (m_length < 9)
            returnResult(kMalformatted);
        if (m_position[0] != 8)
            returnResult(kUnsupported);

        m_height = decode16(m_position + 1);
        m_width = decode16(m_position + 3);
        m_bpp = m_position[5];
        skip(6);

        switch (m_bpp) {
            case 1:
            case 3:
                break;
            default:
                returnResult(kUnsupported);
        }

        if (m_length < int(m_bpp * 3))
            returnResult(kMalformatted);

        size_t i;
        for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
            c->cid = m_position[0];
            if (!(c->ssx = m_position[1] >> 4))
                returnResult(kMalformatted);
            if (c->ssx & (c->ssx - 1))
                returnResult(kUnsupported);  // non-power of two
            if (!(c->ssy = m_position[1] & 15))
                returnResult(kMalformatted);
            if (c->ssy & (c->ssy - 1))
                returnResult(kUnsupported);  // non-power of two
            if ((c->qtsel = m_position[2]) & 0xFC)
                returnResult(kMalformatted);
            skip(3);
            if (c->ssx > ssxmax)
                ssxmax = c->ssx;
            if (c->ssy > ssymax)
                ssymax = c->ssy;
        }
        m_mbsizex = ssxmax << 3;
        m_mbsizey = ssymax << 3;
        m_mbwidth = (m_width + m_mbsizex - 1) / m_mbsizex;
        m_mbheight = (m_height + m_mbsizey - 1) / m_mbsizey;
        for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
            c->width = (m_width * c->ssx + ssxmax - 1) / ssxmax;
            c->stride = (c->width + 7) & 0x7FFFFFF8;
            c->height = (m_height * c->ssy + ssymax - 1) / ssymax;
            c->stride = m_mbwidth * m_mbsizex * c->ssx / ssxmax;
            if (((c->width < 3) && (c->ssx != ssxmax)) || ((c->height < 3) && (c->ssy != ssymax)))
                returnResult(kUnsupported);
            c->pixels.resize(c->stride * (m_mbheight * m_mbsizey * c->ssy / ssymax));
        }
        if (m_bpp == 3)
            m_rgb.resize(m_width * m_height * m_bpp);
        skip(m_length);
    }

    void decodeDHT(void) {
        unsigned char counts[16];
        decodeLength();
        while (m_length >= 17) {
            int i = m_position[0];
            if (i & 0xEC)
                returnResult(kMalformatted);
            if (i & 0x02)
                returnResult(kUnsupported);
            i = (i | (i >> 3)) & 3;  // combined DC/AC + table identification value
            for (size_t codelen = 1; codelen <= 16; ++codelen)
                counts[codelen - 1] = m_position[codelen];
            skip(17);
            vlcCode *vlc = &m_vlctab[i][0];
            int remain = 65536;
            int spread = 65536;
            for (size_t codelen = 1; codelen <= 16; ++codelen) {
                spread >>= 1;
                int currcnt = counts[codelen - 1];
                if (!currcnt)
                    continue;
                if (m_length < currcnt)
                    returnResult(kMalformatted);
                remain -= currcnt << (16 - codelen);
                if (remain < 0)
                    returnResult(kMalformatted);
                for (i = 0; i < currcnt; ++i) {
                    register unsigned char code = m_position[i];
                    for (int j = spread; j; --j) {
                        vlc->bits = (unsigned char) codelen;
                        vlc->code = code;
                        ++vlc;
                    }
                }
                skip(currcnt);
            }
            while (remain--) {
                vlc->bits = 0;
                ++vlc;
            }
        }
        if (m_length)
            returnResult(kMalformatted);
    }

    void decodeDQT(void) {
        unsigned char *t;
        decodeLength();
        while (m_length >= 65) {
            int i = m_position[0];
            if (i & 0xFC)
                returnResult(kMalformatted);
            t = &m_qtab[i][0];
            for (i = 0; i < 64;  ++i)
                t[i] = m_position[i + 1];
            skip(65);
        }
        if (m_length)
            returnResult(kMalformatted);
    }

    void decodeDRI(void) {
        decodeLength();
        if (m_length < 2)
            returnResult(kMalformatted);
        m_rstinterval = decode16(m_position);
        skip(m_length);
    }

    int getCoding(vlcCode* vlc, unsigned char* code) {
        int value = viewBits(16);
        int bits = vlc[value].bits;
        if (!bits) {
            m_error = kMalformatted;
            return 0;
        }
        skipBits(bits);
        value = vlc[value].code;
        if (code)
            *code = (unsigned char) value;
        if (!(bits = value & 15))
            return 0;
        if ((value = getBits(bits)) < (1 << (bits - 1)))
            value += ((-1) << bits) + 1;
        return value;
    }

    void decodeBlock(component* c, unsigned char* out) {
        unsigned char code = 0;
        int coef = 0;
        memset(m_block, 0, sizeof(m_block));
        c->dcpred += getCoding(&m_vlctab[c->dctabsel][0], NULL);
        m_block[0] = (c->dcpred) * m_qtab[c->qtsel][0];
        do {
            int value = getCoding(&m_vlctab[c->actabsel][0], &code);
            if (!code)
                break; // EOB
            if (!(code & 0x0F) && (code != 0xF0))
                returnResult(kMalformatted);
            coef += (code >> 4) + 1;
            if (coef > 63)
                returnResult(kMalformatted);
            m_block[(size_t)m_zz[coef]] = value * m_qtab[c->qtsel][coef];
        } while (coef < 63);
        for (coef = 0;  coef < 64;  coef += 8)
            rowIDCT(&m_block[coef]);
        for (coef = 0;  coef < 8;  ++coef)
            columnIDCT(&m_block[coef], &out[coef], c->stride);
    }

    void decodeScanlines(void) {
        size_t i;
        size_t rstcount = m_rstinterval;
        size_t nextrst = 0;
        component* c;
        decodeLength();
        if (m_length < int(4 + 2 * m_bpp))
            returnResult(kMalformatted);
        if (m_position[0] != m_bpp)
            returnResult(kUnsupported);
        skip(1);
        for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
            if (m_position[0] != c->cid)
                returnResult(kMalformatted);
            if (m_position[1] & 0xEE)
                returnResult(kMalformatted);
            c->dctabsel = m_position[1] >> 4;
            c->actabsel = (m_position[1] & 1) | 2;
            skip(2);
        }
        if (m_position[0] || (m_position[1] != 63) || m_position[2])
            returnResult(kUnsupported);
        skip(m_length);
        for (int mby = 0; mby < m_mbheight; ++mby) {
            for (int mbx = 0; mbx < m_mbwidth; ++mbx) {
                for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
                    for (int sby = 0; sby < c->ssy; ++sby) {
                        for (int sbx = 0; sbx < c->ssx; ++sbx) {
                            decodeBlock(c, &c->pixels[((mby * c->ssy + sby) * c->stride + mbx * c->ssx + sbx) << 3]);
                            if (m_error)
                                return;
                        }
                    }
                }
                if (m_rstinterval && !(--rstcount)) {
                    alignBits();
                    i = getBits(16);
                    if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != nextrst))
                        returnResult(kMalformatted);
                    nextrst = (nextrst + 1) & 7;
                    rstcount = m_rstinterval;
                    for (i = 0;  i < 3;  ++i)
                        m_comp[i].dcpred = 0;
                }
            }
        }
        m_error = kFinished;
    }

    // http://www.media.mit.edu/pia/Research/deepview/exif.html
    uint16_t exifRead16(const unsigned char *data) {
        if (m_exifLittleEndian)
            return data[0] | (data[1] << 8);
        return (data[0] << 8) | data[1];
    }

    uint32_t exifRead32(const unsigned char *data) {
        if (m_exifLittleEndian)
            return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }

    void decodeExif(void) {
        decodeLength();
        const unsigned char *pos = m_position;
        if (m_length < 18)
            return;

        size_t size = m_length;
        skip(size);

        if (!memcmp(pos, "Exif\0\0II*\0", 10))
            m_exifLittleEndian = true;
        else if (!memcmp(pos, "Exif\0\0MM\0*", 10))
            m_exifLittleEndian = false;
        else
            returnResult(kMalformatted);

        uint32_t i = exifRead32(pos + 10) + 6;
        if ((i < 14) || (i > size - 2))
            return;

        pos += i;
        size -= i;
        uint16_t count = exifRead16(pos);
        i = (size - 2) / 12;
        if (count > i)
            return;

        // read all the Exif information until we find a YCbCrPositioning tag
        pos += 2;
        while (count--) {
            if (exifRead16(pos) == 0x0213 && exifRead16(pos + 2) == 3 && exifRead32(pos + 4) == 1) {
                m_coSitedChroma = !!(exifRead16(pos + 8) == 2);
                return;
            }
            pos += 12;
        }
    }

    static constexpr int kCF4A = -9;
    static constexpr int kCF4B = 111;
    static constexpr int kCF4C = 29;
    static constexpr int kCF4D = -3;
    static constexpr int kCF3A = 28;
    static constexpr int kCF3B = 109;
    static constexpr int kCF3C = -9;
    static constexpr int kCF3X = 104;
    static constexpr int kCF3Y = 27;
    static constexpr int kCF3Z = -3;
    static constexpr int kCF2A = 139;
    static constexpr int kCF2B = -11;

    template <size_t clipAdd, size_t clipShift>
    unsigned char clipGen(const size_t x) {
        return clip((x + clipAdd) >> clipShift);
    }
    unsigned char CF(const size_t x) {
        return clipGen<64, 7>(x);
    }
    unsigned char SF(const size_t x) {
        return clipGen<8, 4>(x);
    }

    // bicubic chroma upsampler
    void upSampleCenteredH(component* c) {
        const size_t xmax = c->width - 3;
        u::vector<unsigned char> out;
        out.resize((c->width * c->height) << 1);
        unsigned char *lin = &c->pixels[0];
        unsigned char *lout = &out[0];
        for (size_t y = c->height; y; --y) {
            lout[0] = CF(kCF2A * lin[0] + kCF2B * lin[1]);
            lout[1] = CF(kCF3X * lin[0] + kCF3Y * lin[1] + kCF3Z * lin[2]);
            lout[2] = CF(kCF3A * lin[0] + kCF3B * lin[1] + kCF3C * lin[2]);
            for (size_t x = 0; x < xmax; ++x) {
                lout[(x << 1) + 3] = CF(kCF4A * lin[x] + kCF4B * lin[x + 1] + kCF4C * lin[x + 2] + kCF4D * lin[x + 3]);
                lout[(x << 1) + 4] = CF(kCF4D * lin[x] + kCF4C * lin[x + 1] + kCF4B * lin[x + 2] + kCF4A * lin[x + 3]);
            }
            lin += c->stride;
            lout += c->width << 1;
            lout[-3] = CF(kCF3A * lin[-1] + kCF3B * lin[-2] + kCF3C * lin[-3]);
            lout[-2] = CF(kCF3X * lin[-1] + kCF3Y * lin[-2] + kCF3Z * lin[-3]);
            lout[-1] = CF(kCF2A * lin[-1] + kCF2B * lin[-2]);
        }
        c->width <<= 1;
        c->stride = c->width;
        c->pixels = u::move(out);
    }

    void upSampleCenteredV(component* c) {
        const size_t w = c->width;
        const size_t s1 = c->stride;
        const size_t s2 = s1 + s1;

        u::vector<unsigned char> out;
        out.resize((c->width * c->height) << 1);

        for (size_t x = 0; x < w; ++x) {
            unsigned char *cin = &c->pixels[x];
            unsigned char *cout = &out[x];
            *cout = CF(kCF2A * cin[0] + kCF2B * cin[s1]);
            cout += w;
            *cout = CF(kCF3X * cin[0] + kCF3Y * cin[s1] + kCF3Z * cin[s2]);
            cout += w;
            *cout = CF(kCF3A * cin[0] + kCF3B * cin[s1] + kCF3C * cin[s2]);
            cout += w;
            cin += s1;
            for (size_t y = c->height - 3; y; --y) {
                *cout = CF(kCF4A * cin[-s1] + kCF4B * cin[0] + kCF4C * cin[s1] + kCF4D * cin[s2]);
                cout += w;
                *cout = CF(kCF4D * cin[-s1] + kCF4C * cin[0] + kCF4B * cin[s1] + kCF4A * cin[s2]);
                cout += w;
                cin += s1;
            }
            cin += s1;
            *cout = CF(kCF3A * cin[0] + kCF3B * cin[-s1] + kCF3C * cin[-s2]);
            cout += w;
            *cout = CF(kCF3X * cin[0] + kCF3Y * cin[-s1] + kCF3Z * cin[-s2]);
            cout += w;
            *cout = CF(kCF2A * cin[0] + kCF2B * cin[-s1]);
        }

        c->height <<= 1;
        c->stride = c->width;
        c->pixels = u::move(out);
    }

    void upSampleCositedH(component *c) {
        const size_t xmax = c->width - 1;
        u::vector<unsigned char> out;
        out.resize((c->width * c->height) << 1);

        unsigned char *lin = &c->pixels[0];
        unsigned char *lout = &out[0];

        for (size_t y = c->height; y; --y) {
            lout[0] = lin[0];
            lout[1] = SF((lin[0] << 3) + 9 * lin[1] - lin[2]);
            lout[2] = lin[1];
            for (size_t x = 2; x < xmax; ++x) {
                lout[(x << 1) - 1] = SF(9 * (lin[x - 1] + lin[x]) - (lin[x - 2] + lin[x + 1]));
                lout[x << 1] = lin[x];
            }
            lin += c->stride;
            lout += c->width << 1;
            lout[-3] = SF((lin[-1] << 3) + 9 * lin[-2] - lin[-3]);
            lout[-2] = lin[-1];
            lout[-1] = SF(17 * lin[-1] - lin[-2]);
        }
        c->width <<= 1;
        c->stride = c->width;
        c->pixels = u::move(out);
    }

    void upSampleCositedV(component *c) {
        const size_t w = c->width;
        const size_t s1 = c->stride;
        const size_t s2 = s1 + s1;

        u::vector<unsigned char> out;
        out.resize((c->width * c->height) << 1);

        for (size_t x = 0; x < w; ++x) {
            unsigned char *cin = &c->pixels[x];
            unsigned char *cout = &out[x];
            *cout = cin[0];
            cout += w;
            *cout = SF((cin[0] << 3) + 9 * cin[s1] - cin[s2]);
            cout += w;
            *cout = cin[s1];
            cout += w;
            cin += s1;
            for (size_t y = c->height - 3; y; --y) {
                *cout = SF(9 * (cin[0] + cin[s1]) - (cin[-s1] + cin[s2]));
                cout += w;
                *cout = cin[s1];  cout += w;
                cin += s1;
            }
            *cout = SF((cin[s1] << 3) + 9 * cin[0] - cin[-s1]);
            cout += w;
            *cout = cin[-s1];  cout += w;
            *cout = SF(17 * cin[s1] - cin[0]);
        }
        c->height <<= 1;
        c->stride = c->width;
        c->pixels = u::move(out);
    }

    // fast pixel repetition upsampler
    void upSampleFast(component *c) {
        size_t xshift = 0;
        size_t yshift = 0;

        while (c->width < m_width) c->width <<= 1, ++xshift;
        while (c->height < m_height) c->height <<= 1, ++yshift;

        u::vector<unsigned char> out;
        out.resize(c->width * c->height);

        unsigned char *lin = &c->pixels[0];
        unsigned char *lout = &out[0];

        for (size_t y = 0; y < c->height; ++y) {
            lin = &c->pixels[(y >> yshift) * c->stride];
            for (size_t x = 0; x < c->width; ++x)
                lout[x] = lin[x >> xshift];
            lout += c->width;
        }

        c->stride = c->width;
        c->pixels = u::move(out);
    }

    void convert(chromaFilter filter) {
        size_t i;
        component* c;
        for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
            if (filter == kBicubic) {
                while (c->width < m_width || c->height < m_height) {
                    if (c->width < m_width) {
                        if (m_coSitedChroma)
                            upSampleCositedH(c);
                        else
                            upSampleCenteredH(c);
                    }
                    if (m_error)
                        return;
                    if (c->height < m_height) {
                        if (m_coSitedChroma)
                            upSampleCositedV(c);
                        else
                            upSampleCenteredV(c);
                    }
                    if (m_error)
                        return;
                }
            } else if (filter == kPixelRepetition) {
                if (c->width < m_width || c->height < m_height)
                    upSampleFast(c);
                if (m_error)
                    return;
            }
            if (c->width < m_width || c->height < m_height)
                returnResult(kInternalError);
        }
        if (m_bpp == 3) {
            // convert to RGB24
            unsigned char *prgb = &m_rgb[0];
            const unsigned char *py  = &m_comp[0].pixels[0];
            const unsigned char *pcb = &m_comp[1].pixels[0];
            const unsigned char *pcr = &m_comp[2].pixels[0];
            for (size_t yy = m_height; yy; --yy) {
                for (size_t x = 0; x < m_width; ++x) {
                    register int y = py[x] << 8;
                    register int cb = pcb[x] - 128;
                    register int cr = pcr[x] - 128;
                    *prgb++ = clip((y            + 359 * cr + 128) >> 8);
                    *prgb++ = clip((y -  88 * cb - 183 * cr + 128) >> 8);
                    *prgb++ = clip((y + 454 * cb            + 128) >> 8);
                }
                py += m_comp[0].stride;
                pcb += m_comp[1].stride;
                pcr += m_comp[2].stride;
            }
        } else if (m_comp[0].width != m_comp[0].stride) {
            // grayscale -> only remove stride
            unsigned char *pin = &m_comp[0].pixels[0] + m_comp[0].stride;
            unsigned char *pout = &m_comp[0].pixels[0] + m_comp[0].width;
            for (int y = m_comp[0].height - 1; y; --y) {
                memcpy(pout, pin, m_comp[0].width);
                pin += m_comp[0].stride;
                pout += m_comp[0].width;
            }
            m_comp[0].stride = m_comp[0].width;
        }
    }

    result decode(const u::vector<unsigned char> &data, chromaFilter filter) {
        m_position = (const unsigned char*)&data[0];
        m_size = data.size() & 0x7FFFFFFF;

        if (m_size < 2)
            return kInvalid;
        if ((m_position[0] ^ 0xFF) | (m_position[1] ^ 0xD8))
            return kInvalid;
        skip(2);
        while (!m_error) {
            if ((m_size < 2) || (m_position[0] != 0xFF))
                return kMalformatted;

            skip(2);

            switch (m_position[-1]) {
                case 0xC0:
                    decodeSOF();
                    break;
                case 0xC4:
                    decodeDHT();
                    break;
                case 0xDB:
                    decodeDQT();
                    break;
                case 0xDD:
                    decodeDRI();
                    break;
                case 0xDA:
                    decodeScanlines();
                    break;
                case 0xFE:
                    skipMarker();
                    break;
                case 0xE1:
                    decodeExif();
                    break;

                default:
                    if ((m_position[-1] & 0xF0) == 0xE0)
                        skipMarker();
                    else
                        return kUnsupported;
            }
        }

        if (m_error != kFinished)
            return m_error;

        m_error = kSuccess;
        convert(filter);

        return m_error;
    }

    component m_comp[3];
    vlcCode m_vlctab[4][65536];
    const unsigned char *m_position;
    unsigned char m_qtab[4][64];
    u::vector<unsigned char> m_rgb;
    size_t m_rstinterval;
    int m_size;
    int m_length;
    int m_mbwidth;
    int m_mbheight;
    int m_mbsizex;
    int m_mbsizey;
    int m_buf;
    int m_bufbits;
    int m_block[64];
    bool m_exifLittleEndian;
    bool m_coSitedChroma;
    static const unsigned char m_zz[64];
};

const unsigned char jpeg::m_zz[64] = {
    0,   1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

///
/// PNG decoder
///
/// Supports all the following bitdepths and color types of PNG
///
/// | bitDepth | colorType | description             |
/// |    <=  8 |         0 | greyscale (no alpha)    |
/// |        8 |         2 | RGB24                   |
/// |    <=  8 |         3 | indexed color (palette) |
/// |        8 |         4 | greyscale (with alpha)  |
/// |        8 |         6 | RGBA32                  |
/// |       16 |         0 | greyscale (no alpha)    |
/// |       16 |         2 | RGB48                   |
/// |       16 |         4 | greyscale (with alpha)  |
/// |       16 |         6 | RGBA64                  |
///
struct png : decoder {
    png(const u::vector<unsigned char> &data) {
        decode(m_decoded, data);
    }

    u::vector<unsigned char> data(void) {
        return u::move(m_decoded);
    }

private:
    void decode(u::vector<unsigned char>& out, const u::vector<unsigned char> &invec) {
        const unsigned char *const in = &invec[0];
        readHeader(&in[0], invec.size());
        if (m_error)
            return;

        bool IEND = false;
        size_t pos = 33;
        u::vector<unsigned char> idat;

        while (!IEND) {
            if (pos + 8 >= invec.size())
                returnResult(kMalformatted);

            const size_t chunkLength = readWord(&in[pos]);

            if (chunkLength > INT32_MAX)
                returnResult(kMalformatted);
            if (pos + chunkLength >= invec.size())
                returnResult(kMalformatted);

            pos += 4;
            if (!memcmp(in + pos, "IDAT", 4)) {
                idat.insert(idat.end(), &in[pos + 4], &in[pos + 4 + chunkLength]);
                pos += (4 + chunkLength);
            } else if (!memcmp(in + pos, "IEND", 4)) {
                pos += 4;
                IEND = true;
            } else if (!memcmp(in + pos, "PLTE", 4)) {
                pos += 4; //go after the 4 letters
                m_palette.resize(4 * (chunkLength / 3));

                if (m_palette.size() > (4 * 256))
                    returnResult(kMalformatted);

                for (size_t i = 0; i < m_palette.size(); i += 4) {
                    for (size_t j = 0; j < 3; j++)
                        m_palette[i + j] = in[pos++]; // RGB
                    m_palette[i + 3] = 255; // alpha
                }
            } else if (!memcmp(in + pos, "tRNS", 4)) {
                pos += 4;
                if (m_colorType == 3) {
                    if (4 * chunkLength > m_palette.size())
                        returnResult(kMalformatted);

                    for (size_t i = 0; i < chunkLength; i++)
                        m_palette[4 * i + 3] = in[pos++];

                } else if (m_colorType == 0) {
                    if (chunkLength != 2)
                        returnResult(kMalformatted);
                    const size_t key = 256 * in[pos] + in[pos + 1];
                    m_hasChromaKey = 1;
                    m_chromaKeyRed = key;
                    m_chromaKeyGreen = key;
                    m_chromaKeyBlue = key;
                    pos += 2;
                } else if(m_colorType == 2) {
                    if (chunkLength != 6)
                        returnResult(kMalformatted);
                    m_hasChromaKey = 1;
                    m_chromaKeyRed = 256 * in[pos] + in[pos + 1]; pos += 2;
                    m_chromaKeyGreen = 256 * in[pos] + in[pos + 1]; pos += 2;
                    m_chromaKeyBlue = 256 * in[pos] + in[pos + 1]; pos += 2;
                } else
                    returnResult(kMalformatted);
            } else {
                // unknown critical chunk (5th bit of the first byte of chunk type is 0)
                if (!(in[pos + 0] & 32))
                    returnResult(kMalformatted);
                pos += (chunkLength + 4);
            }
            pos += 4; // step over CRC
        }

        const size_t bpp = calculateBitsPerPixel();
        m_bpp = bpp / 8;
        u::vector<unsigned char> scanlines(((m_width * (m_height * bpp + 7)) / 8) + m_height);
        u::zlib zlib;
        if (!zlib.decompress(scanlines, idat))
            returnResult(kMalformatted);

        const size_t bytewidth = (bpp + 7) / 8;
        const size_t outlength = (m_height * m_width * bpp + 7) / 8;

        out.resize(outlength);
        unsigned char* out_ = outlength ? &out[0] : 0;

        // no interlace, just filter
        if (m_interlaceMethod == 0) {
            size_t linestart = 0;
            const size_t linelength = (m_width * bpp + 7) / 8;
            if (bpp >= 8) {
                for (size_t y = 0; y < m_height; y++) {
                    const size_t filterType = scanlines[linestart];
                    const unsigned char *prevline = (y == 0)
                        ? 0 : &out_[(y - 1) * m_width * bytewidth];
                    unfilterScanline(&out_[linestart - y], &scanlines[linestart + 1],
                        prevline, bytewidth, filterType, linelength);
                    if (m_error)
                        return;
                    linestart += (1 + linelength);
                }
            } else {
                u::vector<unsigned char> templine((m_width * bpp + 7) >> 3);
                for (size_t y = 0, obp = 0; y < m_height; y++) {
                    const size_t filterType = scanlines[linestart];
                    const unsigned char* prevline = (y == 0) ? 0 : &out_[(y - 1) * m_width * bytewidth];
                    unfilterScanline(&templine[0], &scanlines[linestart + 1],
                        prevline, bytewidth, filterType, linelength);
                    if (m_error)
                        return;
                    for (size_t bp = 0; bp < m_width * bpp;)
                        setBitReversed(obp, out_, readBitReverse(bp, &templine[0]));
                    linestart += (1 + linelength);
                }
            }
        } else {
            // adam7 interlaced
            size_t passw[7] = {
                (m_width + 7) / 8, (m_width + 3) / 8,
                (m_width + 3) / 4, (m_width + 1) / 4,
                (m_width + 1) / 2, (m_width + 0) / 2,
                (m_width + 0) / 1
            };
            size_t passh[7] = {
                (m_height + 7) / 8,
                (m_height + 7) / 8, (m_height + 3) / 8,
                (m_height + 3) / 4, (m_height + 1) / 4,
                (m_height + 1) / 2, (m_height + 0) / 2
            };

            size_t passstart[7] = {0};

            for (size_t i = 0; i < 6; i++)
                passstart[i + 1] = passstart[i] + passh[i] * ((passw[i] ? 1 : 0) + (passw[i] * bpp + 7) / 8);

            u::vector<unsigned char> scanlineo((m_width * bpp + 7) / 8);
            u::vector<unsigned char> scanlinen((m_width * bpp + 7) / 8);

            for (size_t i = 0; i < 7; i++) {
                adam7Pass(out_, &scanlinen[0], &scanlineo[0],
                    &scanlines[passstart[i]], m_width, i, passw[i], passh[i], bpp);
            }
        }
    }

    void readHeader(const unsigned char* in, size_t inlength) {
        if (inlength < 29)
            returnResult(kInvalid);
        if (memcmp(in, "\x89\x50\x4E\x47\xD\xA\x1A\xA", 8))
            returnResult(kInvalid);
        if (memcmp(in + 12, "IHDR", 4))
            returnResult(kInvalid);

        m_width = readWord(&in[16]);
        m_height = readWord(&in[20]);
        m_bitDepth = in[24];
        m_colorType = in[25];
        m_compressionMethod = in[26];
        // only compression method 0 is allowed in the specification
        if (in[26] != 0)
            returnResult(kMalformatted);
        m_filterMethod = in[27];
        // only filter method 0 is allowed in the specification
        if (in[27] != 0)
            returnResult(kMalformatted);
        m_interlaceMethod = in[28];
        // only interlace methods 0 and 1 exist in the specification
        if (in[28] > 1)
            returnResult(kMalformatted);
        m_error = validateColor(m_colorType, m_bitDepth);
    }

    void unfilterScanline(unsigned char* recon, const unsigned char* scanline,
        const unsigned char* precon, size_t bytewidth, size_t filterType, size_t length)
    {
        switch (filterType) {
            case 0:
                for (size_t i = 0; i < length; i++)
                    recon[i] = scanline[i];
                break;
            case 1:
                for (size_t i = 0; i < bytewidth; i++)
                    recon[i] = scanline[i];
                for (size_t i = bytewidth; i < length; i++)
                    recon[i] = scanline[i] + recon[i - bytewidth];
                break;
            case 2:
                if (precon)
                    for(size_t i = 0; i < length; i++)
                        recon[i] = scanline[i] + precon[i];
                else
                    for(size_t i = 0; i < length; i++) recon[i] = scanline[i];
                break;
            case 3:
                if (precon) {
                    for (size_t i = 0; i < bytewidth; i++)
                        recon[i] = scanline[i] + precon[i] / 2;
                    for (size_t i = bytewidth; i < length; i++)
                        recon[i] = scanline[i] + ((recon[i - bytewidth] + precon[i]) / 2);
                } else {
                    for (size_t i = 0; i < bytewidth; i++)
                        recon[i] = scanline[i];
                    for (size_t i = bytewidth; i < length; i++)
                        recon[i] = scanline[i] + recon[i - bytewidth] / 2;
                }
                break;
            case 4:
                if (precon) {
                    for (size_t i = 0; i < bytewidth; i++)
                        recon[i] = scanline[i] + paethPredictor(0, precon[i], 0);
                    for (size_t i = bytewidth; i < length; i++)
                        recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], precon[i], precon[i - bytewidth]);
                } else {
                    for(size_t i = 0; i < bytewidth; i++)
                        recon[i] = scanline[i];
                    for(size_t i = bytewidth; i < length; i++)
                        recon[i] = scanline[i] + paethPredictor(recon[i - bytewidth], 0, 0);
                }
                break;
            default:
                returnResult(kMalformatted);
        }
    }


    void adam7Pass(unsigned char* out, unsigned char* linen, unsigned char* lineo,
        const unsigned char* in, size_t w, size_t i, size_t passw, size_t passh, size_t bpp)
    {
        if (passw == 0)
            return;

        const size_t passleft = m_adam7Pattern[i];
        const size_t passtop = m_adam7Pattern[i + 7];
        const size_t spacex = m_adam7Pattern[i + 14];
        const size_t spacey = m_adam7Pattern[i + 21];

        const size_t bytewidth = (bpp + 7) / 8;
        const size_t linelength = 1 + ((bpp * passw + 7) / 8);

        for (size_t y = 0; y < passh; y++) {
            const unsigned char filterType = in[y * linelength];
            const unsigned char *const prevline = (y == 0) ? 0 : lineo;

            unfilterScanline(linen, &in[y * linelength + 1], prevline,
                bytewidth, filterType, (w * bpp + 7) / 8);

            if (m_error)
                return;

            if (bpp >= 8) {
                for (size_t i = 0; i < passw; i++)
                    for (size_t b = 0; b < bytewidth; b++) // b = current byte of this pixel
                out[bytewidth * w * (passtop + spacey * y) + bytewidth * (passleft + spacex * i) + b] = linen[bytewidth * i + b];
            } else {
                for (size_t i = 0; i < passw; i++) {
                    size_t obp = bpp * w * (passtop + spacey * y) + bpp * (passleft + spacex * i);
                    size_t bp = i * bpp;
                    for (size_t b = 0; b < bpp; b++)
                        setBitReversed(obp, out, readBitReverse(bp, &linen[0]));
                }
            }
            unsigned char *temp = linen;
            linen = lineo;
            lineo = temp;
        }
    }

    size_t readBitReverse(size_t& bitp, const unsigned char* bits) {
        size_t r = (bits[bitp >> 3] >> (7 - (bitp & 0x7))) & 1;
        bitp++;
        return r;
    }

    size_t readBitsReverse(size_t& bitp, const unsigned char* bits, size_t nbits) {
        size_t r = 0;
        for (size_t i = nbits - 1; i < nbits; i--)
            r += ((readBitReverse(bitp, bits)) << i);
        return r;
    }

    void setBitReversed(size_t& bitp, unsigned char* bits, size_t bit) {
        bits[bitp >> 3] |=  (bit << (7 - (bitp & 0x7)));
        bitp++;
    }

    size_t readWord(const unsigned char* buffer) {
        return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    }

    result validateColor(size_t colorType, size_t bd) {
        if (colorType == 2 || colorType == 4 || colorType == 6)
            return (bd != 8 && bd != 16)
                ? kMalformatted : kSuccess;
        else if (colorType == 0)
            return (bd != 1 && bd != 2 && bd != 4 && bd != 8 && bd != 16)
                ? kMalformatted : kSuccess;
        else if (colorType == 3)
            return (bd != 1 && bd != 2 && bd != 4 && bd != 8)
                ? kMalformatted : kSuccess;
        return kMalformatted;
    }

    size_t calculateBitsPerPixel(void) const {
        if (m_colorType == 2)
            return (3 * m_bitDepth);
        else if (m_colorType >= 4)
            return (m_colorType - 2) * m_bitDepth;
        return m_bitDepth;
    }

    // Paeth predicter
    unsigned char paethPredictor(short a, short b, short c) {
        short p = a + b - c;
        short pa = p > a ? (p - a) : (a - p);
        short pb = p > b ? (p - b) : (b - p);
        short pc = p > c ? (p - c) : (c - p);
        return (unsigned char)((pa <= pb && pa <= pc) ? a : pb <= pc ? b : c);
    }

protected:
    size_t m_colorType;
    size_t m_bitDepth;
    size_t m_compressionMethod;
    size_t m_filterMethod;
    size_t m_interlaceMethod;
    size_t m_chromaKeyRed;
    size_t m_chromaKeyGreen;
    size_t m_chromaKeyBlue;
    bool m_hasChromaKey;
    u::vector<unsigned char> m_palette;
    u::vector<unsigned char> m_decoded;
    static const size_t m_adam7Pattern[28];
};

// values for the adam7 passes
const size_t png::m_adam7Pattern[28]  = {
    0, 4, 0, 2, 0, 1, 0, 0, 0, 4, 0, 2, 0, 1,
    8, 8, 4, 4, 2, 2, 1, 8, 8, 8, 4, 4, 2, 2
};

///
/// TGA decoder
///
struct tga : decoder {
    tga(const u::vector<unsigned char> &data) {
        decode(data);
    }

    u::vector<unsigned char> data(void) {
        return u::move(m_data);
    }

private:
    void read(unsigned char *dest, size_t size) {
        memcpy(dest, m_position, size);
        seek(size);
    }

    int get(void) {
        return *m_position++;
    }

    void seek(size_t where) {
        m_position += where;
    }

    result decode(const u::vector<unsigned char> &data) {
        m_position = &data[0];
        read((unsigned char *)&header, sizeof(header));
        seek(header.identSize);

        if (!memchr("\x8\x18\x20", header.pixelSize, 3))
            return kUnsupported;

        m_bpp = header.pixelSize / 8;
        m_width = header.width[0] | (header.width[1] << 8);
        m_height = header.height[0] | (header.height[1] << 8);

        switch (header.imageType) {
            case 1:
                decodeColor();
                break;
            case 2:
                decodeImage();
                break;
            case 9:
                decodeColorRLE();
                break;
            case 10:
                decodeImageRLE();
                break;
            default:
                return kUnsupported;
        }

        return m_error ? m_error : kSuccess;
    }

    void decodeColor(void) {
        size_t colorMapSize = header.colorMapSize[0] | (header.colorMapSize[1] << 8);
        if (!memchr("\x8\x18\x20", header.colorMapEntrySize, 3))
            returnResult(kUnsupported);

        m_bpp = header.colorMapEntrySize / 8;
        u::vector<unsigned char> colorMap;
        colorMap.resize(m_bpp * colorMapSize);
        read(&colorMap[0], m_bpp * colorMapSize);

        if (m_bpp >= 3)
            convert(&colorMap[0], m_bpp * colorMapSize, m_bpp);

        m_data.resize(m_bpp * m_width * m_height);
        unsigned char *indices = &m_data[(m_bpp - 1) * m_width * m_height];
        read(indices, m_width * m_height);
        unsigned char *src = indices;
        unsigned char *dst = &m_data[m_bpp * m_width * m_height];

        for (size_t i = 0; i < m_height; i++) {
            dst -= m_bpp * m_width;
            unsigned char *row = dst;
            for (size_t j = 0; j < m_width; j++) {
                memcpy(row, &colorMap[*src++ * m_bpp], m_bpp);
                row += m_bpp;
            }
        }
    }

    void decodeImage(void) {
        m_data.resize(m_bpp * m_width * m_height);
        unsigned char *dst = &m_data[m_bpp * m_width * m_height];
        for (size_t i = 0; i < m_height; i++) {
            dst -= m_bpp * m_width;
            read(dst, m_bpp * m_width);
        }
        if (m_bpp >= 3)
            convert(&m_data[0], m_bpp * m_width * m_height, m_bpp);
    }

    void decodeColorRLE(void) {
        size_t colorMapSize = header.colorMapSize[0] | (header.colorMapSize[1] << 8);
        if (!memchr("\x8\x18\x20", header.colorMapEntrySize, 3))
            returnResult(kUnsupported);

        m_bpp = header.colorMapEntrySize / 8;
        u::vector<unsigned char> colorMap;
        colorMap.resize(m_bpp * colorMapSize);
        read(&colorMap[0], m_bpp * colorMapSize);

        if (m_bpp >= 3)
            convert(&colorMap[0], m_bpp * colorMapSize, m_bpp);

        m_data.resize(m_bpp * m_width * m_height);

        unsigned char buffer[128];
        for (unsigned char *end = &m_data[m_bpp * m_width * m_height], *dst = end - m_bpp * m_width; dst >= &m_data[0]; ) {
            int c = get();
            if (c & 0x80) {
                int index = get();
                const unsigned char *column = &colorMap[index * m_bpp];
                c -= 0x7F;
                c *= m_bpp;

                while (c > 0 && dst >= &m_data[0]) {
                    int n = u::min(c, int(end - dst));
                    for (unsigned char *run = dst + n; dst < run; dst += m_bpp)
                        memcpy(dst, column, m_bpp);
                    c -= n;
                    if (dst >= end) {
                        end -= m_bpp * m_width;
                        dst = end - m_bpp * m_width;
                    }
                }
            } else {
                c += 1;
                while (c > 0 && dst >= &m_data[0]) {
                    int n = u::min(c, int(end - dst) / int(m_bpp));
                    read(buffer, n);
                    for (unsigned char *src = buffer; src <= &buffer[n]; dst += m_bpp)
                        memcpy(dst, &colorMap[*src++ * m_bpp], m_bpp);
                    c -= n;
                    if (dst >= end) {
                        end -= m_bpp * m_width;
                        dst = end - m_bpp * m_width;
                    }
                }
            }
        }
    }

    void decodeImageRLE(void) {
        m_data.resize(m_bpp * m_width * m_height);
        unsigned char buffer[4];
        for (unsigned char *end = &m_data[m_bpp * m_width * m_height], *dst = end - m_bpp * m_width; dst >= &m_data[0]; ) {
            int c = get();
            if (c & 0x80) {
                read(buffer, m_bpp);
                c -= 0x7F;
                if (m_bpp >= 3)
                    u::swap(buffer[0], buffer[2]);
                c *= m_bpp;

                while (c > 0) {
                    int n = u::min(c, int(end - dst));
                    for (unsigned char *run = dst + n; dst < run; dst += m_bpp)
                        memcpy(dst, buffer, m_bpp);
                    c -= n;
                    if (dst >= end) {
                        end -= m_bpp * m_width;
                        dst = end - m_bpp * m_width;
                        if (dst < &m_data[0])
                            break;
                    }
                }
            } else {
                c += 1;
                c *= m_bpp;
                while (c > 0) {
                    int n = u::min(c, int(end - dst));
                    read(dst, n);
                    if (m_bpp >= 3)
                        convert(dst, n, m_bpp);
                    dst += n;
                    c -= n;
                    if (dst >= end) {
                        end -= m_bpp * m_width;
                        dst = end - m_bpp * m_width;
                        if (dst < &m_data[0])
                            break;
                    }
                }
            }
        }
    }

    void convert(unsigned char *data, size_t length, size_t m_bpp) {
        for (unsigned char *end = &data[length]; data < end; data += m_bpp)
            u::swap(data[0], data[2]);
    }

    struct {
        unsigned char identSize;
        unsigned char colorMapType;
        unsigned char imageType;
        unsigned char colorMapOrigin[2];
        unsigned char colorMapSize[2];
        unsigned char colorMapEntrySize;
        unsigned char xorigin[2];
        unsigned char yorigin[2];
        unsigned char width[2];
        unsigned char height[2];
        unsigned char pixelSize;
        unsigned char description;
    } header;

    const unsigned char *m_position;
    u::vector<unsigned char> m_data;
};

///
/// Texture utilities:
///   halve (useful for generating mipmaps), shift, scale and reorient.
///

template <size_t S>
void texture::halve(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst) {
    for (unsigned char *yend = src + sh * stride; src < yend;) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += 2 * S, dst += S) {
            for (size_t i = 0; i < S; i++)
                dst[i] = (size_t(xsrc[i]) + size_t(xsrc[i+S]) + size_t(xsrc[stride+i]) + size_t(xsrc[stride+i+S])) >> 2;
        }
        src += 2 * stride;
    }
}

template <size_t S>
void texture::shift(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = sw/dw, hfrac = sh/dh, wshift = 0, hshift = 0;
    while (dw << wshift < sw) wshift++;
    while (dh << hshift < sh) hshift++;
    size_t tshift = wshift + hshift;
    for (unsigned char *yend = src + sh * stride; src < yend; ) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += wfrac * S, dst += S) {
            size_t r[S] = {0};
            for (unsigned char *ycur = xsrc, *xend = ycur + wfrac * S,
                    *yend = src + hfrac * stride; ycur<yend; ycur += stride, xend += stride) {
                for (unsigned char *xcur = ycur; xcur < xend; xcur += S) {
                    for (size_t i = 0; i < S; i++)
                        r[i] += xcur[i];
                }
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i]) >> tshift;
        }
        src += hfrac*stride;
    }
}

template <size_t S>
void texture::scale(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = (sw << 12) / dw;
    size_t hfrac = (sh << 12) / dh;
    size_t darea = dw * dh;
    size_t sarea = sw * sh;
    int over, under;
    for (over = 0; (darea >> over) > sarea; over++);
    for (under = 0; (darea << under) < sarea; under++);
    size_t cscale = m::clamp(under, over - 12, 12);
    size_t ascale = m::clamp(12 + under - over, 0, 24);
    size_t dscale = ascale + 12 - cscale;
    size_t area = ((unsigned long long)darea << ascale) / sarea;
    dw *= wfrac;
    dh *= hfrac;
    for (size_t y = 0; y < dh; y += hfrac) {
        const size_t yn = y + hfrac - 1;
        const size_t yi = y >> 12;
        const size_t h = (yn >> 12) - yi;
        const size_t ylow = ((yn | (-int(h) >> 24)) & 0xFFFU) + 1 - (y & 0xFFFU);
        const size_t yhigh = (yn & 0xFFFU) + 1;
        const unsigned char *ysrc = src + yi * stride;
        for (size_t x = 0; x < dw; x += wfrac, dst += S) {
            const size_t xn = x + wfrac - 1;
            const size_t xi = x >> 12;
            const size_t w = (xn >> 12) - xi;
            const size_t xlow = ((w + 0xFFFU) & 0x1000U) - (x & 0xFFFU);
            const size_t xhigh = (xn & 0xFFFU) + 1;
            const unsigned char *xsrc = ysrc + xi * S;
            const unsigned char *xend = xsrc + w * S;
            size_t r[S] = {0};
            for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                for (size_t i = 0; i < S; i++)
                    r[i] += xcur[i];
            for (size_t i = 0; i < S; i++)
                r[i] = (ylow * (r[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            if (h) {
                xsrc += stride;
                xend += stride;
                for (size_t hcur = h; --hcur; xsrc += stride, xend += stride) {
                    size_t p[S] = {0};
                    for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                        for (size_t i = 0; i < S; i++)
                            p[i] += xcur[i];
                    for (size_t i = 0; i < S; i++)
                        r[i] += ((p[i] << 12) + xsrc[i] * xlow + xend[i] * xhigh) >> cscale;
                }
                size_t p[S] = {0};
                for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                    for (size_t i = 0; i < S; i++)
                        p[i] += xcur[i];
                for (size_t i = 0; i < S; i++)
                    r[i] += (yhigh * (p[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i] * area) >> dscale;
        }
    }
}

void texture::scale(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t pitch, unsigned char *dst, size_t dw, size_t dh) {
    if (sw == dw * 2 && sh == dh * 2)
        switch (bpp) {
            case 3: return halve<3>(src, sw, sh, pitch, dst);
            case 4: return halve<4>(src, sw, sh, pitch, dst);
        }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1))
        switch (bpp) {
            case 3: return scale<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return scale<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    switch(bpp) {
        case 3: return shift<3>(src, sw, sh, pitch, dst, dw, dh);
        case 4: return shift<4>(src, sw, sh, pitch, dst, dw, dh);
    }
}

void texture::reorient(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t stride, unsigned char *dst, bool flipx, bool flipy, bool swapxy) {
    size_t stridex = swapxy ? bpp * sh : bpp;
    size_t stridey = swapxy ? bpp : bpp * sw;
    if (flipx)
        dst += (sw - 1) * stridex, stridex = -stridex;
    if (flipy)
        dst += (sh - 1) * stridey, stridey = -stridey;
    unsigned char *srcrow = src;
    for (size_t i = 0; i < sh; i++) {
        for (unsigned char *curdst = dst, *src = srcrow, *end = srcrow + sw * bpp; src < end; ) {
            for (size_t k = 0; k < bpp; k++)
                curdst[k] = *src++;
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

enum decoderType  {
    kDecoderNone,
    kDecoderPNG,
    kDecoderJPEG,
    kDecoderTGA
    // TODO DXTn
};

static const struct {
    u::vector<const char *> extensions;
    decoderType decoder;
} decoders[] = {
    { {{"jpg", "jpeg", "jpe", "jif", "jfif", "jfi"}}, kDecoderJPEG },
    { {{"png"}},                                      kDecoderPNG  },
    { {{"tga"}},                                      kDecoderTGA  }
};

template <typename T>
bool texture::decode(const u::vector<unsigned char> &data, const char *name) {
    T decode(data);
    if (decode.status() != decoder::kSuccess) {
        fprintf(stderr, "failed to decode `%s' %s\n", name, decode.error());
        return false;
    }

    switch (decode.bpp()) {
        case 1:
            m_format = TEX_LUMINANCE;
            break;
        case 3:
            m_format = TEX_RGB;
            break;
        case 4:
            m_format = TEX_RGBA;
            break;
    }

    m_width = decode.width();
    m_height = decode.height();
    m_pitch = m_width * decode.bpp();
    m_data = u::move(decode.data());

    return true;
}

bool texture::load(const u::string &file) {
    const char *fileName = file.c_str();

    // load into vector
    auto load = u::read(file, "r");
    if (!load)
        return false;
    u::vector<unsigned char> data = *load;

    // find the appropriate decoder
    const char *extension = strrchr(fileName, '.');
    if (!extension)
        return false;

    decoderType decodeMethod = kDecoderNone;
    for (size_t i = 0; i < sizeof(decoders)/sizeof(*decoders); i++) {
        for (auto &it : decoders[i].extensions) {
            if (strcmp(it, extension + 1))
                continue;
            decodeMethod = decoders[i].decoder;
            break;
        }
    }

    // decode it
    switch (decodeMethod) {
        case kDecoderJPEG:
            if (!decode<jpeg>(data, fileName))
                return false;
            break;
        case kDecoderPNG:
            if (!decode<png>(data, fileName))
                return false;
            break;
        case kDecoderTGA:
            if (!decode<tga>(data, fileName))
                return false;
            break;
        case kDecoderNone:
            fprintf(stderr, "no decoder found for `%s'\n", fileName);
            return false;
    }

    return true;
}

void texture::resize(size_t width, size_t height) {
    u::vector<unsigned char> data;
    data.resize(m_bpp * width * height);
    scale(&m_data[0], m_width, m_height, m_bpp, m_pitch, &data[0], width, height);
    m_data = u::move(data);
    m_width = width;
    m_height = height;
    m_pitch = m_width * m_bpp;
}

size_t texture::width(void) const {
    return m_width;
}

size_t texture::height(void) const {
    return m_height;
}

textureFormat texture::format(void) const {
    return m_format;
}

const unsigned char *texture::data(void) {
    return &m_data[0];
}
