#include "engine.h"
#include "texture.h"
#include "cvar.h"

#include "u_zlib.h"
#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"
#include "u_traits.h"

#include "m_const.h"
#include "m_trig.h"
#include "m_vec.h"

// To render text into textures we use an 8x8 bitmap font
static const uint64_t kFont[128] = {
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x0000000000000000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x0000000000000000,0x0808080800080000,0x2828000000000000,0x00287C287C280000,
    0x081E281C0A3C0800,0x6094681629060000,0x1C20201926190000,0x0808000000000000,
    0x0810202010080000,0x1008040408100000,0x2A1C3E1C2A000000,0x0008083E08080000,
    0x0000000000081000,0x0000003C00000000,0x0000000000080000,0x0204081020400000,
    0x1824424224180000,0x08180808081C0000,0x3C420418207E0000,0x3C420418423C0000,
    0x081828487C080000,0x7E407C02423C0000,0x3C407C42423C0000,0x7E04081020400000,
    0x3C423C42423C0000,0x3C42423E023C0000,0x0000080000080000,0x0000080000081000,
    0x0006186018060000,0x00007E007E000000,0x0060180618600000,0x3844041800100000,
    0x003C449C945C201C,0x1818243C42420000,0x7844784444780000,0x3844808044380000,
    0x7844444444780000,0x7C407840407C0000,0x7C40784040400000,0x3844809C44380000,
    0x42427E4242420000,0x3E080808083E0000,0x1C04040444380000,0x4448507048440000,
    0x40404040407E0000,0x4163554941410000,0x4262524A46420000,0x1C222222221C0000,
    0x7844784040400000,0x1C222222221C0200,0x7844785048440000,0x1C22100C221C0000,
    0x7F08080808080000,0x42424242423C0000,0x8142422424180000,0x4141495563410000,
    0x4224181824420000,0x4122140808080000,0x7E040810207E0000,0x3820202020380000,
    0x4020100804020000,0x3808080808380000,0x1028000000000000,0x00000000007E0000,
    0x1008000000000000,0x003C023E463A0000,0x40407C42625C0000,0x00001C20201C0000,
    0x02023E42463A0000,0x003C427E403C0000,0x0018103810100000,0x0000344C44340438,
    0x2020382424240000,0x0800080808080000,0x0800180808080870,0x20202428302C0000,
    0x1010101010180000,0x0000665A42420000,0x00002E3222220000,0x00003C42423C0000,
    0x00005C62427C4040,0x00003A46423E0202,0x00002C3220200000,0x001C201804380000,
    0x00103C1010180000,0x00002222261A0000,0x0000424224180000,0x000081815A660000,
    0x0000422418660000,0x0000422214081060,0x00003C08103C0000,0x1C103030101C0000,
    0x0808080808080800,0x38080C0C08380000,0x000000324C000000,0x7E7E7E7E7E7E0000
};

VAR(int, tex_jpg_chroma, "chroma filtering method", 0, 1, 0);
VAR(int, tex_jpg_cliplut, "clipping lookup table", 0, 1, 1);
VAR(int, tex_tga_compress, "compress TGA", 0, 1, 1);
VAR(int, tex_png_compress_quality, "compression quality for PNG", 5, 128, 16);
VAR(int, tex_jpg_compress_quality, "compression quality for JPG", 1, 100, 90);

#define returnResult(E) \
    do { \
        m_error = (E); \
        return; \
    } while (0)

struct decoder {
    decoder()
        : m_error(kSuccess)
        , m_format(kTexFormatLuminance)
        , m_width(0)
        , m_height(0)
        , m_pitch(0)
        , m_bpp(0)
        , m_mips(0)
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

