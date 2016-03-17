#include "engine.h"
#include "cvar.h"

#include "r_texture.h"

#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"
#include "u_zlib.h"

#include "m_const.h"

VAR(int, r_tex_compress, "texture compression", 0, 1, 1);
VAR(int, r_tex_compress_cache, "cache compressed textures", 0, 1, 1);
VAR(int, r_tex_compress_cache_zlib, "zlib compress cached compressed textures", 0, 1, 1);
VAR(int, r_aniso, "anisotropic filtering", 0, 16, 4);
VAR(int, r_bilinear, "bilinear filtering", 0, 1, 1);
VAR(int, r_trilinear, "trilinear filtering", 0, 1, 1);
VAR(int, r_mipmaps, "mipmaps", 0, 1, 1);
VAR(int, r_dxt_optimize, "DXT endpoints optimization", 0, 1, 1);

#if defined(DXT_COMPRESSOR)
VAR(int, r_dxt_compressor, "DXT compressor", 0, 1, 1);
#else
VAR(int, r_dxt_compressor, "DXT compressor", 0, 0, 0);
#endif

VAR(float, r_texquality, "texture quality", 0.0f, 1.0f, 1.0f);

namespace r {

#if defined(GL_UNSIGNED_INT_8_8_8_8_REV)
#   define R_TEX_DATA_RGBA GL_UNSIGNED_INT_8_8_8_8_REV
#else
#   define R_TEX_DATA_RGBA GL_UNSINGED_BYTE
#endif
#if defined(GL_UNSIGNED_INT_8_8_8_8)
#   define R_TEX_DATA_BGRA GL_UNSIGNED_INT_8_8_8_8
#else
#   define R_TEX_DATA_BGRA GL_UNSIGNED_BYTE
#endif
#define R_TEX_DATA_RGB       GL_UNSIGNED_BYTE
#define R_TEX_DATA_BGR       GL_UNSIGNED_BYTE
#define R_TEX_DATA_LUMINANCE GL_UNSIGNED_BYTE
#define R_TEX_DATA_RG        GL_UNSIGNED_BYTE

enum dxtType {
    kDXT1,
    kDXT5
};

enum dxtColor {
    kDXTColor33,
    kDXTColor66,
    kDXTColor50
};

struct dxtBlock {
    uint16_t color0;
    uint16_t color1;
    uint32_t pixels;
};

static uint16_t dxtPack565(uint16_t &r, uint16_t &g, uint16_t &b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void dxtUnpack565(uint16_t src, uint16_t &r, uint16_t &g, uint16_t &b) {
    r = (((src>>11)&0x1F)*527 + 15) >> 6;
    g = (((src>>5)&0x3F)*259 + 35) >> 6;
    b = ((src&0x1F)*527 + 15) >> 6;
}

template <dxtColor E>
static uint16_t dxtCalcColor(uint16_t color0, uint16_t color1) {
    uint16_t r[3], g[3], b[3];
    dxtUnpack565(color0, r[0], g[0], b[0]);
    dxtUnpack565(color1, r[1], g[1], b[1]);
    if (E == kDXTColor33) {
        r[2] = (2*r[0] + r[1]) / 3;
        g[2] = (2*g[0] + g[1]) / 3;
        b[2] = (2*b[0] + b[1]) / 3;
    } else if (E == kDXTColor66) {
        r[2] = (r[0] + 2*r[1]) / 3;
        g[2] = (g[0] + 2*g[1]) / 3;
        b[2] = (b[0] + 2*b[1]) / 3;
    } else if (E == kDXTColor50) {
        r[2] = (r[0] + r[1]) / 2;
        g[2] = (g[0] + g[1]) / 2;
        b[2] = (b[0] + b[1]) / 2;
    }
    return dxtPack565(r[2], g[2], b[2]);
}

template <dxtType T>
static size_t dxtOptimize(unsigned char *data, size_t width, size_t height) {
    size_t count = 0;
    const size_t numBlocks = (width / 4) * (height / 4);
    dxtBlock *block = ((dxtBlock*)data) + (T == kDXT5); // DXT5: alpha block is first
    for (size_t i = 0; i != numBlocks; ++i, block += (T == kDXT1 ? 1 : 2)) {
        const uint16_t color0 = block->color0;
        const uint16_t color1 = block->color1;
        const uint32_t pixels = block->pixels;
        if (pixels == 0) {
            // Solid color0
            block->color1 = 0;
            count++;
        } else if (pixels == 0x55555555u) {
            // Solid color1, fill with color0 instead, possibly encoding the block
            // as 1-bit alpha if color1 is black.
            block->color0 = color1;
            block->color1 = 0;
            block->pixels = 0;
            count++;
        } else if (pixels == 0xAAAAAAAAu) {
            // Solid color2, fill with color0 instead, possibly encoding the block
            // as 1-bit alpha if color2 is black.
            block->color0 = (color0 > color1 || T == kDXT5)
                ? dxtCalcColor<kDXTColor33>(color0, color1)
                : dxtCalcColor<kDXTColor50>(color0, color1);
            block->color1 = 0;
            block->pixels = 0;
            count++;
        } else if (pixels == 0xFFFFFFFFu) {
            // Solid color3
            if (color0 > color1 || T == kDXT5) {
                // Fill with color0 instead, possibly encoding the block as 1-bit
                // alpha if color3 is black.
                block->color0 = dxtCalcColor<kDXTColor66>(color0, color1);
                block->color1 = 0;
                block->pixels = 0;
                count++;
            } else {
                // Transparent / solid black
                block->color0 = 0;
                block->color1 = T == kDXT1 ? 0xFFFFu : 0; // kDXT1: Transparent black
                if (T == kDXT5) // Solid black
                    block->pixels = 0;
                count++;
            }
        } else if (T == kDXT5 && (pixels & 0xAAAAAAAAu) == 0xAAAAAAAAu) {
            // Only interpolated colors are used, not the endpoints
            block->color0 = dxtCalcColor<kDXTColor66>(color0, color1);
            block->color1 = dxtCalcColor<kDXTColor33>(color0, color1);
            block->pixels = ~pixels;
            count++;
        } else if (T == kDXT5 && color0 < color1) {
            // Otherwise, ensure the colors are always in the same order
            block->color0 = color1;
            block->color1 = color0;
            block->pixels ^= 0x55555555u;
            count++;
        }
    }
    return count;
}

#if defined(DXT_COMPRESSOR)

#if defined(DXT_HIGHP)
typedef double real;
#else
typedef float real;
#endif

// Color line refinement iterations:
// Minimum is 1
// Default is 3
//
// The maximum really has a lot to do with how much error you'll eventually
// introduce due to precision of the `real' type used in the color line algorithm.
//
// It's suggested you use #define DXT_HIGHP if you want to increase this.
static constexpr size_t kRefineIterations = 3;

template <size_t C>
static inline void dxtComputeColorLine(const unsigned char *const uncompressed,
    float (&point)[3], float (&direction)[3])
{
    static constexpr real kSixteen = real(16.0);
    static constexpr real kOne = real(1.0);
    static constexpr real kZero = real(0.0);
    static constexpr real kInv16 = kOne / kSixteen;
    real sumR = kZero, sumG = kZero, sumB = kZero;
    real sumRR = kZero, sumGG = kZero, sumBB = kZero;
    real sumRG = kZero, sumRB = kZero, sumGB = kZero;

    for (size_t i = 0; i < 16*C; i += C) {
        sumR += uncompressed[i+0];
        sumG += uncompressed[i+1];
        sumB += uncompressed[i+2];
        sumRR += uncompressed[i+0] * uncompressed[i+0];
        sumGG += uncompressed[i+1] * uncompressed[i+1];
        sumBB += uncompressed[i+2] * uncompressed[i+2];
        sumRG += uncompressed[i+0] * uncompressed[i+1];
        sumRB += uncompressed[i+0] * uncompressed[i+2];
        sumGB += uncompressed[i+1] * uncompressed[i+2];
    }
    // Average all sums
    sumR *= kInv16;
    sumG *= kInv16;
    sumB *= kInv16;
    // Convert squares to squares of the value minus their average
    sumRR -= kSixteen * sumR * sumR;
    sumGG -= kSixteen * sumG * sumG;
    sumBB -= kSixteen * sumB * sumB;
    sumRG -= kSixteen * sumR * sumG;
    sumRB -= kSixteen * sumR * sumB;
    sumGB -= kSixteen * sumG * sumB;
    // The point on the color line is the average
    point[0] = sumR;
    point[1] = sumG;
    point[2] = sumB;
    // RYGDXT covariance matrix
    direction[0] = real(1.0);
    direction[1] = real(2.718281828);
    direction[2] = real(3.141592654);
    for (size_t i = 0; i < kRefineIterations; ++i) {
        sumR = direction[0];
        sumG = direction[1];
        sumB = direction[2];
        direction[0] = float(sumR*sumRR + sumG*sumRG + sumB*sumRB);
        direction[1] = float(sumR*sumRG + sumG*sumGG + sumB*sumGB);
        direction[2] = float(sumR*sumRB + sumG*sumGB + sumB*sumBB);
    }
}

template <size_t C>
static inline void dxtLSEMasterColorsClamp(uint16_t (&colors)[2],
    const unsigned char *const uncompressed)
{
    float sumx1[] = { 0.0f, 0.0f, 0.0f };
    float sumx2[] = { 0.0f, 0.0f, 0.0f };
    dxtComputeColorLine<C>(uncompressed, sumx1, sumx2);

    const float length = 1.0f / (0.00001f + sumx2[0]*sumx2[0] + sumx2[1]*sumx2[1] + sumx2[2]*sumx2[2]);
    // Calcualte range for vector values
    float dotMax = sumx2[0] * uncompressed[0] +
                   sumx2[1] * uncompressed[1] +
                   sumx2[2] * uncompressed[2];
    float dotMin = dotMax;
    for (size_t i = 1; i < 16; ++i) {
        const float dot = sumx2[0] * uncompressed[i*C+0] +
                          sumx2[1] * uncompressed[i*C+1] +
                          sumx2[2] * uncompressed[i*C+2];
        if (dot < dotMin)
            dotMin = dot;
        else if (dot > dotMax)
            dotMax = dot;
    }

    // Calculate offset from the average location
    const float dot = sumx2[0]*sumx1[0] + sumx2[1]*sumx1[1] + sumx2[2]*sumx1[2];
    dotMin -= dot;
    dotMax -= dot;
    dotMin *= length;
    dotMax *= length;
    // Build the master colors
    uint16_t c0[3];
    uint16_t c1[3];
    for (size_t i = 0; i < 3; ++i) {
        c0[i] = m::clamp(int(0.5f + sumx1[i] + dotMax * sumx2[i]), 0, 255);
        c1[i] = m::clamp(int(0.5f + sumx1[i] + dotMin * sumx2[i]), 0, 255);
    }
    // Down sample the master colors to RGB565
    const uint16_t i = dxtPack565(c0[0], c0[1], c0[2]);
    const uint16_t j = dxtPack565(c1[0], c1[1], c1[2]);
    if (i > j)
        colors[0] = i, colors[1] = j;
    else
        colors[1] = i, colors[0] = j;
}

template <size_t C>
static inline void dxtCompressColorBlock(const unsigned char *const uncompressed, unsigned char (&compressed)[8]) {
    uint16_t encodeColor[2];
    dxtLSEMasterColorsClamp<C>(encodeColor, uncompressed);
    // Store 565 color
    compressed[0] = encodeColor[0] & 255;
    compressed[1] = (encodeColor[0] >> 8) & 255;
    compressed[2] = encodeColor[1] & 255;
    compressed[3] = (encodeColor[1] >> 8) & 255;
    for (size_t i = 4; i < 8; i++)
        compressed[i] = 0;

    // Reconstitute master color vectors
    uint16_t c0[3];
    uint16_t c1[3];
    dxtUnpack565(encodeColor[0], c0[0], c0[1], c0[2]);
    dxtUnpack565(encodeColor[1], c1[0], c1[1], c1[2]);

    float colorLine[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float length = 0.0f;
    for (size_t i = 0; i < 3; ++i) {
        colorLine[i] = float(c1[i] - c0[i]);
        length += colorLine[i] * colorLine[i];
    }
    if (length > 0.0f)
        length = 1.0f / length;
    // Scaling
    for (size_t i = 0; i < 3; i++)
        colorLine[i] *= length;
    // Offset portion of dot product
    const float dotOffset = colorLine[0]*c0[0] + colorLine[1]*c0[1] + colorLine[2]*c0[2];
    // Store rest of bits
    size_t nextBit = 8*4;
    for (size_t i = 0; i < 16; ++i) {
        // Find the dot product for this color, to place it on the line with
        // A range of [-1, 1]
        const float dotProduct = colorLine[0] * uncompressed[i*C+0] +
                                 colorLine[1] * uncompressed[i*C+1] +
                                 colorLine[2] * uncompressed[i*C+2] - dotOffset;
        // Map to [0, 3]
        const int nextValue = m::clamp(int(dotProduct * 3.0f + 0.5f), 0, 3);
        compressed[nextBit >> 3] |= "\x0\x2\x3\x1"[nextValue] << (nextBit & 7);
        nextBit += 2;
    }
}

static inline void dxtCompressAlphaBlock(const unsigned char *const uncompressed, unsigned char (&compressed)[8]) {
    unsigned char a0 = uncompressed[3];
    unsigned char a1 = uncompressed[3];
    for (size_t i = 4+3; i < 16*4; i += 4) {
        if (uncompressed[i] > a0)
            a0 = uncompressed[i];
        if (uncompressed[i] < a1)
            a1 = uncompressed[i];
    }
    compressed[0] = a0;
    compressed[1] = a1;
    for (size_t i = 2; i < 8; i++)
        compressed[i] = 0;
    size_t nextBit = 8*2;
    const float scale = 7.9999f / (a0 - a1);
    for (size_t i = 3; i < 16*4; i += 4) {
        const unsigned char value = "\x1\x7\x6\x5\x4\x3\x2\x0"[size_t((uncompressed[i] - a1) * scale) & 7];
        compressed[nextBit >> 3] |= value << (nextBit & 7);
        // Spans two bytes
        if ((nextBit & 7) > 5)
            compressed[1 + (nextBit >> 3)] |= value >> (8 - (nextBit & 7));
        nextBit += 3;
    }
}

// Note: `uncompressed' is assumed to be a RGB8 data stream when T == kDXT1 and
// RGBA8 data stream when T == kDXT5
template <dxtType T>
static u::vector<unsigned char> dxtCompress(const unsigned char *const uncompressed,
    size_t width, size_t height, size_t channels)
{
    size_t index = 0;
    const size_t chanStep = channels < 3 ? 0 : 1;
    const int hasAlpha = 1 - (channels & 1);
    const size_t outSize = ((width + 3) >> 2) * ((height + 3) >> 2) * (T == kDXT1 ? 8 : 16);
    u::vector<unsigned char> compressed(outSize);
    unsigned char ublock[16 * (T == kDXT1 ? 3 : 4)];
    unsigned char cblock[8];
    for (size_t j = 0; j < height; j += 4) {
        for (size_t i = 0; i < width; i += 4) {
            size_t z = 0;
            const size_t my = j + 4 >= height ? height - j : 4;
            const size_t mx = i + 4 >= width ? width - i : 4;
            for (size_t y = 0; y < my; ++y) {
                for (size_t x = 0; x < mx; ++x) {
                    for (size_t p = 0; p < 3; ++p)
                        ublock[z++] = uncompressed[((((j+y)*width)*channels)+((i+x)*channels))+(chanStep * p)];
                    if (T == kDXT5)
                        ublock[z++] = hasAlpha * uncompressed[(j+y)*width*channels+(i+x)*channels+channels-1] + (1 - hasAlpha) * 255;
                }
                for (size_t x = mx; x < 4; ++x)
                    for (size_t p = 0; p < (T == kDXT1 ? 3 : 4); ++p)
                        ublock[z++] = ublock[p];
            }
            for (size_t y = my; y < 4; ++y)
                for (size_t x = 0; x < 4; ++x)
                    for (size_t p = 0; p < (T == kDXT1 ? 3 : 4); ++p)
                        ublock[z++] = ublock[p];
            if (T == kDXT5) {
                dxtCompressAlphaBlock(ublock, cblock);
                for (size_t x = 0; x < 8; ++x)
                    compressed[index++] = cblock[x];
            }
            dxtCompressColorBlock<(T == kDXT1 ? 3 : 4)>(ublock, cblock);
            for (size_t x = 0; x < 8; ++x)
                compressed[index++] = cblock[x];
        }
    }
    return compressed;
}

template u::vector<unsigned char> dxtCompress<kDXT1>(const unsigned char *const uncompressed,
    size_t width, size_t height, size_t channels);
template u::vector<unsigned char> dxtCompress<kDXT5>(const unsigned char *const uncompressed,
    size_t width, size_t height, size_t channels);

#endif //! DXT_COMPRESSOR

static const unsigned char kTextureCacheVersion = 0x06;

struct textureCacheHeader {
    uint8_t version;
    uint16_t width;
    uint16_t height;
    uint32_t internal;
    uint16_t format;
    uint8_t compressed;
    uint8_t mips;
    void endianSwap();
};

void textureCacheHeader::endianSwap() {
    width = u::endianSwap(width);
    height = u::endianSwap(height);
    internal = u::endianSwap(internal);
    format = u::endianSwap(format);
}

static const char *cacheFormat(GLuint internal) {
    switch (internal) {
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
        return "RGBA_BPTC_UNORM";
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
        return "RGB_BPTC_SIGNED_FLOAT";
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return "RGBA_S3TC_DXT5";
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return "RGBA_S3TC_DXT1";
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
        return "RED_GREEN_RGTC2";
    case GL_COMPRESSED_RED_RGTC1_EXT:
        return "RED_RGTC1";
    case GL_COMPRESSED_RGB8_ETC2:
        return "RGB8_ETC2";
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
        return "RGBA8_ETC2_EAC";
    case GL_COMPRESSED_R11_EAC:
        return "R11_EAC";
    case GL_COMPRESSED_RG11_EAC:
        return "RG11_EAC";
    }
    return "";
}

static bool readCache(texture &tex, GLuint &internal) {
    if (!r_tex_compress)
        return false;

    // If the texture is not on disk then don't cache the compressed version
    // of it to disk.
    if (!(tex.flags() & kTexFlagDisk))
        return false;

    // If no compression was specified then don't read a cached compressed version
    // of it.
    if (tex.flags() & kTexFlagNoCompress)
        return false;

    // Do we even have it in cache?
    const u::string cacheString = u::format("cache/textures/%s", tex.hashString());
    const u::string file = neoUserPath() + cacheString;
    if (!u::exists(file))
        return false;

    // Found it in cache, unload the current texture and load the cache from disk.
    auto load = u::read(file, "rb");
    if (!load)
        return false;

    // Parse header
    const auto &vec = *load;
    textureCacheHeader head;
    memcpy(&head, &vec[0], sizeof head);
    if (head.version != kTextureCacheVersion) {
        u::remove(file);
        return false;
    }
    head.endianSwap();

    // Make sure we even support the format before using it
    switch (head.internal) {
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
        if (!gl::has(gl::ARB_texture_compression_bptc))
            return false;
        break;

    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        if (!gl::has(gl::EXT_texture_compression_s3tc))
            return false;
        break;

    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_RED_RGTC1_EXT:
        if (!gl::has(gl::EXT_texture_compression_rgtc))
            return false;
        break;

    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
        if (!gl::has(gl::ARB_ES3_compatibility))
            return false;
        break;
    }

    const unsigned char *const data = &vec[0] + sizeof head;
    const size_t length = vec.size() - sizeof head;

    // decompress
    u::vector<unsigned char> decompress;
    const unsigned char *fromData = nullptr;
    size_t fromSize = 0;
    if (head.compressed) {
        u::zlib::decompress(decompress, data, length);
        fromData = &decompress[0];
        fromSize = decompress.size();
    } else {
        fromData = data;
        fromSize = length;
    }

    internal = head.internal;

    // Now swap!
    tex.unload();
    tex.from(fromData, fromSize, head.width, head.height, false, textureFormat(head.format), head.mips);
    u::print("[cache] => loaded %.50s... %s (%s)\n", u::fixPath(cacheString),
        cacheFormat(head.internal), u::sizeMetric(fromSize));
    return true;
}

static bool writeCacheData(textureFormat format,
                           size_t texSize,
                           const u::string &cacheString,
                           unsigned char *compressedData,
                           size_t compressedWidth,
                           size_t compressedHeight,
                           size_t compressedSize,
                           size_t mips,
                           GLuint internal,
                           const char *from = "driver")
{
    // Build the header
    textureCacheHeader head;
    head.version = kTextureCacheVersion;

    // Note: the explicit casts are needed to match the textureCacheHeader types
    // so that the type-deduction for u::endianSwap actually works.
    head.width = compressedWidth;
    head.height = compressedHeight;
    head.internal = internal;
    head.format = format;
    head.compressed = uint8_t(r_tex_compress_cache_zlib);
    head.mips = mips;
    head.endianSwap();

    // Apply DXT optimizations if we can
    const bool dxt = internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
                     internal == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

    size_t dxtOptimCount = 0;
    if (r_dxt_optimize && dxt) {
        size_t offset = 0;
        size_t mipWidth = compressedWidth;
        size_t mipHeight = compressedHeight;
        const size_t blockSize = internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16;

        // Run the optimizer on each level
        for (size_t i = 0; i < mips; i++) {
            const size_t mipSize = ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * blockSize;
            const size_t count = (internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
                ? dxtOptimize<kDXT1>(compressedData + offset, mipWidth, mipHeight)
                : dxtOptimize<kDXT5>(compressedData + offset, mipWidth, mipHeight);
            // Only report for the base level
            if (i == 0)
                dxtOptimCount = count;
            gl::CompressedTexImage2D(GL_TEXTURE_2D, i, internal, mipWidth,
                mipHeight, 0, mipSize, compressedData + offset);
            offset += mipSize;
            mipWidth = u::max(mipWidth >> 1, 1_z);
            mipHeight = u::max(mipHeight >> 1, 1_z);
        }
    }

    // zlib compress the texture data
    u::vector<unsigned char> compressedTexture;
    const unsigned char *toData = nullptr;
    size_t toSize = 0;
    if (r_tex_compress_cache_zlib) {
        u::zlib::compress(compressedTexture, compressedData, compressedSize);
        toData = &compressedTexture[0];
        toSize = compressedTexture.size();
    } else {
        toData = compressedData;
        toSize = compressedSize;
    }

    // prepare file data
    u::vector<unsigned char> data(sizeof head + toSize);
    memcpy(&data[0], &head, sizeof head);
    memcpy(&data[0] + sizeof head, toData, toSize);

    u::print("[cache] => wrote %.50s... %s (compressed %s to %s with %s compressor)",
        u::fixPath(cacheString),
        cacheFormat(internal),
        // TODO: calculate 33% more for mip levels
        u::sizeMetric(texSize),
        u::sizeMetric(compressedSize),
        from
    );

    if (dxt && dxtOptimCount) {
        const float blockCount = (compressedWidth / 4.0f) * (compressedHeight / 4.0f);
        const float blockDifference = blockCount - dxtOptimCount;
        const float blockPercent = (blockDifference / blockCount) * 100.0f;
        u::print(" (optimized endpoints in %.2f%% of blocks)", blockPercent);
    }
    u::print("\n");

    // Write it out
    return u::write(data, neoUserPath() + cacheString);
}

static bool writeCache(const texture &tex, GLuint internal, GLuint handle, size_t mips) {
    if (!r_tex_compress_cache)
        return false;

    // Don't cache already disk-compressed textures
    if (tex.flags() & kTexFlagCompressed)
        return false;

    // Only cache compressed textures
    switch (internal) {
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_RED_RGTC1_EXT:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
        break;
    default:
        return false;
    }

    // Some drivers just don't do online compression
    GLint compressed = 0;
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compressed);
    if (compressed == 0)
        return false;

    // Don't bother caching if we already have it
    const u::string cacheString = u::format("cache/textures/%s", tex.hashString());
    const u::string file = neoUserPath() + cacheString;
    if (u::exists(file))
        return false;

    // Query the compressed texture size
    gl::BindTexture(GL_TEXTURE_2D, handle);

    size_t mipWidth = tex.width();
    size_t mipHeight = tex.height();

    size_t totalSize = 0;
    size_t totalMips = 0;
    for (size_t i = 0; i < mips; i++, totalMips++) {
        GLint size;
        gl::GetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        totalSize += size;
        if (u::max(mipWidth, mipHeight) <= 1)
            break;
        mipWidth = u::max(mipWidth >> 1, 1_z);
        mipHeight = u::max(mipHeight >> 1, 1_z);
    }

    // Query the compressed height and width (driver may add padding)
    GLint compressedWidth;
    GLint compressedHeight;
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &compressedWidth);
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &compressedHeight);

    u::vector<unsigned char> compressedData(totalSize);
    unsigned char *dest = &compressedData[0];
    for (size_t i = 0; i < totalMips; i++) {
        GLint size;
        gl::GetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        gl::GetCompressedTexImage(GL_TEXTURE_2D, i, dest);
        dest += size;
    }

    return writeCacheData(tex.format(), tex.size(), cacheString, &compressedData[0],
        compressedWidth, compressedHeight, compressedData.size(), totalMips, internal);
}

struct queryFormat {
    constexpr queryFormat()
        : format(0)
        , data(0)
        , internal(0)
    {
    }

    constexpr queryFormat(GLenum format, GLenum data, GLenum internal)
        : format(format)
        , data(data)
        , internal(internal)
    {
    }

    GLenum format;
    GLenum data;
    GLenum internal;
};

static size_t textureAlignment(const texture &tex) {
    const unsigned char *data = tex.data();
    const size_t width = tex.width();
    const size_t bpp = tex.bpp();
    const size_t address = size_t(data) | (width * bpp);
    if (address & 1) return 1;
    if (address & 2) return 2;
    if (address & 4) return 4;
    return 8;
}

// Given a source texture the following function finds the best way to present
// that texture to the hardware. This function will also favor texture compression
// if the hardware supports it by converting the texture if it needs to.
static u::optional<queryFormat> getBestFormat(texture &tex) {
    auto checkSupport = [](size_t what) {
        if (!gl::has(what))
            neoFatal("No support for `%s'", gl::extensionString(what));
    };

    // The texture is compressed?
    if (tex.flags() & kTexFlagCompressed) {
        switch (tex.format()) {
        case kTexFormatDXT1:
            checkSupport(gl::EXT_texture_compression_s3tc);
            return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT };
        case kTexFormatDXT3:
            checkSupport(gl::EXT_texture_compression_s3tc);
            return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT };
        case kTexFormatDXT5:
            checkSupport(gl::EXT_texture_compression_s3tc);
            return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT };
        case kTexFormatBC4U:
            checkSupport(gl::EXT_texture_compression_rgtc);
            return queryFormat { GL_RED, R_TEX_DATA_LUMINANCE, GL_COMPRESSED_RED_RGTC1_EXT };
        case kTexFormatBC4S:
            checkSupport(gl::EXT_texture_compression_rgtc);
            return queryFormat { GL_RED, R_TEX_DATA_LUMINANCE, GL_COMPRESSED_SIGNED_RED_RGTC1_EXT };
        case kTexFormatBC5U:
            checkSupport(gl::EXT_texture_compression_rgtc);
            return queryFormat { GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RED_GREEN_RGTC2_EXT };
        case kTexFormatBC5S:
            checkSupport(gl::EXT_texture_compression_rgtc);
            return queryFormat { GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT };
        default:
            break;
        }
        u::unreachable();
    }

    if (tex.flags() & kTexFlagNormal)
        tex.convert<kTexFormatRG>();
    else if (tex.flags() & kTexFlagGrey)
        tex.convert<kTexFormatLuminance>();

    // Texture compression?
    if (r_tex_compress && !(tex.flags() & kTexFlagNoCompress)) {
        const bool bptc = gl::has(gl::ARB_texture_compression_bptc);
        const bool s3tc = gl::has(gl::EXT_texture_compression_s3tc);
        const bool rgtc = gl::has(gl::EXT_texture_compression_rgtc);
        const bool es3c = gl::has(gl::ARB_ES3_compatibility);
        // Deal with conversion from BGR[A] space to RGB[A] space for compression
        // While falling through to the correct internal format for the compression
        if (bptc || s3tc || rgtc || es3c) {
            switch (tex.format()) {
            case kTexFormatBGRA:
                tex.convert<kTexFormatRGBA>();
            case kTexFormatRGBA:
                if (bptc) return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB };
                if (s3tc) return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT };
                if (es3c) return queryFormat { GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA8_ETC2_EAC };
            break;
            case kTexFormatBGR:
                tex.convert<kTexFormatRGB>();
            case kTexFormatRGB:
                if (bptc) return queryFormat { GL_RGB,R_TEX_DATA_RGB, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB };
                if (s3tc) return queryFormat { GL_RGB,R_TEX_DATA_RGB, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT };
                if (es3c) return queryFormat { GL_RGB,R_TEX_DATA_RGB, GL_COMPRESSED_RGB8_ETC2 };
                break;
            case kTexFormatRG:
                if (es3c) return queryFormat { GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RG11_EAC };
                if (rgtc) return queryFormat { GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RED_GREEN_RGTC2_EXT };
                break;
            case kTexFormatLuminance:
                if (es3c) return queryFormat { GL_RED,R_TEX_DATA_LUMINANCE, GL_COMPRESSED_R11_EAC };
                if (rgtc) return queryFormat { GL_RED,R_TEX_DATA_LUMINANCE, GL_COMPRESSED_RED_RGTC1_EXT };
                break;
            default:
                break;
            }
        }
    }

    // If we made it here then no compression is possible so use a raw internal
    // format.
    switch (tex.format()) {
    case kTexFormatRGBA:
        return queryFormat { GL_RGBA, R_TEX_DATA_RGBA,GL_RGBA };
    case kTexFormatRGB:
        return queryFormat { GL_RGB, R_TEX_DATA_RGB, GL_RGBA };
    case kTexFormatBGRA:
        return queryFormat { GL_BGRA, R_TEX_DATA_BGRA,GL_RGBA };
    case kTexFormatBGR:
        return queryFormat { GL_BGR, R_TEX_DATA_BGR, GL_RGBA };
    case kTexFormatRG:
        return queryFormat { GL_RG, R_TEX_DATA_RG, GL_RG8 };
    case kTexFormatLuminance:
        return queryFormat { GL_RED, R_TEX_DATA_LUMINANCE, GL_RED };
    default:
        u::unreachable();
        break;
    }
    return u::none;
}