    const char *error() const {
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

    size_t width() const {
        return m_width;
    }

    size_t height() const {
        return m_height;
    }

    size_t pitch() const {
        return m_pitch;
    }

    size_t bpp() const {
        return m_bpp;
    }

    size_t mips() const {
        return m_mips;
    }

    textureFormat format() const {
        return m_format;
    }

    result status() const {
        return m_error;
    }

protected:
    result m_error;
    textureFormat m_format;
    size_t m_width;
    size_t m_height;
    size_t m_pitch;
    size_t m_bpp;
    size_t m_mips;
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
    static constexpr const char *kMagic = "\xFF\xD8\xFF";

    static bool test(const u::vector<unsigned char> &data) {
        return memcmp(&data[0], kMagic, 3) == 0;
    }

    enum chromaFilter {
        kBicubic,
        kPixelRepetition
    };

    jpeg(const u::vector<unsigned char> &data)
        : m_rstinterval(0)
        , m_size(0)
        , m_length(0)
        , m_mbwidth(0)
        , m_mbheight(0)
        , m_mbsizex(0)
        , m_mbsizey(0)
        , m_buf(0)
        , m_bufbits(0)
        , m_exifLittleEndian(false)
        , m_coSitedChroma(false)
    {
        memset(m_comp, 0, sizeof m_comp);
        memset(m_vlctab, 0, sizeof m_vlctab);
        memset(m_qtab, 0, sizeof m_qtab);
        memset(m_block, 0, sizeof m_block);

        const chromaFilter filter = chromaFilter(tex_jpg_chroma.get());
        if (tex_jpg_cliplut.get())
            decode<true>(data, filter);
        else
            decode<false>(data, filter);

        switch (m_bpp) {
        case 1:
            m_format = kTexFormatLuminance;
            break;
        case 3:
            m_format = kTexFormatRGB;
            break;
        case 4:
            m_format = kTexFormatRGBA;
            break;
        default:
            U_UNREACHABLE();
            break;
        }

        m_pitch = m_bpp * m_width;
    }

    u::vector<unsigned char> data() {
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

    // Separable 2D integer inverse discrete cosine transform
    //
    // Input coefficients are assumed to have been multiplied by the appropriate
    // quantization table. Fixed point computation is used with the number of
    // bits for the fractional component varying over the intermediate stages.
    //
    // For more information on the algorithm, see Z. Wang, "Fast algorithms for
    // the discrete W transform and the discrete Fourier transform", IEEE Trans.
    // ASSP, Vol. ASSP- 32, pp. 803-816, Aug. 1984.
    //
    // 32-bit integer arithmetic (8-bit coefficients)
    //   Instructions per DCT:
    //    - 11 muls
    //    - 37 adds (if clipping /w branching)
    //    - 29 adds (if clipping /w table)
    //    - 12 branches (if clipping /w table)
    //    - 28 branches (if clipping /w branching)

    unsigned char clip(const int x) {
        return (x < 0) ? 0 : ((x > 0xFF) ? 0xFF : (unsigned char)x);
    }

    // 2KB clipping table, fits in L1 cache.
    // operator()(x) is functionally equivalent to clip(x) above
    static struct IDCTClippingTable {
        IDCTClippingTable()
            : read(table + 512)
        {
            for (int i = -512; i < 512; i++)
                read[i] = m::clamp(i, -256, 255);
            read += 128;
        }
        unsigned char operator()(int index) const {
            return read[index];
        }
        int16_t table[1024];
        int16_t *read;
    } gIDCTClippingTable;

    // 13-bit fixed point representation of trigonometric constants
    static constexpr int kW1 = 2841; // 2048*sqrt(2)*cos(1*pi/16)
    static constexpr int kW2 = 2676; // 2048*sqrt(2)*cos(2*pi/16)
    static constexpr int kW3 = 2408; // 2048*sqrt(2)*cos(3*pi/16)
    static constexpr int kW5 = 1609; // 2048*sqrt(2)*cos(5*pi/16)
    static constexpr int kW6 = 1108; // 2048*sqrt(2)*cos(6*pi/16)
    static constexpr int kW7 = 565;  // 2048*sqrt(2)*cos(7*pi/16)

    // portable signed left shift
    #define sls(X, Y) (int)((unsigned int)(X) << (Y))

    // row (horizontal) IDCT
    //
    //           7                        pi         i
    // dst[k] = sum c[i] * src[i] * cos ( -- * ( k + - ) * i )
    //          i=0                       8          2
    //
    // where: c[0]    = 128
    //        c[1..7] = 128*sqrt(2)
    void rowIDCT(int *U_RESTRICT blk) {
        int x0, x1, x2, x3, x4, x5, x6, x7, x8;
        // if all the AC components are zero, the IDCT is trivial.
        if (!((x1 = sls(blk[4], 11))
            | (x2 = blk[6]) | (x3 = blk[2])
            | (x4 = blk[1]) | (x5 = blk[7])
            | (x6 = blk[5]) | (x7 = blk[3])))
        {
            // short circuit the 1D DCT if the DC component is non-zero
            int value = sls(blk[0], 3);
            for (size_t i = 0; i < 8; i++)
                blk[i] = value;
            return;
        }
        // for proper rounding in fourth stage
        x0 = sls(blk[0], 11) + 128;

        // first stage (prescale)
        x8 = kW7 * (x4 + x5);
        x4 = x8 + (kW1 - kW7) * x4;
        x5 = x8 - (kW1 + kW7) * x5;
        x8 = kW3 * (x6 + x7);
        x6 = x8 - (kW3 - kW5) * x6;
        x7 = x8 - (kW3 + kW5) * x7;

        // second stage
        x8 = x0 + x1;
        x0 -= x1;
        x1 = kW6 * (x3 + x2);
        x2 = x1 - (kW2 + kW6) * x2;
        x3 = x1 + (kW2 - kW6) * x3;
        x1 = x4 + x6;
        x4 -= x6;
        x6 = x5 + x7;
        x5 -= x7;

        // third stage
        x7 = x8 + x3;
        x8 -= x3;
        x3 = x0 + x2;
        x0 -= x2;
        x2 = (181 * (x4 + x5) + 128) >> 8; // (256/sqrt(2) * <clip>) / 256
        x4 = (181 * (x4 - x5) + 128) >> 8; // (256/sqrt(2) * <clip>) / 256

        // fourth stage
        blk[0] = (x7 + x1) >> 8;
        blk[1] = (x3 + x2) >> 8;
        blk[2] = (x0 + x4) >> 8;
        blk[3] = (x8 + x6) >> 8;
        blk[4] = (x8 - x6) >> 8;
        blk[5] = (x0 - x4) >> 8;
        blk[6] = (x3 - x2) >> 8;
        blk[7] = (x7 - x1) >> 8;
    }

    // column (vertical) IDCT
    //
    //             7                         pi         i
    // dst[8*k] = sum c[i] * src[8*i] * cos( -- * ( k + - ) * i )
    //            i=0                        8          2
    // where: c[0]    = 1/1024
    //        c[1..7] = (1/1024)*sqrt(2)
    template <bool ClippingTable>
    void columnIDCT(const int *U_RESTRICT blk, unsigned char *U_RESTRICT out, int stride) {
        int x0, x1, x2, x3, x4, x5, x6, x7, x8;
        // if all AC components are zero, then IDCT is trival.
        if (!((x1 = sls(blk[8*4], 8))
            | (x2 = blk[8*6]) | (x3 = blk[8*2])
            | (x4 = blk[8*1]) | (x5 = blk[8*7])
            | (x6 = blk[8*5]) | (x7 = blk[8*3])))
        {
            // short circuit the 1D DCT if the DC component is non-zero
            x1 = clip(((blk[0] + 32) >> 6) + 128); // descale
            for (x0 = 8; x0; --x0) {
                *out = (unsigned char)x1;
                out += stride;
            }
            return;
        }
        // for proper rounding in fourth stage
        x0 = sls(blk[0], 8) + 8192;

        // first stage (prescale)
        x8 = kW7 * (x4 + x5) + 4;
        x4 = (x8 + (kW1 - kW7) * x4) >> 3;
        x5 = (x8 - (kW1 + kW7) * x5) >> 3;
        x8 = kW3 * (x6 + x7) + 4;
        x6 = (x8 - (kW3 - kW5) * x6) >> 3;
        x7 = (x8 - (kW3 + kW5) * x7) >> 3;

        // second stage
        x8 = x0 + x1;
        x0 -= x1;
        x1 = kW6 * (x3 + x2) + 4;
        x2 = (x1 - (kW2 + kW6) * x2) >> 3;
        x3 = (x1 + (kW2 - kW6) * x3) >> 3;
        x1 = x4 + x6;
        x4 -= x6;
        x6 = x5 + x7;
        x5 -= x7;

        // third stage
        x7 = x8 + x3;
        x8 -= x3;
        x3 = x0 + x2;
        x0 -= x2;
        x2 = (181 * (x4 + x5) + 128) >> 8; // (256/sqrt(2) * <clip>) / 256
        x4 = (181 * (x4 - x5) + 128) >> 8; // (256/sqrt(2) * <clip>) / 256

        // forth stage
        if (ClippingTable) {
            *out = gIDCTClippingTable((x7 + x1) >> 14); out += stride;
            *out = gIDCTClippingTable((x3 + x2) >> 14); out += stride;
            *out = gIDCTClippingTable((x0 + x4) >> 14); out += stride;
            *out = gIDCTClippingTable((x8 + x6) >> 14); out += stride;
            *out = gIDCTClippingTable((x8 - x6) >> 14); out += stride;
            *out = gIDCTClippingTable((x0 - x4) >> 14); out += stride;
            *out = gIDCTClippingTable((x3 - x2) >> 14); out += stride;
            *out = gIDCTClippingTable((x7 - x1) >> 14);
        } else {
            *out = clip(((x7 + x1) >> 14) + 128); out += stride;
            *out = clip(((x3 + x2) >> 14) + 128); out += stride;
            *out = clip(((x0 + x4) >> 14) + 128); out += stride;
            *out = clip(((x8 + x6) >> 14) + 128); out += stride;
            *out = clip(((x8 - x6) >> 14) + 128); out += stride;
            *out = clip(((x0 - x4) >> 14) + 128); out += stride;
            *out = clip(((x3 - x2) >> 14) + 128); out += stride;
            *out = clip(((x7 - x1) >> 14) + 128);
        }
    }

    int viewBits(int bits) {
        unsigned char newbyte;
        if (!bits)
            return 0;
        while (m_bufbits < bits) {
            if (m_size <= 0) {
                m_buf = sls(m_buf, 8) | 0xFF;
                m_bufbits += 8;
                continue;
            }
            newbyte = *m_position++;
            m_size--;
            m_bufbits += 8;
            m_buf = sls(m_buf, 8) | newbyte;
            if (newbyte == 0xFF) {
                if (m_size) {
                    const unsigned char marker = *m_position++;
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
                            m_buf = sls(m_buf, 8) | marker;
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
            viewBits(bits);
        m_bufbits -= bits;
    }

    int getBits(int bits) {
        int res = viewBits(bits);
        skipBits(bits);
        return res;
    }

    void alignBits() {
        m_bufbits &= 0xF8;
    }

    void skip(int count) {
        m_position += count;
        m_size -= count;
        m_length -= count;
        if (m_size < 0)
            m_error = kMalformatted;
    }

    uint16_t decode16(const unsigned char *m_position) {
        return (m_position[0] << 8) | m_position[1];
    }

    void decodeLength() {
        if (m_size < 2)
            returnResult(kMalformatted);
        m_length = decode16(m_position);
        if (m_length > m_size)
            returnResult(kMalformatted);
        skip(2);
    }

    void skipMarker() {
        decodeLength();
        skip(m_length);
    }

    void decodeSOF() {
        int ssxmax = 0;
        int ssymax = 0;
        component *c;

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
        U_ASSERT(ssxmax);
        U_ASSERT(ssymax);
        m_mbsizex = ssxmax << 3;
        m_mbsizey = ssymax << 3;
        U_ASSERT(m_mbsizex);
        U_ASSERT(m_mbsizey);
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

    void decodeDHT() {
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
                    unsigned char code = m_position[i];
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

    void decodeDQT() {
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

    void decodeDRI() {
        decodeLength();
        if (m_length < 2)
            returnResult(kMalformatted);
        m_rstinterval = decode16(m_position);
        skip(m_length);
    }

    int getCoding(const vlcCode *const vlc, unsigned char *const code) {
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
            value += sls(-1, bits) + 1;
        return value;
    }

    template <bool ClippingTable>
    void decodeBlock(component *const c, unsigned char *const out) {
        unsigned char code = 0;
        int coef = 0;
        memset(m_block, 0, sizeof m_block);
        c->dcpred += getCoding(&m_vlctab[c->dctabsel][0], NULL);
        m_block[0] = (c->dcpred) * m_qtab[c->qtsel][0];
        do {
            const int value = getCoding(&m_vlctab[c->actabsel][0], &code);
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
            columnIDCT<ClippingTable>(&m_block[coef], &out[coef], c->stride);
    }

    template <bool ClippingTable>
    void decodeScanlines() {
        size_t i;
        size_t rstcount = m_rstinterval;
        size_t nextrst = 0;
        component *c;
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
                            decodeBlock<ClippingTable>(c, &c->pixels[((mby * c->ssy + sby) * c->stride + mbx * c->ssx + sbx) << 3]);
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
    uint16_t exifRead16(const unsigned char *const data) {
        if (m_exifLittleEndian)
            return data[0] | (data[1] << 8);
        return (data[0] << 8) | data[1];
    }

    uint32_t exifRead32(const unsigned char *const data) {
        if (m_exifLittleEndian)
            return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }

    void decodeExif() {
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
    void upSampleCenteredH(component *const c) {
        const size_t xmax = c->width - 3;
        u::vector<unsigned char> out((c->width * c->height) << 1);
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

    void upSampleCenteredV(component *const c) {
        const size_t w = c->width;
        const size_t s1 = c->stride;
        const size_t s2 = s1 + s1;

        u::vector<unsigned char> out((c->width * c->height) << 1);

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

    void upSampleCositedH(component *const c) {
        const size_t xmax = c->width - 1;
        u::vector<unsigned char> out((c->width * c->height) << 1);

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

    void upSampleCositedV(component *const c) {
        const size_t w = c->width;
        const size_t s1 = c->stride;
        const size_t s2 = s1 + s1;

        u::vector<unsigned char> out((c->width * c->height) << 1);

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
    void upSampleFast(component *const c) {
        size_t xshift = 0;
        size_t yshift = 0;

        while (c->width < m_width) c->width <<= 1, ++xshift;
        while (c->height < m_height) c->height <<= 1, ++yshift;

        u::vector<unsigned char> out(c->width * c->height);

        unsigned char *lout = &out[0];
        for (size_t y = 0; y < c->height; ++y) {
            const unsigned char *lin = &c->pixels[(y >> yshift) * c->stride];
            for (size_t x = 0; x < c->width; ++x)
                lout[x] = lin[x >> xshift];
            lout += c->width;
        }

        c->stride = c->width;
        c->pixels = u::move(out);
    }

    void convert(chromaFilter filter) {
        size_t i;
        component *c;
        switch (filter) {
        case kBicubic:
            for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
                bool w = c->width < m_width;
                bool h = c->height < m_height;
                if (m_coSitedChroma) {
                    while (w || h) {
                        if (w) upSampleCositedH(c);
                        if (h) upSampleCositedV(c);
                        w = c->width < m_width;
                        h = c->height < m_height;
                    }
                } else {
                    while (w || h) {
                        if (w) upSampleCenteredH(c);
                        if (h) upSampleCenteredV(c);
                        w = c->width < m_width;
                        h = c->height < m_height;
                    }
                }
                if (c->width < m_width || c->height < m_height)
                    returnResult(kInternalError);
            }
            break;
        case kPixelRepetition:
            for (i = 0, c = m_comp; i < m_bpp; ++i, ++c) {
                if (c->width < m_width || c->height < m_height)
                    upSampleFast(c);
                if (c->width < m_width || c->height < m_height)
                    returnResult(kInternalError);
            }
            break;
        default:
            U_ASSERT(0 && "unknown chroma filter");
            break;
        }

        if (m_bpp == 3) {
            // convert to RGB24
            unsigned char *prgb = &m_rgb[0];
            const unsigned char *py  = &m_comp[0].pixels[0];
            const unsigned char *pcb = &m_comp[1].pixels[0];
            const unsigned char *pcr = &m_comp[2].pixels[0];
            for (size_t yy = m_height; yy; --yy) {
                for (size_t x = 0; x < m_width; ++x) {
                    const int y = py[x] << 8;
                    const int cb = pcb[x] - 128;
                    const int cr = pcr[x] - 128;
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
            const unsigned char *pin = &m_comp[0].pixels[0] + m_comp[0].stride;
            unsigned char *pout = &m_comp[0].pixels[0] + m_comp[0].width;
            for (int y = m_comp[0].height - 1; y; --y) {
                memcpy(pout, pin, m_comp[0].width);
                pin += m_comp[0].stride;
                pout += m_comp[0].width;
            }
            m_comp[0].stride = m_comp[0].width;
        }
    }

    template <bool ClippingTable>
    result decode(const u::vector<unsigned char> &data, chromaFilter filter) {
        m_position = (const unsigned char*)&data[0];
        m_size = data.size() & 0x7FFFFFFF;

        if (m_size < 2)
            return kInvalid;
        if (memcmp(m_position, kMagic, 3))
            return kInvalid;

        skip(2);
        while (!m_error) {
            if (m_size < 2 || m_position[0] != 0xFF)
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
                decodeScanlines<ClippingTable>();
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

jpeg::IDCTClippingTable jpeg::gIDCTClippingTable;

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
    static constexpr const char *kMagic = "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A";

    static bool test(const u::vector<unsigned char> &data) {
        return memcmp(&data[0], kMagic, 8) == 0;
    }

    png(const u::vector<unsigned char> &data) {
        decode(m_decoded, data);

        switch (m_bpp) {
        case 1:
            m_format = kTexFormatLuminance;
            break;
        case 3:
            m_format = kTexFormatRGB;
            break;
        case 4:
            m_format = kTexFormatRGBA;
            break;
        default:
            U_UNREACHABLE();
            break;
        }

        m_pitch = m_bpp * m_width;
    }

    u::vector<unsigned char> data() {
        return u::move(m_decoded);
    }

private:
    friend struct texture;

    void decode(u::vector<unsigned char> &out, const u::vector<unsigned char> &invec) {
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
        if (!u::zlib::decompress(scanlines, idat))
            returnResult(kMalformatted);

        const size_t bytewidth = (bpp + 7) / 8;
        const size_t outlength = (m_height * m_width * bpp + 7) / 8;

        out.resize(outlength);
        unsigned char *const out_ = outlength ? &out[0] : 0;

        // no interlace, just filter
        if (m_interlaceMethod == 0) {
            size_t linestart = 0;
            const size_t linelength = (m_width * bpp + 7) / 8;
            if (bpp >= 8) {
                for (size_t y = 0; y < m_height; y++) {
                    const size_t filterType = scanlines[linestart];
                    const unsigned char *const prevline = (y == 0)
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
                    const unsigned char *const prevline = (y == 0) ? 0 : &out_[(y - 1) * m_width * bytewidth];
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
            size_t passw[7], *pw = passw;
            size_t passh[7], *ph = passh;
            size_t passs[7], *ps = passs;
            for (const char *x = "\x7\x8\x3\x8\x3\x4\x1\x4\x1\x2\x0\x2\x0\x1"; pw != &passw[7]; x+=2)
                *pw++ = (m_width + x[0]) / x[1];
            for (const char *x = "\x7\x8\x7\x8\x3\x8\x3\x4\x1\x4\x1\x2\x0\x2"; ph != &passh[7]; x+=2)
                *ph++ = (m_height + x[0]) / x[1];
            for (size_t i = 0; i < 6; i++, ps++)
                ps[1] = *ps + passh[i] * ((!!passw[i]) + (passw[i] * bpp + 7) / 8);

            u::vector<unsigned char> scanlineo((m_width * bpp + 7) / 8);
            u::vector<unsigned char> scanlinen((m_width * bpp + 7) / 8);

            for (size_t i = 0; i < 7; i++) {
                adam7Pass(out_, &scanlinen[0], &scanlineo[0],
                    &scanlines[passs[i]], m_width, i, passw[i], passh[i], bpp);
            }
        }
        if (m_colorType == 3 || m_colorType == 4) {
            const u::vector<unsigned char> data = out; // Make a copy to work with
            if (!convert(out, &data[0]))
                returnResult(kInternalError);
        }
    }

    bool convert(u::vector<unsigned char> &out, const unsigned char *in) {
        // This will always produce RGBA32
        const size_t numPixels = m_width * m_height;
        out.resize(numPixels * 4);
        if (m_bitDepth == 8) {
            switch(m_colorType) {
            case 3: // Indexed color
                for (size_t i = 0; i < numPixels; i++) {
                    if (4u * in[i] >= m_palette.size())
                        return false;
                    for (size_t c = 0; c < 4; c++)
                        out[4 * i + c] = m_palette[4 * in[i] + c]; // RGB colors from palette
                }
                break;
            case 4: // Greyscale with alpha
                for (size_t i = 0; i < numPixels; i++) {
                    out[4 * i + 0] = in[2 * i + 0];
                    out[4 * i + 1] = in[2 * i + 0];
                    out[4 * i + 2] = in[2 * i + 0];
                    out[4 * i + 3] = in[2 * i + 1];
                }
                break;
            }
        }
        m_bpp = 4;
        return true;
    }

    void readHeader(const unsigned char* in, size_t inlength) {
        if (inlength < 29)
            returnResult(kInvalid);
        if (memcmp(in, kMagic, 8))
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

    void unfilterScanline(unsigned char *const recon, const unsigned char *const scanline,
        const unsigned char *const precon, size_t bytewidth, size_t filterType, size_t length)
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


    void adam7Pass(unsigned char *out, unsigned char *linen, unsigned char *lineo,
        const unsigned char *const in, size_t w, size_t i, size_t passw, size_t passh, size_t bpp)
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

    size_t readBitReverse(size_t& bitp, const unsigned char *const bits) {
        const size_t r = (bits[bitp >> 3] >> (7 - (bitp & 0x7))) & 1;
        bitp++;
        return r;
    }

    size_t readBitsReverse(size_t &bitp, const unsigned char *const bits, size_t nbits) {
        size_t r = 0;
        for (size_t i = nbits - 1; i < nbits; i--)
            r += ((readBitReverse(bitp, bits)) << i);
        return r;
    }

    void setBitReversed(size_t &bitp, unsigned char *const bits, size_t bit) {
        U_ASSERT(bits);
        bits[bitp >> 3] |=  (bit << (7 - (bitp & 0x7)));
        bitp++;
    }

    uint32_t readWord(const unsigned char *const buffer) {
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

    size_t calculateBitsPerPixel() const {
        if (m_colorType == 2)
            return (3 * m_bitDepth);
        else if (m_colorType >= 4)
            return (m_colorType - 2) * m_bitDepth;
        return m_bitDepth;
    }

    // Paeth predicter
    static unsigned char paethPredictor(short a, short b, short c) {
        const short p = a + b - c;
        const short pa = p > a ? (p - a) : (a - p);
        const short pb = p > b ? (p - b) : (b - p);
        const short pc = p > c ? (p - c) : (c - p);
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
    static constexpr const char *kMagic = "TRUEVISION-XFILE";

    static bool test(const u::vector<unsigned char> &data) {
        // Not pretty but required to prevent more involved test from reading
        // past the end of the buffer.
        if (data.size() <= sizeof(header) || data.size() <= 26)
            return false;
        // The TGA file format has no magic number. It has an `optional' trailer
        // section which could contain kMagic. It's always the last 26 bytes of
        // the file. We test for that as an early optimization, otherwise we have
        // to do a more involved test.
        const unsigned char *const trailer = &data[data.size() - 26]
            + 4  // Skip `extensions offset' field
            + 4; // Skip `developer area offset' field
        if (!memcmp(trailer, kMagic, 16))
            return true;

        // We've made it here, time for a more involved process
        header h;
        memcpy(&h, &data[0], sizeof h);
        // Color type (0, or 1)
        if (h.colorMapType > 1)
            return false;
        // Image type RGB/grey +/- RLE only
        if (!memchr("\x1\x2\x3\xA\xB\xC", h.imageType, 6))
            return false;
        // Verify dimensions
        if ((h.width[0] | (h.width[1] << 8)) == 0)
            return false;
        if ((h.height[0] | (h.height[1] << 8)) == 0)
            return false;
        return true;
    }

    tga(const u::vector<unsigned char> &data) {
        decode(data);

        switch (m_bpp) {
        case 1:
            m_format = kTexFormatLuminance;
            break;
        case 3:
            m_format = kTexFormatRGB;
            break;
        case 4:
            m_format = kTexFormatRGBA;
            break;
        default:
            U_ASSERT(0 && "unsupported bpp");
            break;
        }

        m_pitch = m_bpp * m_width;
    }

    u::vector<unsigned char> data() {
        return u::move(m_data);
    }

private:
    friend struct texture;

    void read(unsigned char *dest, size_t size) {
        memcpy(dest, m_position, size);
        seek(size);
    }

    int get() {
        return *m_position++;
    }

    void seek(size_t where) {
        m_position += where;
    }

    result decode(const u::vector<unsigned char> &data) {
        m_position = &data[0];
        read((unsigned char *)&m_header, sizeof m_header);
        seek(m_header.identSize);

        if (!memchr("\x8\x18\x20", m_header.pixelSize, 3))
            return kUnsupported;

        m_bpp = m_header.pixelSize / 8;
        m_width = m_header.width[0] | (m_header.width[1] << 8);
        m_height = m_header.height[0] | (m_header.height[1] << 8);

        switch (m_header.imageType) {
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

    void decodeColor() {
        const size_t colorMapSize = m_header.colorMapSize[0] | (m_header.colorMapSize[1] << 8);
        if (!memchr("\x8\x18\x20", m_header.colorMapEntrySize, 3))
            returnResult(kUnsupported);

        m_bpp = m_header.colorMapEntrySize / 8;
        u::vector<unsigned char> colorMap(m_bpp * colorMapSize);
        read(&colorMap[0], m_bpp * colorMapSize);

        if (m_bpp >= 3)
            convert(&colorMap[0], m_bpp * colorMapSize, m_bpp);

        m_data.resize(m_bpp * m_width * m_height);
        unsigned char *const indices = &m_data[(m_bpp - 1) * m_width * m_height];
        read(indices, m_width * m_height);
        const unsigned char *src = indices;
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

    void decodeImage() {
        m_data.resize(m_bpp * m_width * m_height);
        unsigned char *dst = &m_data[m_bpp * m_width * m_height];
        for (size_t i = 0; i < m_height; i++) {
            dst -= m_bpp * m_width;
            read(dst, m_bpp * m_width);
        }
        if (m_bpp >= 3)
            convert(&m_data[0], m_bpp * m_width * m_height, m_bpp);
    }

    void decodeColorRLE() {
        const size_t colorMapSize = m_header.colorMapSize[0] | (m_header.colorMapSize[1] << 8);
        if (!memchr("\x8\x18\x20", m_header.colorMapEntrySize, 3))
            returnResult(kUnsupported);

        m_bpp = m_header.colorMapEntrySize / 8;
        u::vector<unsigned char> colorMap(m_bpp * colorMapSize);
        read(&colorMap[0], m_bpp * colorMapSize);

        if (m_bpp >= 3)
            convert(&colorMap[0], m_bpp * colorMapSize, m_bpp);

        m_data.resize(m_bpp * m_width * m_height);

        unsigned char buffer[128];
        for (unsigned char *end = &m_data[m_bpp * m_width * m_height], *dst = end - m_bpp * m_width; dst >= &m_data[0]; ) {
            int c = get();
            if (c & 0x80) {
                const int index = get();
                const unsigned char *column = &colorMap[index * m_bpp];
                c -= 0x7F;
                c *= m_bpp;

                while (c > 0 && dst >= &m_data[0]) {
                    const int n = u::min(c, int(end - dst));
                    for (const unsigned char *const run = dst + n; dst < run; dst += m_bpp)
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
                    const int n = u::min(c, int(end - dst) / int(m_bpp));
                    read(buffer, n);
                    for (const unsigned char *src = buffer; src <= &buffer[n]; dst += m_bpp)
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

    void decodeImageRLE() {
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
                    const int n = u::min(c, int(end - dst));
                    for (const unsigned char *const run = dst + n; dst < run; dst += m_bpp)
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
                    const int n = u::min(c, int(end - dst));
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
        for (const unsigned char *const end = &data[length]; data < end; data += m_bpp)
            u::swap(data[0], data[2]);
    }

    struct header {
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
    } m_header;

    const unsigned char *m_position;
    u::vector<unsigned char> m_data;
};

struct dds : decoder {
    static bool test(const u::vector<unsigned char> &data) {
        return !memcmp(&data[0], (const void *)"DDS ", 4);
    }

    dds(const u::vector<unsigned char> &data) {
        decode(data);
    }

    u::vector<unsigned char> data() {
        return u::move(m_data);
    }

    static constexpr uint32_t kDXT1 = u::fourCC<uint32_t>("DXT1");
    static constexpr uint32_t kDXT3 = u::fourCC<uint32_t>("DXT3");
    static constexpr uint32_t kDXT5 = u::fourCC<uint32_t>("DXT5");
    static constexpr uint32_t kBC4U = u::fourCC<uint32_t>("BC4U");
    static constexpr uint32_t kBC4S = u::fourCC<uint32_t>("BC4S");
    static constexpr uint32_t kBC5U = u::fourCC<uint32_t>("BC5U");
    static constexpr uint32_t kBC5S = u::fourCC<uint32_t>("BC5S");
    static constexpr uint32_t kATI1 = u::fourCC<uint32_t>("ATI1");
    static constexpr uint32_t kATI2 = u::fourCC<uint32_t>("ATI2");

private:
    void read(unsigned char *dest, size_t size) {
        memcpy(dest, m_position, size);
        m_position += size;
    }

    void decode(const u::vector<unsigned char> &data) {
        m_position = &data[0];

        unsigned char magic[4];
        read(magic, 4);

        if (memcmp((const void*)magic, (const void*)"DDS ", 4))
            returnResult(kInvalid);

        surfaceDescriptor surface;
        read((unsigned char*)&surface, sizeof surface);

        m_data.resize(data.end() - m_position);
        read(&m_data[0], m_data.size());

        switch (surface.format.fourcc) {
        case kDXT1: m_format = kTexFormatDXT1; break;
        case kDXT3: m_format = kTexFormatDXT3; break;
        case kDXT5: m_format = kTexFormatDXT5; break;
        case kATI1: m_format = kTexFormatBC4U; break;
        case kATI2: m_format = kTexFormatBC5U; break;
        case kBC4U: m_format = kTexFormatBC4U; break;
        case kBC4S: m_format = kTexFormatBC4S; break;
        case kBC5U: m_format = kTexFormatBC5U; break;
        case kBC5S: m_format = kTexFormatBC5S; break;
        default:
            returnResult(kUnsupported);
        }

        m_width = surface.width;
        m_height = surface.height;
        m_mips = surface.mipLevel;
        m_bpp = surface.pitch / surface.width;
    }

    struct pixelFormat {
        uint32_t size;
        uint32_t flags;
        uint32_t fourcc;
        uint32_t bpp;
        uint32_t colorMask[4];
    };

    struct surfaceDescriptor {
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitch;
        uint32_t depth;
        uint32_t mipLevel;
        uint32_t alphaBitDepth;
        uint32_t reserved;
        uint32_t surface;
        uint32_t dstOverlay[2];
        uint32_t dstBlit[2];
        uint32_t srcOverlay[2];
        uint32_t srcBlit[2];
        pixelFormat format;
        uint32_t caps[4];
        uint32_t textureStage;
    };

    const unsigned char *m_position;
    u::vector<unsigned char> m_data;
};

///
/// NetPBM
///
/// Supports all of PBM except P7 (PAM)
///
struct pnm : decoder {
    static bool test(const u::vector<unsigned char> &data) {
        return data[0] == 'P' && data[1] >= '1' && data[1] <= '6' && u::isspace(data[2]);
    }

    pnm(const u::vector<unsigned char> &data) {
        decode(data);
    }

    u::vector<unsigned char> data() {
        return u::move(m_data);
    }

protected:
    void decode(const u::vector<unsigned char> &data) {
        // No line can be longer than 70 characters in PBM files
        unsigned char buffer[70];
        const char *string = (const char *)buffer;
        m_buffer = &data[3]; // Skip "P[1...6]"
        m_end = &data[data.size()];

        #define checkRange() \
            do { \
                if (ptr > &m_data[m_data.size()]) \
                    returnResult(kMalformatted); \
            } while (0)

        // Read size
        if (!next(buffer, sizeof buffer))
            returnResult(kMalformatted);
        m_width = strtol(string, nullptr, 10);
        if (!next(buffer, sizeof buffer))
            returnResult(kMalformatted);
        m_height = strtol(string, nullptr, 10);

        // Don't allow files larger than 4096*4096
        if (m_width > 4096)
            returnResult(kUnsupported);
        if (m_height > 4096)
            returnResult(kUnsupported);

        // Calculate scaling factor
        int max = 0;
        float scale = 0.0f;
        if (data[1] != '1' && data[1] != '4') {
            if (!next(buffer, sizeof buffer))
                returnResult(kMalformatted);
            max = strtol(string, nullptr, 10);
            if (max < 1 || max > 65535)
                returnResult(kMalformatted);
            scale = 255.0f / float(max);
        }

        auto readBigEndian16 = [](unsigned char *data) {
            return size_t(data[1]) | size_t(data[0]) << 8;
        };

        const size_t size = m_width * m_height;
        m_bpp = (data[1] == '3' || data[1] == '6') ? 3 : 1;
        m_data.resize(size * m_bpp);
        unsigned char *ptr = &m_data[0];
        if (data[1] >= '1' && data[1] <= '4') {
            for (size_t j = 0; j < size; ++j) {
                switch (data[1]) {
                case '1':
                    if (!next(buffer, sizeof buffer))
                        returnResult(kMalformatted);
                    *ptr++ = buffer[0] == '1' ? 255 : 0;
                    checkRange();
                    break;
                case '2':
                    if (!next(buffer, sizeof buffer))
                        returnResult(kMalformatted);
                    *ptr++ = strtol(string, nullptr, 10) * scale;
                    checkRange();
                    break;
                case '3':
                    for (size_t c = 0; c < 3; c++) {
                        if (!next(buffer, sizeof buffer))
                            returnResult(kMalformatted);
                        *ptr++ = strtol(string, nullptr, 10) * scale;
                        checkRange();
                    }
                    break;
                case '4':
                    if (read(buffer, 1) != 1)
                        returnResult(kMalformatted);
                    for (size_t i = 0; i < 8 && j < size; ++i, ++j) {
                        *ptr++ = (*buffer & (1<<(7-i))) ? 255 : 0;
                        checkRange();
                    }
                    break;
                }
            }
        } else {
            switch (data[1]) {
            case '5':
                if (max > 255) {
                    for (size_t j = 0; j < size; ++j) {
                        if (read(buffer, 2) != 2)
                            returnResult(kMalformatted);
                        *ptr++ = readBigEndian16(buffer) * scale;
                        checkRange();
                    }
                } else {
                    if (!read(ptr, size))
                        returnResult(kMalformatted);
                    for (size_t j = 0; j < size; ++j) {
                        *ptr++ *= scale;
                        checkRange();
                    }
                }
                break;
            case '6':
                if (max > 255) {
                    for (size_t j = 0; j < size; ++j) {
                        for (size_t c = 0; c < 3; c++) {
                            if (read(buffer, 2) != 2)
                                returnResult(kMalformatted);
                            *ptr++ = readBigEndian16(buffer) * scale;
                            checkRange();
                        }
                    }
                } else {
                    if (read(ptr, size*3) != int(size*3))
                        returnResult(kMalformatted);
                    for (size_t j = 0; j < size*3; ++j) {
                        *ptr++ *= scale;
                        checkRange();
                    }
                }
                break;
            default:
                returnResult(kUnsupported);
                break;
            }
        }

        // The pointer should be at the end of memory if everything decoded
        // successfully
        if (ptr != &m_data[m_data.size()])
            returnResult(kMalformatted);

        // Figure out the appropriate format
        switch (m_bpp) {
        case 1:
            m_format = kTexFormatLuminance;
            break;
        case 3:
            m_format = kTexFormatRGB;
            break;
        default:
            U_ASSERT(0 && "unsupported bpp");
            break;
        }

        m_pitch = m_bpp * m_width;

        returnResult(kSuccess);
    }

    int read(unsigned char *const buffer, size_t count) {
        if (m_buffer >= m_end)
            return EOF;
        if (m_buffer + count >= m_end)
            count = m_end - m_buffer;
        memcpy(buffer, m_buffer, count);
        m_buffer += count;
        return int(count);
    }

    bool next(unsigned char *const store, size_t size) {
        for (int c=0;;) {
            // Skip space characters
            while ((c = read(store, 1)) != EOF && u::isspace(*store))
                if (c != 1)
                    return false;
            // Skip comment line
            if (c == '#') {
                while ((c = read(store, 1)) != EOF && strchr("\r\n", *store))
                    if (c != 1)
                        return false;
            } else {
                // Read characters until we hit a space
                size_t i;
                for (i = 1; i < size && (c = read(store+i, 1)) != EOF && !u::isspace(store[i]); ++i)
                    if (c != 1)
                        return false;
                store[i < size ? i : size - 1] = '\0';
                return true;
            }
        }
        return false;
    }
private:
    u::vector<unsigned char> m_data;
    const unsigned char *m_buffer;
    const unsigned char *m_end;
};

/// Trivial PCX image loader (only supports 8-bit indexed or 24-bit RLE RGB)
/// (The only sane formats of PCX for this day and age.)
struct pcx : decoder {
    pcx(const u::vector<unsigned char> &data)
        : m_uncompressed(true)
        , m_planes(0)
        , m_bpl(0)
    {
        decode(data);
    }

    static bool test(const u::vector<unsigned char> &data) {
        if (data[0] != 0xA) // PCX ID Byte: 0xA
            return false;
        if (data[1] > 5)    // Only supports version <= 5
            return false;
        if (data[2] > 1)    // RLE (0 or 1)
            return false;
        // Supported Bytes Per Pixel
        if (!strchr("\x1\x2\x4\x8", data[3]))
            return false;
        return true;
    }

    void decode(const u::vector<unsigned char> &data) {
        // Read the header
        m_uncompressed = !data[2];
        m_bpp = data[3];

        // We only support 8-bit indexed and 24-bit RGB
        if (m_bpp != 8)
            returnResult(kUnsupported);
        // We only know about RLE compressed PCX
        if (m_uncompressed)
            returnResult(kUnsupported);

        // Read image window
        uint16_t x1, y1;
        uint16_t x2, y2;
        memcpy(&x1, &data[4], sizeof x1);
        memcpy(&y1, &data[6], sizeof y1);
        memcpy(&x2, &data[8], sizeof x2);
        memcpy(&y2, &data[10], sizeof y2);

        const int w = (x2 - x1) + 1;
        const int h = (y2 - y1) + 1;

        if (w < 1 || h < 1)
            returnResult(kMalformatted);
        // Sanity check
        if (w > 4096 || h > 4096)
            returnResult(kInvalid);

        m_width = w;
        m_height = h;
        m_planes = data[65];
        if (!strchr("\x1\x3", m_planes))
            returnResult(kUnsupported);

        // Bits per line
        memcpy(&m_bpl, &data[66], 2);

        if (!loadRLE(&data[128]))
            returnResult(kMalformatted);
    }

    u::vector<unsigned char> data() {
        return u::move(m_data);
    }
protected:
    bool loadRLE(const unsigned char *const data) {
        const unsigned char *current = data;
        size_t bufferLineLength = m_bpl*m_planes;
        size_t imageLineLength = m_width*m_planes;
        u::vector<unsigned char> line(bufferLineLength);

        m_data.resize((imageLineLength * m_height)+1);
        for (size_t lineCount = 0; lineCount < m_height; ++lineCount) {
            size_t linePosition = 0;
            while (linePosition < bufferLineLength) {
                // First 2 bits indicate run of next byte value
                if (*current > 0xC0) {
                    // Remaining 6 bits indicate run length
                    int runLength = *current++ & 0x3F;
                    for (; runLength != 0; runLength--)
                        line[linePosition++] = *current;
                    ++current;
                } else {
                    line[linePosition++] = *current++;
                }
            }
            if (m_planes == 1) {
                memcpy(&m_data[lineCount*imageLineLength], &line[0], imageLineLength);
                m_bpp = 1;
                m_format = kTexFormatLuminance;
            } else if (m_planes == 3) {
                unsigned char *interleave = &m_data[lineCount * imageLineLength];
                for (linePosition = 0; linePosition != m_width; ++linePosition, interleave += 3) {
                    interleave[0] = line[linePosition];
                    interleave[1] = line[linePosition + m_width];
                    interleave[2] = line[linePosition + m_width*2];
                }
                m_bpp = 3;
                m_format = kTexFormatRGB;
            } else {
                return false;
            }
        }
        m_pitch = m_bpp*m_width;
        return true;
    }
private:
    u::vector<unsigned char> m_data;
    bool m_uncompressed;
    unsigned char m_planes; // 1 = Indexes, 3 = RGB
    uint16_t m_bpl;
};


///
/// Texture utilities:
///   halve (useful for generating mipmaps), shift, scale and reorient.
///

template <size_t S>
void texture::halve(unsigned char *U_RESTRICT src, size_t sw, size_t sh, size_t stride, unsigned char *U_RESTRICT dst) {
    for (unsigned char *yend = src + sh * stride; src < yend;) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += 2 * S, dst += S) {
            for (size_t i = 0; i < S; i++)
                dst[i] = (size_t(xsrc[i]) + size_t(xsrc[i+S]) + size_t(xsrc[stride+i]) + size_t(xsrc[stride+i+S])) >> 2;
        }
        src += 2 * stride;
    }
}

template <size_t S>
void texture::shift(unsigned char *U_RESTRICT src, size_t sw, size_t sh, size_t stride, unsigned char *U_RESTRICT dst, size_t dw, size_t dh) {
    size_t wfrac = sw/dw,
           hfrac = sh/dh,
           wshift = 0,
           hshift = 0;
    while (dw << wshift < sw) wshift++;
    while (dh << hshift < sh) hshift++;
    const size_t tshift = wshift + hshift;
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
void texture::scale(unsigned char *U_RESTRICT src, size_t sw, size_t sh, size_t stride, unsigned char *U_RESTRICT dst, size_t dw, size_t dh) {
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
    size_t area = size_t((unsigned long long)darea << ascale) / sarea;
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

void texture::scale(unsigned char *U_RESTRICT src, size_t sw, size_t sh, size_t bpp, size_t pitch, unsigned char *U_RESTRICT dst, size_t dw, size_t dh) {
    if (sw == dw * 2 && sh == dh * 2) {
        switch (bpp) {
        case 1: return halve<1>(src, sw, sh, pitch, dst);
        case 2: return halve<2>(src, sw, sh, pitch, dst);
        case 3: return halve<3>(src, sw, sh, pitch, dst);
        case 4: return halve<4>(src, sw, sh, pitch, dst);
        }
    } else if (sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1)) {
        switch (bpp) {
        case 1: return scale<1>(src, sw, sh, pitch, dst, dw, dh);
        case 2: return scale<2>(src, sw, sh, pitch, dst, dw, dh);
        case 3: return scale<3>(src, sw, sh, pitch, dst, dw, dh);
        case 4: return scale<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    } else {
        switch (bpp) {
        case 1: return shift<1>(src, sw, sh, pitch, dst, dw, dh);
        case 2: return shift<2>(src, sw, sh, pitch, dst, dw, dh);
        case 3: return shift<3>(src, sw, sh, pitch, dst, dw, dh);
        case 4: return shift<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    }
}

void texture::reorient(unsigned char *U_RESTRICT src, size_t sw, size_t sh, size_t bpp, size_t stride, unsigned char *U_RESTRICT dst, bool flipx, bool flipy, bool swapxy) {
    int stridex = swapxy ? bpp * sh : bpp;
    int stridey = swapxy ? bpp : bpp * sw;
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

void texture::flip(int flags) {
    u::vector<unsigned char> temp(m_data.size());
    reorient(&m_data[0],
             m_width,
             m_height,
             m_bpp,
             m_pitch,
             &temp[0],
             flags & kFlipVertical,
             flags & kFlipHorizontal,
             false);
    m_data = u::move(temp);
}

void texture::drawString(size_t &line, const char *string) {
    // Can't render into compressed textures or normal maps
    if (m_flags & kTexFlagCompressed || m_flags & kTexFlagNormal)
        return;

    u::vector<unsigned char> pixelData;
    size_t c = 0;
    // Avoid using m_bpp in the glyph's intermediate storage so that we
    // don't need to heap allocate it.
    const size_t glyphSize = 8*8*m_bpp;
    unsigned char glyph[8*8*4];
    for (; *string; string++, c++) {
        memset(glyph, 0, glyphSize);
        unsigned char *prev = &glyph[8*8*m_bpp-1];
        uint64_t n = 1;
        for (size_t h = 0; h < 8; h++)
            for (size_t w = 0; w < 8; w++, n += n)
                for (size_t k = 0; k < m_bpp; k++)
                    *prev-- = (kFont[int(*string)] & n) ? 255 : 0;
        const size_t size = pixelData.size();
        pixelData.resize(size + glyphSize);
        memcpy(&pixelData[size], glyph, glyphSize);
    }
    const size_t size = 8*c*8*m_bpp,
                 offset = pixelData.size();
    pixelData.resize(size + offset);
    for (size_t i = 0; i < c; i++) {
        unsigned char *s = &pixelData[8*i*8*m_bpp];
        unsigned char *d = &pixelData[offset]+i*8*m_bpp;
        for (size_t h = 0; h < 8; h++, s+=8*m_bpp, d+=8*c*m_bpp)
            memcpy(d, s, 8*m_bpp);
    }
    for (size_t h = 0; h < 8; h++) {
        unsigned char *s = &pixelData[offset]+8*c*m_bpp*h;
        unsigned char *d = &m_data[0] + 8*m_pitch*line+m_pitch*h;
        // No way to render this information into the texture for there is no
        // available space to do so.
        if (d+8*c*m_bpp >= &m_data[m_data.size()])
            break;
        memcpy(d, s, 8*c*m_bpp);
    }
    line++;
    // Recalculate hash string
    u::djbx33a hash(&m_data[0], m_data.size());
    m_hashString = hash.hex();
}

template <textureFormat F>
void texture::convert() {
    if (F == m_format)
        return;

    if (F != kTexFormatRG && F != kTexFormatLuminance) {
        unsigned char *data = &m_data[0];
        const size_t length = m_data.size();

        // Converts BGR -> RGB or RGB -> BGR, or
        //          BGRA -> RGBA or RGBA -> BGRA
        for (unsigned char *end = &data[length]; data < end; data += m_bpp)
            u::swap(data[0], data[2]);
    } else if (F == kTexFormatRG) {
        // Eliminate blue and alpha
        u::vector<unsigned char> rework;
        rework.reserve(m_width * m_height * 2);
        const size_t roff = (m_format == kTexFormatBGR || m_format == kTexFormatBGRA) ? 2 : 0;
        for (size_t i = 0; i < m_data.size(); i += m_bpp) {
            rework.push_back(m_data[i + roff]); // R
            rework.push_back(m_data[i + 1]); // G
        }
        // Eliminate the memory of the original texture and swap in the new one
        m_data.destroy();
        m_data.swap(rework);
        m_bpp = 2;
        m_pitch = m_width * m_bpp;
    } else if (F == kTexFormatLuminance) {
        u::vector<unsigned char> rework;
        rework.reserve(m_width * m_height);
        // weight sum can be calculated as follows
        //  0.299 * R + 0.587 * G + 0.114 * B + 0.5
        // convert this to integer domain
        //  (2 * R + 5 * G + 1 * B) / 8
        // then use the following bitwise calculation
        // (unsigned char)(((R << 1) + ((G << 2) + G) + B) >> 3)
        if (m_format == kTexFormatBGR || m_format == kTexFormatBGRA) {
            // Format is BGR[A]
            for (size_t i = 0; i < m_data.size(); i+= m_bpp) {
                const unsigned char R = m_data[i + 2];
                const unsigned char G = m_data[i + 1];
                const unsigned char B = m_data[i];
                rework.push_back((unsigned char)((((R << 1) + ((G << 2) + G) + B)) >> 3));
            }
        } else {
            // Format is RGB[A]
            for (size_t i = 0; i < m_data.size(); i+= m_bpp) {
                const unsigned char R = m_data[i];
                const unsigned char G = m_data[i + 1];
                const unsigned char B = m_data[i + 2];
                rework.push_back((unsigned char)((((R << 1) + ((G << 2) + G) + B)) >> 3));
            }
        }
        m_data.destroy();
        m_data.swap(rework);
        m_bpp = 1;
        m_pitch = m_width;
    }

    // Update the format
    m_format = F;
}

template void texture::convert<kTexFormatRG>();
template void texture::convert<kTexFormatRGB>();
template void texture::convert<kTexFormatRGBA>();
template void texture::convert<kTexFormatLuminance>();

static inline float textureQualityScale(float quality) {
    // Add 0.5 to the mantissa then zero it. If the mantissa overflows, let
    // the carry bit carry into the exponent; thus increasing the exponent.
    // If the carry happens then the value is rounded up, otherwise it rounds
    // down. This also works +/- INF as well since INF has zero mantissa.
    m::floatShape u = { quality };
    u.asInt += 0x00400000u;
    u.asInt &= 0xFF800000u;
    return u.asFloat;
}

template <typename T>
bool texture::decode(const u::vector<unsigned char> &data, const char *name, float quality) {
    auto decode = u::unique_ptr<T>(new T(data));
    if (decode->status() != decoder::kSuccess) {
        u::print("failed to decode `%s' %s\n", name, decode->error());
        return false;
    }

    m_format = decode->format();
    m_width = decode->width();
    m_height = decode->height();
    m_pitch = decode->pitch();
    m_bpp = decode->bpp();
    m_mips = decode->mips();
    m_data = u::move(decode->data());
    m_flags |= kTexFlagDisk;
    if (u::is_same<T, dds>::value) {
        // Don't allow any post-load processes take place on DDS textures
        m_flags |= kTexFlagCompressed;
    } else {
        // Quality will resize the texture accordingly
        if (quality != 1.0f) {
            const float f = textureQualityScale(quality);
            size_t w = m_width * f;
            size_t h = m_height * f;
            if (w == 0) w = 1;
            if (h == 0) h = 1;
            resize(w, h);
        }

        // Premultiply alpha
        if ((m_flags & kTexFlagPremul) && (m_format == kTexFormatRGBA || m_format == kTexFormatBGRA)) {
            u::vector<unsigned char> rework;
            rework.reserve(m_width * m_height * m_bpp);
            for (size_t i = 0; i < m_data.size(); i += m_bpp) {
                const unsigned char A = m_data[i + 3];
                rework.push_back(m_data[i + 0] * A / 255);
                rework.push_back(m_data[i + 1] * A / 255);
                rework.push_back(m_data[i + 2] * A / 255);
                rework.push_back(A);
            }
            m_data.destroy();
            m_data.swap(rework);
        }

        // Hash the contents as well to generate a hash string
        u::djbx33a hash(&m_data[0], m_data.size());
        m_hashString = hash.hex();
    }
    return true;
}

void texture::colorize(uint32_t color) {
    const uint8_t alpha = color & 0xFF;
    auto blend = [&alpha](uint32_t a, uint32_t b) {
        const uint32_t rb1 = ((0x100 - alpha) * (a & 0xFF00FF)) >> 8;
        const uint32_t rb2 = (alpha * (b & 0xFF00FF)) >> 8;
        const uint32_t g1 = ((0x100 - alpha) * (a & 0x00FF00)) >> 8;
        const uint32_t g2 = (alpha * (b & 0x00FF00)) >> 8;
        return ((rb1 | rb2) & 0xFF00FF) + ((g1 | g2) & 0x00FF00);
    };

    U_ASSERT(m_format == kTexFormatBGR || m_format == kTexFormatBGRA ||
              m_format == kTexFormatRGB || m_format == kTexFormatRGBA);

    for (size_t i = 0; i < m_data.size(); i += m_bpp) {
        uint8_t *R = nullptr;
        uint8_t *B = nullptr;
        uint8_t *G = &m_data[i+1];
        if (m_format == kTexFormatBGR || m_format == kTexFormatBGRA) {
            R = &m_data[i + 2];
            B = &m_data[i + 0];
        } else {
            R = &m_data[i + 0];
            B = &m_data[i + 2];
        }
        // input color is RGBA, we need RGB int
        const uint8_t R_ = color >> 24;
        const uint8_t G_ = color >> 16;
        const uint8_t B_ = color >> 8;
        const uint32_t RGB = blend((*R << 16) | (*G << 8) | *B, (R_ << 16) | (G_ << 8) | B_);
        *R = (RGB >> 16) & 0xFF;
        *G = (RGB >> 8) & 0xFF;
        *B = RGB & 0xFF;
    }

    // Hash the contents as well to generate a hash string
    u::djbx33a hash(&m_data[0], m_data.size());
    m_hashString = hash.hex();
}

u::optional<u::string> texture::find(const u::string &infile) {
    static struct tag {
        const char *name;
        int flag;
    } tags[] = {
        { "normal", kTexFlagNormal },
        { "grey", kTexFlagGrey },
        { "premul", kTexFlagPremul },
        { "nocompress", kTexFlagNoCompress }
    };

    u::string file = infile;

    for (auto beg = file.find('<'); beg != u::string::npos; beg = file.find('<')) {
        auto end = file.find('>');
        if (end == u::string::npos)
            return u::none;
        const size_t length = end - beg - 1;
        for (const auto &it : tags)
            if (!strncmp(&file[beg+1], it.name, length))
                m_flags |= it.flag;
        file.erase(beg, end+1);
    }

    static const char *kExtensions[] = {
        "png", "jpg", "tga", "dds", "pbm", "ppm", "pgm", "pnm", "pcx"
    };
    size_t bits = 0;
    for (size_t i = 0; i < sizeof kExtensions / sizeof *kExtensions; i++)
        if (u::exists(u::format("%s.%s", file, kExtensions[i])))
            bits |= (1 << i);
    if (bits == 0)
        return u::none;
    size_t index = 0;
    while (!(bits & 1)) {
        bits >>= 1;
        ++index;
    }
    return u::format("%s.%s", file, kExtensions[index]);
}

bool texture::load(const u::string &file, float quality) {
    // Construct a texture from a file
    auto name = find(neoGamePath() + file);
    if (!name)
        return false;

    const char *fileName = (*name).c_str();
    auto load = u::read(fileName, "rb");
    if (!load)
        return false;
    u::vector<unsigned char> &data = *load;
    if (jpeg::test(data))
        return decode<jpeg>(data, fileName, quality);
    else if (png::test(data))
        return decode<png>(data, fileName, quality);
    else if (tga::test(data))
        return decode<tga>(data, fileName, quality);
    else if (dds::test(data))
        return decode<dds>(data, fileName, quality);
    else if (pnm::test(data))
        return decode<pnm>(data, fileName, quality);
    else if (pcx::test(data))
        return decode<pcx>(data, fileName, quality);
    u::print("no decoder found for `%s'\n", fileName);
    return false;
}

texture::texture(const unsigned char *const data, size_t length, size_t width,
    size_t height, bool normal, textureFormat format, size_t mips)
    : m_width(width)
    , m_height(height)
    , m_mips(mips)
    , m_flags(normal ? kTexFlagNormal : 0)
    , m_format(format)
{
    m_data.resize(length);
    memcpy(&m_data[0], data, length);
    switch (format) {
    case kTexFormatRGB:
    case kTexFormatBGR:
        m_bpp = 3;
        break;
    case kTexFormatRGBA:
    case kTexFormatBGRA:
        m_bpp = 4;
        break;
    case kTexFormatRG:
        m_bpp = 2;
        break;
    case kTexFormatLuminance:
        m_bpp = 1;
        break;
    default:
        // m_bpp and m_pitch get assigned zero here since compressed textures
        // don't really have them. If a compressed texture is operated on
        // as if it were not compressed, multiplications by m_bpp or m_pitch
        // will result in values of zero which will prevent those paths from
        // running.
        m_bpp = 0;
        m_pitch = 0;
        return;
    }
    m_pitch = m_width * m_bpp;
}

texture::texture(texture &&other)
    : m_hashString(u::move(other.m_hashString))
    , m_data(u::move(other.m_data))
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_bpp(other.m_bpp)
    , m_pitch(other.m_pitch)
    , m_mips(other.m_mips)
    , m_flags(other.m_flags)
    , m_format(other.m_format)
{
}

texture::texture(const texture &other)
    : m_hashString(other.m_hashString)
    , m_data(other.m_data)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_bpp(other.m_bpp)
    , m_pitch(other.m_pitch)
    , m_mips(other.m_mips)
    , m_flags(other.m_flags)
    , m_format(other.m_format)
{
}

texture &texture::operator=(texture &&other) {
    m_hashString = u::move(other.m_hashString);
    m_data = u::move(other.m_data);
    m_width = other.m_width;
    m_height = other.m_height;
    m_bpp = other.m_bpp;
    m_pitch = other.m_pitch;
    m_mips = other.m_mips;
    m_flags = other.m_flags;
    m_format = other.m_format;
    return *this;
}

void texture::writeTGA(u::vector<unsigned char> &outData) {
    tga::header hdr;
    memset(&hdr, 0, sizeof hdr);
    outData.resize(sizeof hdr);
    hdr.pixelSize = m_bpp*8;
    hdr.width[0] = m_width & 0xFF;
    hdr.width[1] = (m_width >> 8) & 0xFF;
    hdr.height[0] = m_height & 0xFF;
    hdr.height[1] = (m_height >> 8) & 0xFF;
    hdr.imageType = tex_tga_compress ? 10 : 2;
    memcpy(&outData[0], &hdr, sizeof hdr);
    outData.reserve(outData.size() * m_width*m_height*3);

    unsigned char buffer[128*4];
    for (size_t i = 0; i < m_height; i++) {
        unsigned char *src = &m_data[0] + (m_height - i - 1) * m_pitch;
        for (int remaining = int(m_width); remaining > 0; ) {
            int raw = 1;
            if (tex_tga_compress) {
                int run = 1;
                for (unsigned char *scan = src; run < u::min(remaining, 128); run++) {
                    scan += m_bpp;
                    if (src[0] != scan[0] || src[1] != scan[1] || src[2] != scan[2] || (m_bpp == 4 && src[3] != scan[3]))
                        break;
                }
                if (run > 1) {
                    outData.push_back(0x80 | (run - 1));
                    for (int i = 2; i >= 0; i--)
                        outData.push_back(src[i]);
                    if (m_bpp == 4)
                        outData.push_back(src[3]);
                    src += run*m_bpp;
                    remaining -= run;
                    if (remaining <= 0)
                        break;
                }
                for (const unsigned char *scan = src; raw < u::min(remaining, 128); raw++) {
                    scan += m_bpp;
                    if (src[0] == scan[0] || src[1] == scan[1] || src[2] == scan[2] || (m_bpp != 4 && src[3] == scan[3]))
                        break;
                }
                outData.push_back(raw - 1);
            } else {
                raw = u::min(remaining, 128);
            }
            unsigned char *dst = buffer;
            for (int j = 0; j < raw; j++) {
                for (int v = 2; v >= 0; v--)
                    dst[2-v] = src[v];
                if (m_bpp == 4)
                    dst[3] = src[3];
                dst += m_bpp;
                src += m_bpp;
            }
            for (size_t j = 0; j < raw*m_bpp; j++)
                outData.push_back(buffer[j]);
            remaining -= raw;
        }
    }
}

template <typename T>
static inline void memput(unsigned char *&store, const T &data) {
    memcpy(store, &data, sizeof data);
    store += sizeof data;
}

void texture::writeBMP(u::vector<unsigned char> &outData) {
    struct {
        char bfType[2];
        int32_t bfSize;
        int32_t bfReserved;
        int32_t bfDataOffset;
        int32_t biSize;
        int32_t biWidth;
        int32_t biHeight;
        int16_t biPlanes;
        int16_t biBitCount;
        int32_t biCompression;
        int32_t biSizeImage;
        int32_t biXPelsPerMeter;
        int32_t biYPelsPerMeter;
        int32_t biClrUsed;
        int32_t biClrImportant;
        void endianSwap() {
            bfSize = u::endianSwap(bfSize);
            bfReserved = u::endianSwap(bfReserved);
            bfDataOffset = u::endianSwap(bfDataOffset);
            biSize = u::endianSwap(biSize);
            biWidth = u::endianSwap(biWidth);
            biHeight = u::endianSwap(biHeight);
            biPlanes = u::endianSwap(biPlanes);
            biBitCount = u::endianSwap(biBitCount);
            biCompression = u::endianSwap(biCompression);
            biSizeImage = u::endianSwap(biSizeImage);
            biXPelsPerMeter = u::endianSwap(biXPelsPerMeter);
            biYPelsPerMeter = u::endianSwap(biYPelsPerMeter);
            biClrUsed = u::endianSwap(biClrUsed);
            biClrImportant = u::endianSwap(biClrImportant);
        }
    } bmph;

    const size_t bytesPerLine = (3 * (m_width + 1) / 4) * 4;
    const size_t headerSize = 54;
    const size_t imageSize = bytesPerLine * m_height;
    const size_t dataSize = headerSize + imageSize;

    // Populate header
    memcpy(bmph.bfType, (const void *)"BM", 2);
    bmph.bfSize = dataSize;
    bmph.bfReserved = 0;
    bmph.bfDataOffset = headerSize;
    bmph.biSize = 40;
    bmph.biWidth = m_width;
    bmph.biHeight = m_height;
    bmph.biPlanes = 1;
    bmph.biBitCount = 24;
    bmph.biCompression = 0;
    bmph.biSizeImage = imageSize;
    bmph.biXPelsPerMeter = 0;
    bmph.biYPelsPerMeter = 0;
    bmph.biClrUsed = 0;
    bmph.biClrImportant = 0;

    // line and data buffer
    u::vector<unsigned char> line(bytesPerLine);
    outData.resize(dataSize);
    bmph.endianSwap();

    unsigned char *store = &outData[0];
    memput(store, bmph.bfType);
    memput(store, bmph.bfSize);
    memput(store, bmph.bfReserved);
    memput(store, bmph.bfDataOffset);
    memput(store, bmph.biSize);
    memput(store, bmph.biWidth);
    memput(store, bmph.biHeight);
    memput(store, bmph.biPlanes);
    memput(store, bmph.biBitCount);
    memput(store, bmph.biCompression);
    memput(store, bmph.biSizeImage);
    memput(store, bmph.biXPelsPerMeter);
    memput(store, bmph.biYPelsPerMeter);
    memput(store, bmph.biClrUsed);
    memput(store, bmph.biClrImportant);

    // Write the bitmap
    for (int i = int(m_height) - 1; i >= 0; i--) {
        for (int j = 0; j < int(m_width); j++) {
            const int ipos = 3 * (m_width * i + j);
            line[3*j] = m_data[ipos + 2];
            line[3*j+1] = m_data[ipos + 1];
            line[3*j+2] = m_data[ipos];
        }
        memcpy(store, &line[0], line.size());
        store += line.size();
    }
}

void texture::writePNG(u::vector<unsigned char> &outData) {
    static constexpr int kMapping[] = { 0, 1, 2, 3, 4 };
    static constexpr int kFirstMapping[] = { 0, 1, 0, 5, 6 };

    u::vector<unsigned char> filter((m_width*m_bpp+1)*m_height);
    u::vector<signed char> lineBuffer(m_width*m_bpp);
    for (size_t j = 0; j < m_height; ++j) {
        const int *map = j ? kMapping : kFirstMapping;
        int best = 0;
        int bestValue = 0x7FFFFFFF;
        for (int p = 0; p < 2; ++p) {
            for (int k = p ? best : 0; k < 5; ++k) {
                const int type = map[k];
                int estimate = 0;
                const unsigned char *const z = &m_data[0] + m_pitch*j;
                const size_t s = m_bpp*m_width;
                const size_t b = m_bpp;
                size_t i = 0;
                switch (type) {
                case 0:
                    while (i < s) lineBuffer[i] = z[i], i++;
                    break;
                case 1:
                    while (i < b) lineBuffer[i] = z[i], i++;
                    while (i < s) lineBuffer[i] = z[i] - z[i-m_bpp], i++;
                    break;
                case 2:
                    while (i < s) lineBuffer[i] = z[i] - z[i-m_pitch], i++;
                    break;
                case 3:
                    while (i < b) lineBuffer[i] = z[i] - (z[i-m_pitch]>>1), i++;
                    while (i < s) lineBuffer[i] = z[i] - ((z[i-m_bpp] + z[i-m_pitch])>>1), i++;
                    break;
                case 4:
                    while (i < b) lineBuffer[i] = (signed char)(z[i] - png::paethPredictor(0, z[i-m_pitch], 0)), i++;
                    while (i < s) lineBuffer[i] = z[i] - png::paethPredictor(z[i-m_bpp], z[i-m_pitch], z[i-m_pitch-m_bpp]), i++;
                    break;
                case 5:
                    while (i < b) lineBuffer[i] = z[i], i++;
                    while (i < s) lineBuffer[i] = z[i] - (z[i-m_bpp]>>1), i++;
                    break;
                case 6:
                    while (i < b) lineBuffer[i] = z[i], i++;
                    while (i < s) lineBuffer[i] = z[i] - png::paethPredictor(z[i-m_bpp], 0, 0), i++;
                    break;
                }
                if (p)
                    break;
                for (size_t i = 0; i < m_width*m_bpp; ++i)
                    estimate += abs((signed char)lineBuffer[i]);
                if (estimate < bestValue) {
                    bestValue = estimate;
                    best = k;
                }
            }
        }
        // Got filter type
        filter[j*(m_width*m_bpp+1)] = (unsigned char)best;
        u::moveMemory(&filter[0]+j*(m_width*m_bpp+1)+1, &lineBuffer[0], m_width*m_bpp);
    }
    u::vector<unsigned char> compressed;
    u::zlib::compress(compressed, filter, tex_png_compress_quality);

    outData.resize(8+12+13+12+12+compressed.size());
    unsigned char *store = &outData[0];

    auto write = [&store](uint32_t data) {
        *store++ = (data >> 24) & 0xFF;
        *store++ = (data >> 16) & 0xFF;
        *store++ = (data >> 8) & 0xFF;
        *store++ = data & 0xFF;
    };

    auto tag = [&store](const char *data) {
        *store++ = data[0];
        *store++ = data[1];
        *store++ = data[2];
        *store++ = data[3];
    };

    auto crc = [&write](unsigned char *data, size_t length) {
        write(u::crc32(data - length - 4, length + 4));
    };

    // magic + header length
    memcpy(store, png::kMagic, 8);
    store += 8;
    write(13);

    // write header
    static constexpr int kType[] = { -1, 0, 4, 2, 6 };
    tag("IHDR");
    write(m_width);
    write(m_height);
    *store++ = 8;
    *store++ = kType[m_bpp];
    *store++ = 0;
    *store++ = 0;
    *store++ = 0;
    crc(store, 13);
    write(compressed.size());

    tag("IDAT");
    u::moveMemory(store, &compressed[0], compressed.size());
    store += compressed.size();
    crc(store, compressed.size());
    write(0);

    tag("IEND");
    crc(store, 0);
}

// writes JPEG baseline (no progressive)
// supports 1, 3 or 4 component (always writes YCbCr JPEG)
// uses fixed huffman tables
void texture::writeJPG(u::vector<unsigned char> &outData) {
    int quality = tex_jpg_compress_quality;
    quality = quality < 1 ? 1 : quality > 100 ? 100 : quality;
    quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

    static const unsigned char kDLC[] = "\x0\x0\x1\x5\x1\x1\x1\x1\x1\x1\x0\x0\x0\x0\x0\x0\x00";                     // dc luma codes
    static const unsigned char kDCC[] = "\x0\x0\x3\x1\x1\x1\x1\x1\x1\x1\x1\x1\x0\x0\x0\x0\x00";                     // dc chroma codes
    static const unsigned char kALC[] = "\x0\x0\x2\x1\x3\x3\x2\x4\x3\x5\x5\x4\x4\x0\x0\x1\x7d";                     // ac luma codes
    static const unsigned char kACC[] = "\x0\x0\x2\x1\x2\x4\x4\x3\x4\x7\x5\x4\x4\x0\x1\x2\x77";                     // ac chroma codes
    static const unsigned char kDLV[] = "\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb";                                     // dc luma values
    static const unsigned char kDCV[] = "\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb";                                     // dc chroma values

    static const unsigned char kZig[] = "\x00\x01\x05\x06\x0e\x0f\x1b\x1c\x02\x04\x07\x0d\x10\x1a\x1d\x2a"          // zig zag pattern
                                        "\x03\x08\x0c\x11\x19\x1e\x29\x2b\x09\x0b\x12\x18\x1f\x28\x2c\x35"
                                        "\x0a\x13\x17\x20\x27\x2d\x34\x36\x14\x16\x21\x26\x2e\x33\x37\x3c"
                                        "\x15\x22\x25\x2f\x32\x38\x3b\x3d\x23\x24\x30\x31\x39\x3a\x3e\x3f";

    static const unsigned char kYQT[] = "\x10\x0b\x0a\x10\x18\x28\x33\x3d\x0c\x0c\x0e\x13\x1a\x3a\x3c\x37"          // y generator
                                        "\x0e\x0d\x10\x18\x28\x39\x45\x38\x0e\x11\x16\x1d\x33\x57\x50\x3e"
                                        "\x12\x16\x25\x38\x44\x6d\x67\x4d\x18\x23\x37\x40\x51\x68\x71\x5c"
                                        "\x31\x40\x4e\x57\x67\x79\x78\x65\x48\x5c\x5f\x62\x70\x64\x67\x63";

    static const unsigned char kUQT[] = "\x11\x12\x18\x2f\x63\x63\x63\x63\x12\x15\x1a\x42\x63\x63\x63\x63"          // u&v generator
                                        "\x18\x1a\x38\x63\x63\x63\x63\x63\x2f\x42\x63\x63\x63\x63\x63\x63"
                                        "\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63"
                                        "\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63";

    static const unsigned char kALV[] = "\x01\x02\x03\x00\x04\x11\x05\x12\x21\x31\x41\x06\x13\x51\x61\x07\x22\x71"  // ac luma values
                                        "\x14\x32\x81\x91\xa1\x08\x23\x42\xb1\xc1\x15\x52\xd1\xf0\x24\x33\x62\x72"
                                        "\x82\x09\x0a\x16\x17\x18\x19\x1a\x25\x26\x27\x28\x29\x2a\x34\x35\x36\x37"
                                        "\x38\x39\x3a\x43\x44\x45\x46\x47\x48\x49\x4a\x53\x54\x55\x56\x57\x58\x59"
                                        "\x5a\x63\x64\x65\x66\x67\x68\x69\x6a\x73\x74\x75\x76\x77\x78\x79\x7a\x83"
                                        "\x84\x85\x86\x87\x88\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99\x9a\xa2\xa3"
                                        "\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xc2\xc3"
                                        "\xc4\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xe1\xe2"
                                        "\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa";

    static const unsigned char kACV[] = "\x00\x01\x02\x03\x11\x04\x05\x21\x31\x06\x12\x41\x51\x07\x61\x71\x13\x22"  // ac chroma values
                                        "\x32\x81\x08\x14\x42\x91\xa1\xb1\xc1\x09\x23\x33\x52\xf0\x15\x62\x72\xd1"
                                        "\x0a\x16\x24\x34\xe1\x25\xf1\x17\x18\x19\x1a\x26\x27\x28\x29\x2a\x35\x36"
                                        "\x37\x38\x39\x3a\x43\x44\x45\x46\x47\x48\x49\x4a\x53\x54\x55\x56\x57\x58"
                                        "\x59\x5a\x63\x64\x65\x66\x67\x68\x69\x6a\x73\x74\x75\x76\x77\x78\x79\x7a"
                                        "\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x92\x93\x94\x95\x96\x97\x98\x99\x9a"
                                        "\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba"
                                        "\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda"
                                        "\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa";

    unsigned char yTable[64];
    unsigned char uTable[64];
    for (size_t i = 0; i < 64; i++) {
        const int yIndex = (kYQT[i] * quality + 50) / 100;
        const int uIndex = (kUQT[i] * quality + 50) / 100;
        yTable[kZig[i]] = m::clamp(yIndex, 1, 255);
        uTable[kZig[i]] = m::clamp(uIndex, 1, 255);
    }

    static const float kSample[] = {
        1.000000000f * 2.828427125f, 1.387039845f * 2.828427125f,
        1.306562965f * 2.828427125f, 1.175875602f * 2.828427125f,
        1.000000000f * 2.828427125f, 0.785694958f * 2.828427125f,
        0.541196100f * 2.828427125f, 0.275899379f * 2.828427125f
    };

    float fyTable[64];
    float fuTable[64];
    for (size_t r = 0, k = 0; r < 8; r++) {
        for (size_t c = 0; c < 8; c++, k++) {
            fyTable[k] = 1.0f / (yTable[kZig[k]] * kSample[r] * kSample[c]);
            fuTable[k] = 1.0f / (uTable[kZig[k]] * kSample[r] * kSample[c]);
        }
    }

    auto writeBytes = [&outData](const u::initializer_list<unsigned char> &values) {
        for (const auto& value : values)
            outData.push_back(value);
    };

    const unsigned char hHi = m_height >> 8;
    const unsigned char hLo = m_height & 0xFF;
    const unsigned char wHi = m_width >> 8;
    const unsigned char wLo = m_width & 0xFF;

    writeBytes({0xFF, 0xD8});       // SOI
    writeBytes({0xFF, 0xE0});       // APP0
    writeBytes({0x00, 0x10});       // APP0 (length) 16 bytes
    writeBytes({'J','F','I','F'});  // APP0 (identifier)
    writeBytes({0x00});             // APP0 (density units) 0 = no units;
    writeBytes({0x01, 0x01});       // APP0 (Xdensity) 257
    writeBytes({0x00, 0x00});       // APP0 (Ydensity) 0
    writeBytes({0x01});             // APP0 (Xthumbnail) 1
    writeBytes({0x00});             // APP0 (Ythumbnail) 0
    writeBytes({0x01, 0x00, 0x00}); // APP0 (thumbnail data)

    writeBytes({0xFF, 0xDB});       // DQT
    writeBytes({0x00, 0x84});       // DQT length
    outData.push_back(0x00);        // DQT information (y table)
    outData.insert(outData.end(), yTable, yTable + sizeof yTable);
    outData.push_back(0x01);        // DQT information (u table)
    outData.insert(outData.end(), uTable, uTable + sizeof uTable);

    writeBytes({0xFF, 0xC0});       // SOF0
    writeBytes({0x00, 0x11});       // SOF0 (length) 17 bytes
    writeBytes({0x08});             // SOF0 (data precision) 8 bits
    writeBytes({hHi, hLo});         // SOF0 (image height)
    writeBytes({wHi, wLo});         // SOF0 (image width)
    writeBytes({0x03});             // SOF0 (number of components 3=YCbCr)
    writeBytes({0x01, 0x11, 0x00}); // SOF0 (component read order for Y)
    writeBytes({0x02, 0x11, 0x01}); // SOF0 (component read order for Cb)
    writeBytes({0x03, 0x11, 0x01}); // SOF0 (component read order for Cr)

    writeBytes({0xFF, 0xC4});       // DHT
    writeBytes({0x01, 0xA2});       // DHT (length) 41473 bytes

    // DC luma info
    outData.push_back(0x00);
    outData.insert(outData.end(), kDLC + 1, kDLC + sizeof kDLC - 1);
    outData.insert(outData.end(), kDLV, kDLV + sizeof kDLV - 1);
    // AC luma info
    outData.push_back(0x10);
    outData.insert(outData.end(), kALC + 1, kALC + sizeof kALC - 1);
    outData.insert(outData.end(), kALV, kALV + sizeof kALV - 1);
    // DC chroma info
    outData.push_back(0x01);
    outData.insert(outData.end(), kDCC + 1, kDCC + sizeof kDCC - 1);
    outData.insert(outData.end(), kDCV, kDCV + sizeof kDCV - 1);
    // AC chroma info
    outData.push_back(0x11);
    outData.insert(outData.end(), kACC + 1, kACC + sizeof kACC - 1);
    outData.insert(outData.end(), kACV, kACV + sizeof kACV - 1);

    writeBytes({0xFF, 0xDA});       // SOS
    writeBytes({0x00, 0x0C});       // SOS (length) 3072 bytes
    writeBytes({0x03});             // SOS (components in scan)
    writeBytes({0x01, 0x00});       // SOS (component table order Y)
    writeBytes({0x02, 0x11});       // SOS (component table order Cb)
    writeBytes({0x03, 0x11});       // SOS (component table order Cr)
    writeBytes({0x00, 0x3F, 0x00}); // SOS (ignored bytes)

    // write out bits
    int bitBuffer = 0;
    int bitCount = 0;
    auto writeBits = [&](const uint16_t *bits) {
        bitCount += bits[1];
        bitBuffer |= bits[0] << (24 - bitCount);
        while (bitCount >= 8) {
            unsigned char byte = (bitBuffer >> 16) & 255;
            outData.push_back(byte);
            if (byte == 255)
                outData.push_back(0);
            bitBuffer <<= 8;
            bitCount -= 8;
        }
    };

    // fixed huffman tables
    static const uint16_t kYDCHT[256][2] = {
        { 0x0000, 0x0002 }, { 0x0002, 0x0003 }, { 0x0003, 0x0003 }, { 0x0004, 0x0003 },
        { 0x0005, 0x0003 }, { 0x0006, 0x0003 }, { 0x000e, 0x0004 }, { 0x001e, 0x0005 },
        { 0x003e, 0x0006 }, { 0x007e, 0x0007 }, { 0x00fe, 0x0008 }, { 0x01fe, 0x0009 }
    };

    static const uint16_t kUDCHT[256][2] = {
        { 0x0000, 0x0002 }, { 0x0001, 0x0002 }, { 0x0002, 0x0002 }, { 0x0006, 0x0003 },
        { 0x000e, 0x0004 }, { 0x001e, 0x0005 }, { 0x003e, 0x0006 }, { 0x007e, 0x0007 },
        { 0x00fe, 0x0008 }, { 0x01fe, 0x0009 }, { 0x03fe, 0x000a }, { 0x07fe, 0x000b }
    };

    static const uint16_t kYACHT[256][2] = {
        { 0x000a, 0x0004 }, { 0x0000, 0x0002 }, { 0x0001, 0x0002 }, { 0x0004, 0x0003 },
        { 0x000b, 0x0004 }, { 0x001a, 0x0005 }, { 0x0078, 0x0007 }, { 0x00f8, 0x0008 },
        { 0x03f6, 0x000a }, { 0xff82, 0x0010 }, { 0xff83, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x000c, 0x0004 }, { 0x001b, 0x0005 }, { 0x0079, 0x0007 },
        { 0x01f6, 0x0009 }, { 0x07f6, 0x000b }, { 0xff84, 0x0010 }, { 0xff85, 0x0010 },
        { 0xff86, 0x0010 }, { 0xff87, 0x0010 }, { 0xff88, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x001c, 0x0005 }, { 0x00f9, 0x0008 }, { 0x03f7, 0x000a },
        { 0x0ff4, 0x000c }, { 0xff89, 0x0010 }, { 0xff8a, 0x0010 }, { 0xff8b, 0x0010 },
        { 0xff8c, 0x0010 }, { 0xff8d, 0x0010 }, { 0xff8e, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x003a, 0x0006 }, { 0x01f7, 0x0009 }, { 0x0ff5, 0x000c },
        { 0xff8f, 0x0010 }, { 0xff90, 0x0010 }, { 0xff91, 0x0010 }, { 0xff92, 0x0010 },
        { 0xff93, 0x0010 }, { 0xff94, 0x0010 }, { 0xff95, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x003b, 0x0006 }, { 0x03f8, 0x000a }, { 0xff96, 0x0010 },
        { 0xff97, 0x0010 }, { 0xff98, 0x0010 }, { 0xff99, 0x0010 }, { 0xff9a, 0x0010 },
        { 0xff9b, 0x0010 }, { 0xff9c, 0x0010 }, { 0xff9d, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x007a, 0x0007 }, { 0x07f7, 0x000b }, { 0xff9e, 0x0010 },
        { 0xff9f, 0x0010 }, { 0xffa0, 0x0010 }, { 0xffa1, 0x0010 }, { 0xffa2, 0x0010 },
        { 0xffa3, 0x0010 }, { 0xffa4, 0x0010 }, { 0xffa5, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x007b, 0x0007 }, { 0x0ff6, 0x000c }, { 0xffa6, 0x0010 },
        { 0xffa7, 0x0010 }, { 0xffa8, 0x0010 }, { 0xffa9, 0x0010 }, { 0xffaa, 0x0010 },
        { 0xffab, 0x0010 }, { 0xffac, 0x0010 }, { 0xffad, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x00fa, 0x0008 }, { 0x0ff7, 0x000c }, { 0xffae, 0x0010 },
        { 0xffaf, 0x0010 }, { 0xffb0, 0x0010 }, { 0xffb1, 0x0010 }, { 0xffb2, 0x0010 },
        { 0xffb3, 0x0010 }, { 0xffb4, 0x0010 }, { 0xffb5, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01f8, 0x0009 }, { 0x7fc0, 0x000f }, { 0xffb6, 0x0010 },
        { 0xffb7, 0x0010 }, { 0xffb8, 0x0010 }, { 0xffb9, 0x0010 }, { 0xffba, 0x0010 },
        { 0xffbb, 0x0010 }, { 0xffbc, 0x0010 }, { 0xffbd, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01f9, 0x0009 }, { 0xffbe, 0x0010 }, { 0xffbf, 0x0010 },
        { 0xffc0, 0x0010 }, { 0xffc1, 0x0010 }, { 0xffc2, 0x0010 }, { 0xffc3, 0x0010 },
        { 0xffc4, 0x0010 }, { 0xffc5, 0x0010 }, { 0xffc6, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01fa, 0x0009 }, { 0xffc7, 0x0010 }, { 0xffc8, 0x0010 },
        { 0xffc9, 0x0010 }, { 0xffca, 0x0010 }, { 0xffcb, 0x0010 }, { 0xffcc, 0x0010 },
        { 0xffcd, 0x0010 }, { 0xffce, 0x0010 }, { 0xffcf, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x03f9, 0x000a }, { 0xffd0, 0x0010 }, { 0xffd1, 0x0010 },
        { 0xffd2, 0x0010 }, { 0xffd3, 0x0010 }, { 0xffd4, 0x0010 }, { 0xffd5, 0x0010 },
        { 0xffd6, 0x0010 }, { 0xffd7, 0x0010 }, { 0xffd8, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x03fa, 0x000a }, { 0xffd9, 0x0010 }, { 0xffda, 0x0010 },
        { 0xffdb, 0x0010 }, { 0xffdc, 0x0010 }, { 0xffdd, 0x0010 }, { 0xffde, 0x0010 },
        { 0xffdf, 0x0010 }, { 0xffe0, 0x0010 }, { 0xffe1, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x07f8, 0x000b }, { 0xffe2, 0x0010 }, { 0xffe3, 0x0010 },
        { 0xffe4, 0x0010 }, { 0xffe5, 0x0010 }, { 0xffe6, 0x0010 }, { 0xffe7, 0x0010 },
        { 0xffe8, 0x0010 }, { 0xffe9, 0x0010 }, { 0xffea, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0xffeb, 0x0010 }, { 0xffec, 0x0010 }, { 0xffed, 0x0010 },
        { 0xffee, 0x0010 }, { 0xffef, 0x0010 }, { 0xfff0, 0x0010 }, { 0xfff1, 0x0010 },
        { 0xfff2, 0x0010 }, { 0xfff3, 0x0010 }, { 0xfff4, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x07f9, 0x000b }, { 0xfff5, 0x0010 }, { 0xfff6, 0x0010 }, { 0xfff7, 0x0010 },
        { 0xfff8, 0x0010 }, { 0xfff9, 0x0010 }, { 0xfffa, 0x0010 }, { 0xfffb, 0x0010 },
        { 0xfffc, 0x0010 }, { 0xfffd, 0x0010 }, { 0xfffe, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }
    };

    static const uint16_t kUACHT[256][2] = {
        { 0x0000, 0x0002 }, { 0x0001, 0x0002 }, { 0x0004, 0x0003 }, { 0x000a, 0x0004 },
        { 0x0018, 0x0005 }, { 0x0019, 0x0005 }, { 0x0038, 0x0006 }, { 0x0078, 0x0007 },
        { 0x01f4, 0x0009 }, { 0x03f6, 0x000a }, { 0x0ff4, 0x000c }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x000b, 0x0004 }, { 0x0039, 0x0006 }, { 0x00f6, 0x0008 },
        { 0x01f5, 0x0009 }, { 0x07f6, 0x000b }, { 0x0ff5, 0x000c }, { 0xff88, 0x0010 },
        { 0xff89, 0x0010 }, { 0xff8a, 0x0010 }, { 0xff8b, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x001a, 0x0005 }, { 0x00f7, 0x0008 }, { 0x03f7, 0x000a },
        { 0x0ff6, 0x000c }, { 0x7fc2, 0x000f }, { 0xff8c, 0x0010 }, { 0xff8d, 0x0010 },
        { 0xff8e, 0x0010 }, { 0xff8f, 0x0010 }, { 0xff90, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x001b, 0x0005 }, { 0x00f8, 0x0008 }, { 0x03f8, 0x000a },
        { 0x0ff7, 0x000c }, { 0xff91, 0x0010 }, { 0xff92, 0x0010 }, { 0xff93, 0x0010 },
        { 0xff94, 0x0010 }, { 0xff95, 0x0010 }, { 0xff96, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x003a, 0x0006 }, { 0x01f6, 0x0009 }, { 0xff97, 0x0010 },
        { 0xff98, 0x0010 }, { 0xff99, 0x0010 }, { 0xff9a, 0x0010 }, { 0xff9b, 0x0010 },
        { 0xff9c, 0x0010 }, { 0xff9d, 0x0010 }, { 0xff9e, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x003b, 0x0006 }, { 0x03f9, 0x000a }, { 0xff9f, 0x0010 },
        { 0xffa0, 0x0010 }, { 0xffa1, 0x0010 }, { 0xffa2, 0x0010 }, { 0xffa3, 0x0010 },
        { 0xffa4, 0x0010 }, { 0xffa5, 0x0010 }, { 0xffa6, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0079, 0x0007 }, { 0x07f7, 0x000b }, { 0xffa7, 0x0010 },
        { 0xffa8, 0x0010 }, { 0xffa9, 0x0010 }, { 0xffaa, 0x0010 }, { 0xffab, 0x0010 },
        { 0xffac, 0x0010 }, { 0xffad, 0x0010 }, { 0xffae, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x007a, 0x0007 }, { 0x07f8, 0x000b }, { 0xffaf, 0x0010 },
        { 0xffb0, 0x0010 }, { 0xffb1, 0x0010 }, { 0xffb2, 0x0010 }, { 0xffb3, 0x0010 },
        { 0xffb4, 0x0010 }, { 0xffb5, 0x0010 }, { 0xffb6, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x00f9, 0x0008 }, { 0xffb7, 0x0010 }, { 0xffb8, 0x0010 },
        { 0xffb9, 0x0010 }, { 0xffba, 0x0010 }, { 0xffbb, 0x0010 }, { 0xffbc, 0x0010 },
        { 0xffbd, 0x0010 }, { 0xffbe, 0x0010 }, { 0xffbf, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01f7, 0x0009 }, { 0xffc0, 0x0010 }, { 0xffc1, 0x0010 },
        { 0xffc2, 0x0010 }, { 0xffc3, 0x0010 }, { 0xffc4, 0x0010 }, { 0xffc5, 0x0010 },
        { 0xffc6, 0x0010 }, { 0xffc7, 0x0010 }, { 0xffc8, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01f8, 0x0009 }, { 0xffc9, 0x0010 }, { 0xffca, 0x0010 },
        { 0xffcb, 0x0010 }, { 0xffcc, 0x0010 }, { 0xffcd, 0x0010 }, { 0xffce, 0x0010 },
        { 0xffcf, 0x0010 }, { 0xffd0, 0x0010 }, { 0xffd1, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01f9, 0x0009 }, { 0xffd2, 0x0010 }, { 0xffd3, 0x0010 },
        { 0xffd4, 0x0010 }, { 0xffd5, 0x0010 }, { 0xffd6, 0x0010 }, { 0xffd7, 0x0010 },
        { 0xffd8, 0x0010 }, { 0xffd9, 0x0010 }, { 0xffda, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x01fa, 0x0009 }, { 0xffdb, 0x0010 }, { 0xffdc, 0x0010 },
        { 0xffdd, 0x0010 }, { 0xffde, 0x0010 }, { 0xffdf, 0x0010 }, { 0xffe0, 0x0010 },
        { 0xffe1, 0x0010 }, { 0xffe2, 0x0010 }, { 0xffe3, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x07f9, 0x000b }, { 0xffe4, 0x0010 }, { 0xffe5, 0x0010 },
        { 0xffe6, 0x0010 }, { 0xffe7, 0x0010 }, { 0xffe8, 0x0010 }, { 0xffe9, 0x0010 },
        { 0xffea, 0x0010 }, { 0xffeb, 0x0010 }, { 0xffec, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x3fe0, 0x000e }, { 0xffed, 0x0010 }, { 0xffee, 0x0010 },
        { 0xffef, 0x0010 }, { 0xfff0, 0x0010 }, { 0xfff1, 0x0010 }, { 0xfff2, 0x0010 },
        { 0xfff3, 0x0010 }, { 0xfff4, 0x0010 }, { 0xfff5, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 },
        { 0x03fa, 0x000a }, { 0x7fc3, 0x000f }, { 0xfff6, 0x0010 }, { 0xfff7, 0x0010 },
        { 0xfff8, 0x0010 }, { 0xfff9, 0x0010 }, { 0xfffa, 0x0010 }, { 0xfffb, 0x0010 },
        { 0xfffc, 0x0010 }, { 0xfffd, 0x0010 }, { 0xfffe, 0x0010 }, { 0x0000, 0x0000 },
        { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }, { 0x0000, 0x0000 }
    };

    auto process = [&](float *c, float *table, int dc, const uint16_t (&htdc)[256][2], const uint16_t (&htac)[256][2]) {
        const unsigned short eob[2] = { htac[0x00][0], htac[0x00][1] };
        const unsigned short m16[2] = { htac[0xF0][0], htac[0xF0][1] };

        auto calcBits = [](int value, uint16_t (&bits)[2]) {
            int t = abs(value);
            int m = value < 0 ? value - 1 : value;
            bits[1] = 1;
            while (t >>= 1) bits[1]++;
            bits[0] = m & ((1 << bits[1]) - 1);
        };

        auto dct = [](float &d0, float &d1, float &d2, float &d3, float &d4, float &d5, float &d6, float &d7) {
            const float t0 = d0 + d7, t7 = d0 - d7;
            const float t1 = d1 + d6, t6 = d1 - d6;
            const float t2 = d2 + d5, t5 = d2 - d5;
            const float t3 = d3 + d4, t4 = d3 - d4;
            // event part
            float t10 = t0 + t3, t13 = t0 - t3;          // phase 2
            float t11 = t1 + t2, t12 = t1 - t2;
            d0 = t10 + t11;                              // phase 3
            d4 = t10 - t11;
            const float z1 = (t12 + t13) * 0.707106781f; // c4
            d2 = t13 + z1;                               // phase 5
            d6 = t13 - z1;
            // odd part
            t10 = t4 + t5;                               // phase 2
            t11 = t5 + t6;
            t12 = t6 + t7;
            const float z5 = (t10 - t12) * 0.382683433f; // c6
            const float z2 = t10 * 0.541196100f + z5;    // c2-c6
            const float z4 = t12 * 1.306562965f + z5;    // c2+c6
            const float z3 = t11 * 0.707106781f;         // c4
            const float z11 = t7 + z3;                   // phase 5
            const float z13 = t7 - z3;
            d5 = z13 + z2;                               // phase 6
            d3 = z13 - z2;
            d1 = z11 + z4;
            d7 = z11 - z4;
        };

        for (size_t i = 0; i < 64; i += 8)
            dct(c[i], c[i+1], c[i+2], c[i+3], c[i+4], c[i+5], c[i+6], c[i+7]);
        for (size_t i = 0; i < 8; i++)
            dct(c[i], c[i+8], c[i+16], c[i+24], c[i+32], c[i+40], c[i+48], c[i+56]);

        // quantize the coefficients
        int du[64];
        for (size_t i = 0; i < 64; i++) {
            const float v = c[i] * table[i];
            du[kZig[i]] = (int)(v < 0 ? m::ceil(v - 0.5f) : m::floor(v + 0.5f));
        }

        // encode DC
        int difference = du[0] - dc;
        if (difference) {
            uint16_t bits[2];
            calcBits(difference, bits);
            writeBits(htdc[bits[1]]);
            writeBits(bits);
        } else {
            writeBits(htdc[0]);
        }

        // encode ACs
        int end = 63;
        for (; end > 0 && !du[end]; --end)
            ;
        if (!end) {
            writeBits(eob);
            return du[0];
        }
        for (int i = 1; i <= end; i++) {
            int beg = i;
            for (; !du[i] && i <= end; i++)
                ;
            int zeroes = i - beg;
            if (zeroes >= 16) {
                int length = zeroes >> 4;
                for (int m = 1; m <= length; m++)
                    writeBits(m16);
                zeroes &= 15;
            }
            uint16_t bits[2];
            calcBits(du[i], bits);
            writeBits(htac[(zeroes << 4) + bits[1]]);
            writeBits(bits);
        }
        if (end != 63)
            writeBits(eob);
        return du[0];
    };

    // encode 8x8 macro blocks
    int dc[] = { 0, 0, 0 };
    const size_t rOffset = m_format == kTexFormatBGRA || m_format == kTexFormatBGR ? 2 : 0;
    const size_t gOffset = m_bpp > 1 ? 1 : 0;
    const size_t bOffset = m_bpp == 1 || rOffset == 2 ? 0 : 2;
    for (size_t y = 0; y < m_height; y += 8) {
        for (size_t x = 0; x < m_width; x += 8) {
            float du[3][64];
            for (size_t r = y, p = 0; r < y+8; r++) {
                for (size_t c = x; c < x+8; c++, p++) {
                    size_t m = r*m_width*m_bpp + c*m_bpp;
                    if (r >= m_height)
                        m -= m_width*m_bpp*(r+1 - m_height);
                    if (c >= m_width)
                        m -= m_bpp*(c+1 - m_width);
                    const float R = m_data[m + rOffset];
                    const float G = m_data[m + gOffset];
                    const float B = m_data[m + bOffset];
                    du[0][p] =  0.29900f * R + 0.58700f * G + 0.11400f * B - 128.0f;
                    du[1][p] = -0.16874f * R - 0.33126f * G + 0.50000f * B;
                    du[2][p] =  0.50000f * R - 0.41869f * G - 0.08131f * B;
                }
            }
            dc[0] = process(du[0], fyTable, dc[0], kYDCHT, kYACHT);
            dc[1] = process(du[1], fuTable, dc[1], kUDCHT, kUACHT);
            dc[2] = process(du[2], fuTable, dc[2], kUDCHT, kUACHT);
        }
    }

    // bit alignment for EOI marker
    static const uint16_t kFillBits[] = { 0x7F, 0x07 };
    writeBits(kFillBits);

    writeBytes({0xFF, 0xD9}); // EOI
}

bool texture::save(const u::string &file, saveFormat format, float quality) {
    if (m_data.empty())
        return false;

    if (quality != 1.0f) {
        const float f = textureQualityScale(quality);
        size_t w = m_width * f;
        size_t h = m_height * f;
        if (w == 0) w = 1;
        if (h == 0) h = 1;
        resize(w, h);
    }

    static const struct {
        void (texture::*function)(u::vector<unsigned char> &out);
        int format;
    } kExtensions[] = {
        { &texture::writeTGA, kSaveTGA },
        { &texture::writeBMP, kSaveBMP },
        { &texture::writePNG, kSavePNG },
        { &texture::writeJPG, kSaveJPG }
    };

    u::vector<unsigned char> data;
    for (const auto &it : kExtensions) {
        if (it.format != format)
            continue;
        (this->*it.function)(data);
        return u::write(data, file + "." + kSaveFormatExtensions[format], "wb");
    }

    return false;
}

bool texture::from(const unsigned char *const data, size_t length, size_t width,
    size_t height, bool normal, textureFormat format, size_t mips)
{
    *this = u::move(texture(data, length, width, height, normal, format, mips));
    return true;
}

void texture::resize(size_t width, size_t height) {
    u::vector<unsigned char> data(m_bpp * width * height);
    scale(&m_data[0], m_width, m_height, m_bpp, m_pitch, &data[0], width, height);
    m_data = u::move(data);
    m_width = width;
    m_height = height;
    m_pitch = m_width * m_bpp;
    normalize();
}

void texture::normalize() {
    if (!(flags() & kTexFlagNormal))
        return;
    u::vector<unsigned char> data(m_bpp * m_width * m_height);
    unsigned char *dst = &data[0];
    for (size_t y = 0; y < m_height; y++) {
        for (size_t x = 0; x < m_width; x++) {
            m::vec3 normal(0.0f, 0.0f, 255.0f);
            normal.x += m_data[y*m_pitch + ((x+m_width-1)%m_width)*m_bpp];
            normal.x -= m_data[y*m_pitch + ((x+1)%m_width)*m_bpp];
            normal.y += m_data[((y+m_height-1)%m_height)*m_pitch + x*m_bpp];
            normal.y -= m_data[((y+1)%m_height)*m_pitch + x*m_bpp];
            normal.normalize();
            *dst++ = (unsigned char)(127.5f + normal.x*127.5f);
            *dst++ = (unsigned char)(127.5f + normal.y*127.5f);
        }
    }
    m_data = u::move(data);
}

void texture::unload() {
    m_data.destroy();
}

const u::string &texture::hashString() const {
    return m_hashString;
}

int texture::flags() const {
    return m_flags;
}

size_t texture::width() const {
    return m_width;
}

size_t texture::height() const {
    return m_height;
}

textureFormat texture::format() const {
    return m_format;
}

const char *texture::components() const {
    switch (m_format) {
    case kTexFormatRGB:       return "RGB";
    case kTexFormatRGBA:      return "RGBA";
    case kTexFormatBGR:       return "BGR";
    case kTexFormatBGRA:      return "BGRA";
    case kTexFormatRG:        return "RG";
    case kTexFormatLuminance: return "Luminance";
    case kTexFormatDXT1:      return "DXT1";
    case kTexFormatDXT3:      return "DXT3";
    case kTexFormatDXT5:      return "DXT5";
    case kTexFormatBC4U:      return "BC4U/ATI1";
    case kTexFormatBC4S:      return "BC4S";
    case kTexFormatBC5U:      return "BC5U/ATI2";
    case kTexFormatBC5S:      return "BC5S";
    }
    U_UNREACHABLE();
};

size_t texture::size() const {
    return m_data.size();
}

size_t texture::bpp() const {
    return m_bpp;
}

size_t texture::mips() const {
    return m_mips;
}

size_t texture::pitch() const {
    return m_pitch;
}

const unsigned char *texture::data() const {
    return &m_data[0];
}