texture2D::texture2D(bool mipmaps, int filter)
    : m_uploaded(false)
    , m_textureHandle(0)
    , m_mipmaps(mipmaps ? 1 : 0)
    , m_filter(filter)
    , m_memory(0)
{
    //
}

texture2D::texture2D(texture &tex, bool mipmaps, int filter)
    : texture2D::texture2D(mipmaps, filter)
{
    m_texture = u::move(tex);
}

texture2D::~texture2D() {
    if (m_uploaded)
        gl::DeleteTextures(1, &m_textureHandle);
}

bool texture2D::useCache() {
    GLuint internalFormat = 0;
    if (!readCache(m_texture, internalFormat))
        return false;

    size_t offset = 0;
    size_t mipWidth = m_texture.width();
    size_t mipHeight = m_texture.height();
    size_t blockSize = 0;

    switch (internalFormat) {
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_RG11_EAC:
        blockSize = 16;
        break;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_RED_RGTC1_EXT:
        blockSize = 8;
        break;
    }

    // Load all mip levels
    m_memory = 0;
    for (size_t i = 0; i < m_mipmaps; i++) {
        const size_t mipSize = ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * blockSize;
        gl::CompressedTexImage2D(GL_TEXTURE_2D, i, internalFormat, mipWidth,
            mipHeight, 0, mipSize, m_texture.data() + offset);
        mipWidth = u::max(mipWidth >> 1, 1_z);
        mipHeight = u::max(mipHeight >> 1, 1_z);
        offset += mipSize;
        m_memory += mipSize;
    }

    return true;
}

void texture2D::colorize(uint32_t color) {
    m_texture.colorize(color);
}

static inline void getTexParams(bool bilinear, bool mipmaps, bool trilinear, GLenum &min, GLenum &mag) {
    const unsigned char index = bilinear | (mipmaps << 1) | (trilinear << 2);

    mag = (index & 1) ? GL_LINEAR : GL_NEAREST;

    static const GLenum kMinLookup[] = {
        GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
        GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
    };

    u::assert(index < sizeof kMinLookup / sizeof *kMinLookup);
    min = kMinLookup[index];
}

void texture2D::applyFilter() {
    const bool aniso = r_aniso && (m_filter & kFilterAniso);
    const bool bilinear = r_bilinear && (m_filter & kFilterBilinear);
    const bool trilinear = r_trilinear && (m_filter & kFilterTrilinear);

    GLenum min = GL_NEAREST;
    GLenum mag = GL_NEAREST;
    getTexParams(bilinear, m_mipmaps, trilinear, min, mag);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    if (aniso)
        gl::TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, float(r_aniso));
}

bool texture2D::cache(GLuint internal) {
    return writeCache(m_texture, internal, m_textureHandle, m_mipmaps);
}

bool texture2D::load(const u::string &file, bool preserveQuality) {
    const bool status = m_texture.load(file, preserveQuality ? 1.0f : r_texquality);
    if (status) {
        m_mipmaps = u::log2(u::max(m_texture.width(), m_texture.height())) + 1;
        return true;
    }
    return false;
}

bool texture2D::load(const u::string &file) {
    return load(file, false);
}

bool texture2D::upload() {
    if (m_uploaded)
        return true;

    gl::GenTextures(1, &m_textureHandle);
    gl::BindTexture(GL_TEXTURE_2D, m_textureHandle);

    // If the texture is compressed on disk then load it in ignoring cache
    if (m_texture.flags() & kTexFlagCompressed) {
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            (m_texture.mips() > 1 && r_mipmaps) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        size_t offset = 0;
        size_t mipWidth = m_texture.width();
        size_t mipHeight = m_texture.height();
        size_t blockSize = 0;

        switch (m_texture.format()) {
        case kTexFormatDXT1:
        case kTexFormatBC4U:
        case kTexFormatBC4S:
            blockSize = 8;
            break;
        case kTexFormatDXT3:
        case kTexFormatDXT5:
        case kTexFormatBC5U:
        case kTexFormatBC5S:
            blockSize = 16;
            break;
        default:
            return false;
        }

        auto query = getBestFormat(m_texture);
        if (!query)
            return false;

        queryFormat format = *query;

        // Load all mip levels
        m_memory = 0;
        for (size_t i = 0; i < m_mipmaps; i++) {
            const size_t mipSize = ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * blockSize;
            gl::CompressedTexImage2D(GL_TEXTURE_2D, i, format.internal, mipWidth,
                mipHeight, 0, mipSize, m_texture.data() + offset);
            mipWidth = u::max(mipWidth >> 1, 1_z);
            mipHeight = u::max(mipHeight >> 1, 1_z);
            offset += mipSize;
            m_memory += mipSize;
        }

        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    } else {
        queryFormat format;
        bool needsCache = !useCache();
        if (needsCache) {
            auto query = getBestFormat(m_texture);
            if (!query)
                return false;
            format = *query;

#if defined(DXT_COMPRESSOR)
            // Use our DXT compressor instead of the driver
            if (r_dxt_compressor &&
                (format.internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
                 format.internal == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT))
            {
                needsCache = false;
                u::vector<unsigned char> compressed;
                texture resize = m_texture;
                size_t mipWidth = m_texture.width();
                size_t mipHeight = m_texture.height();
                for (size_t i = 0; i < m_mipmaps; i++) {
                    const size_t size = compressed.size();
                    if (format.internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
                        const auto data = dxtCompress<kDXT1>(resize.data(), mipWidth, mipHeight, m_texture.bpp());
                        compressed.resize(size + data.size());
                        memcpy(&compressed[size], &data[0], data.size());
                    } else {
                        const auto data = dxtCompress<kDXT5>(resize.data(), mipWidth, mipHeight, m_texture.bpp());
                        compressed.resize(size + data.size());
                        memcpy(&compressed[size], &data[0], data.size());
                    }
                    mipWidth = u::max(mipWidth >> 1, 1_z);
                    mipHeight = u::max(mipHeight >> 1, 1_z);
                    resize.resize(mipWidth, mipHeight);
                }

                // Write cache data
                const u::string cacheString = u::format("cache/textures/%s",
                                                        m_texture.hashString());
                if (!writeCacheData(m_texture.format(),
                                    m_texture.size(),
                                    u::fixPath(cacheString),
                                    &compressed[0],
                                    m_texture.width(),
                                    m_texture.height(),
                                    compressed.size(),
                                    m_mipmaps,
                                    format.internal,
                                    "our") || !useCache())
                {
                    neoFatal("failed to cache");
                }
            }
            else
#endif
            {
                if (r_mipmaps) {
                    texture resizing = m_texture;
                    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_mipmaps);
                    for (unsigned char i = 0; i < m_mipmaps; i++) {
                        gl::PixelStorei(GL_UNPACK_ALIGNMENT, textureAlignment(resizing));
                        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, resizing.pitch() / resizing.bpp());
                        gl::TexImage2D(GL_TEXTURE_2D, i, format.internal, resizing.width(),
                            resizing.height(), 0, format.format, format.data, resizing.data());
                        const size_t width = u::max(resizing.width() >> 1, 1_z);
                        const size_t height = u::max(resizing.height() >> 1, 1_z);
                        resizing.resize(width, height);
                    }
                } else {
                    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                    gl::PixelStorei(GL_UNPACK_ALIGNMENT, textureAlignment(m_texture));
                    gl::PixelStorei(GL_UNPACK_ROW_LENGTH, m_texture.pitch() / m_texture.bpp());
                    gl::TexImage2D(GL_TEXTURE_2D, 0, format.internal, m_texture.width(),
                        m_texture.height(), 0, format.format, format.data, m_texture.data());
                }
                gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                gl::PixelStorei(GL_UNPACK_ALIGNMENT, 8);

                static const GLuint kCompressedFormats[] = {
                    GL_COMPRESSED_RGBA_BPTC_UNORM_ARB,
                    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                    GL_COMPRESSED_RGBA8_ETC2_EAC,
                    GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB,
                    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                    GL_COMPRESSED_RGB8_ETC2,
                    GL_COMPRESSED_RG11_EAC,
                    GL_COMPRESSED_RED_GREEN_RGTC2_EXT,
                    GL_COMPRESSED_R11_EAC,
                    GL_COMPRESSED_RED_RGTC1_EXT
                };

                for (const auto &it : kCompressedFormats) {
                    if (format.internal != it)
                        continue;
                    needsCache = false;
                    cache(format.internal);
                    if (!useCache())
                        neoFatal("failed to cache");
                    break;
                }
            }
        }

        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        applyFilter();

        if (needsCache)
            cache(format.internal);
    }

    m_texture.unload();
    return m_uploaded = true;
}

void texture2D::bind(GLenum unit) {
    gl::ActiveTexture(unit);
    gl::BindTexture(GL_TEXTURE_2D, m_textureHandle);
}

void texture2D::resize(size_t width, size_t height) {
    m_texture.resize(width, height);
}

textureFormat texture2D::format() const {
  return m_texture.format();
}

///! texture3D
texture3D::texture3D() :
    m_uploaded(false),
    m_textureHandle(0)
{

}

texture3D::~texture3D() {
    if (m_uploaded)
        gl::DeleteTextures(1, &m_textureHandle);
}

void texture3D::applyFilter() {
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Anisotropic filtering
    if (r_aniso)
        gl::TexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, float(r_aniso));
}

bool texture3D::load(const u::string &ft, const u::string &bk, const u::string &up,
                     const u::string &dn, const u::string &rt, const u::string &lf)
{
    if (!m_textures[kFront].load(ft, r_texquality)) return false;
    if (!m_textures[kBack].load(bk, r_texquality)) return false;
    if (!m_textures[kUp].load(up, r_texquality)) return false;
    if (!m_textures[kDown].load(dn, r_texquality)) return false;
    if (!m_textures[kRight].load(rt, r_texquality)) return false;
    if (!m_textures[kLeft].load(lf, r_texquality)) return false;
    return true;
}

bool texture3D::upload() {
    if (m_uploaded)
        return true;

    gl::GenTextures(1, &m_textureHandle);
    gl::BindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    applyFilter();

    // find the largest texture in the cubemap and scale all others to it
    size_t mw = 0;
    size_t mh = 0;
    for (size_t i = 0; i < 6; i++) {
        const size_t w = m_textures[i].width();
        const size_t h = m_textures[i].height();
        if (w*h > mw*mh) {
            mw = w;
            mh = h;
        }
    }

    for (size_t i = 0; i < 6; i++) {
        if (m_textures[i].width() != mw || m_textures[i].height() != mh)
            m_textures[i].resize(mw, mh);
        auto query = getBestFormat(m_textures[i]);
        if (!query)
            return false;
        const auto &format = *query;
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, textureAlignment(m_textures[i]));
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, m_textures[i].pitch() / m_textures[i].bpp());
        gl::TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            format.internal, mw, mh, 0, format.format, format.data, m_textures[i].data());
    }
    gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 8);

    return m_uploaded = true;
}

void texture3D::bind(GLenum unit) {
    gl::ActiveTexture(unit);
    gl::BindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);
}

void texture3D::resize(size_t width, size_t height) {
    for (size_t i = 0; i < 6; i++)
        m_textures[i].resize(width, height);
}

const texture &texture3D::get(int direction) const {
    return m_textures[direction];
}

}
