#include <assert.h>

#include "engine.h"
#include "cvar.h"

#include "r_texture.h"

#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"

VAR(int, r_texcomp, "texture compression", 0, 1, 1);
VAR(int, r_texcompcache, "cache compressed textures", 0, 1, 1);
VAR(int, r_aniso, "anisotropic filtering", 0, 1, 1);
VAR(int, r_bilinear, "bilinear filtering", 0, 1, 1);
VAR(int, r_trilinear, "trilinear filtering", 0, 1, 1);
VAR(int, r_mipmaps, "mipmaps", 0, 1, 1);
VAR(int, r_dxt_optimize, "DXT endpoints optimization", 0, 1, 1);
VAR(float, r_texquality, "texture quality", 0.0f, 1.0f, 1.0f);

namespace r {

#ifdef GL_UNSIGNED_INT_8_8_8_8_REV
#   define R_TEX_DATA_RGBA GL_UNSIGNED_INT_8_8_8_8_REV
#else
#   define R_TEX_DATA_RGBA GL_UNSINGED_BYTE
#endif
#ifdef GL_UNSIGNED_INT_8_8_8_8
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

static const unsigned char kTextureCacheVersion = 0x02;

struct textureCacheHeader {
    unsigned char version;
    size_t width;
    size_t height;
    GLuint internal;
    textureFormat format;
};

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
    }
    return "";
}

static u::string sizeMetric(size_t size) {
    static const char *sizes[] = { "B", "kB", "MB", "GB" };
    size_t r = 0;
    size_t i = 0;
    for (; size >= 1024 && i < sizeof(sizes)/sizeof(*sizes); i++) {
        r = size % 1024;
        size /= 1024;
    }
    assert(i != sizeof(sizes)/sizeof(*sizes));
    return u::format("%.2f %s", float(size) + float(r) / 1024.0f, sizes[i]);
}

static bool readCache(texture &tex, GLuint &internal) {
    if (!r_texcomp)
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
    const u::string cacheString = u::format("cache%c%s", PATH_SEP, tex.hashString());
    const u::string file = neoUserPath() + cacheString;
    if (!u::exists(file))
        return false;

    // Found it in cache, unload the current texture and load the cache from disk.
    auto load = u::read(file, "rb");
    if (!load)
        return false;

    // Parse header
    auto vec = *load;
    textureCacheHeader head;
    memcpy(&head, &vec[0], sizeof(head));
    if (head.version != kTextureCacheVersion) {
        u::remove(file);
        return false;
    }
    head.width = u::endianSwap(head.width);
    head.height = u::endianSwap(head.height);
    head.internal = u::endianSwap(head.internal);
    head.format = u::endianSwap(head.format);

    // Make sure we even support the format before using it
    switch (head.internal) {
        case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
            if (!gl::has(ARB_texture_compression_bptc))
                return false;
            break;

        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            if (!gl::has(EXT_texture_compression_s3tc))
                return false;
            break;

        case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
        case GL_COMPRESSED_RED_RGTC1_EXT:
            if (!gl::has(EXT_texture_compression_rgtc))
                return false;
            break;
    }

    const unsigned char *data = &vec[0] + sizeof(head);
    const size_t length = vec.size() - sizeof(head);

    internal = head.internal;

    // Now swap!
    tex.unload();
    tex.from(data, length, head.width, head.height, false, head.format);
    u::print("[cache] => read %.50s... %s (%s)\n", cacheString,
        cacheFormat(head.internal), sizeMetric(length));
    return true;
}

static bool writeCache(const texture &tex, GLuint internal, GLuint handle) {
    if (!r_texcompcache)
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
    const u::string cacheString = u::format("cache%c%s", PATH_SEP, tex.hashString());
    const u::string file = neoUserPath() + cacheString;
    if (u::exists(file))
        return false;

    // Query the compressed texture size
    gl::BindTexture(GL_TEXTURE_2D, handle);
    GLint compressedSize;
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0,
        GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressedSize);

    // Query the compressed height and width (driver may add padding)
    GLint compressedWidth;
    GLint compressedHeight;
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &compressedWidth);
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &compressedHeight);

    // Build the header
    textureCacheHeader head;
    head.version = kTextureCacheVersion;
    head.width = u::endianSwap(compressedWidth);
    head.height = u::endianSwap(compressedHeight);
    head.internal = u::endianSwap(internal);
    head.format = u::endianSwap(tex.format());

    // Prepare the data
    u::vector<unsigned char> data;
    data.resize(sizeof(head) + compressedSize);
    memcpy(&data[0], &head, sizeof(head));

    // Read the compressed image
    gl::GetCompressedTexImage(GL_TEXTURE_2D, 0, &data[0] + sizeof(head));

    // Apply DXT optimizations if we can
    const bool dxt = internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
                     internal == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

    size_t dxtOptimCount = 0;
    if (r_dxt_optimize && dxt) {
        dxtOptimCount = (internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
            ? dxtOptimize<kDXT1>(&data[0] + sizeof(head), compressedWidth, compressedHeight)
            : dxtOptimize<kDXT5>(&data[0] + sizeof(head), compressedWidth, compressedHeight);
    }

    u::print("[cache] => wrote %.50s... %s (compressed %s to %s)",
        cacheString,
        cacheFormat(head.internal),
        sizeMetric(tex.size()),
        sizeMetric(compressedSize)
    );

    if (dxt && dxtOptimCount) {
        const float blockCount = (compressedWidth / 4.0f) * (compressedHeight / 4.0f);
        const float blockDifference = blockCount - dxtOptimCount;
        const float blockPercent = (blockDifference / blockCount) * 100.0f;
        u::print(" (optimized endpoints in %.2f%% of blocks)",
            sizeMetric(tex.size()),
            sizeMetric(compressedSize),
            blockPercent
        );
    }
    u::print("\n");

    // Write it to disk!
    return u::write(data, file);
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
                checkSupport(EXT_texture_compression_s3tc);
                return queryFormat(GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
            case kTexFormatDXT3:
                checkSupport(EXT_texture_compression_s3tc);
                return queryFormat(GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
            case kTexFormatDXT5:
                checkSupport(EXT_texture_compression_s3tc);
                return queryFormat(GL_RGBA, R_TEX_DATA_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
            case kTexFormatBC4U:
                checkSupport(EXT_texture_compression_rgtc);
                return queryFormat(GL_RED, R_TEX_DATA_LUMINANCE, GL_COMPRESSED_RED_RGTC1_EXT);
            case kTexFormatBC4S:
                checkSupport(EXT_texture_compression_rgtc);
                return queryFormat(GL_RED, R_TEX_DATA_LUMINANCE, GL_COMPRESSED_SIGNED_RED_RGTC1_EXT);
            case kTexFormatBC5U:
                checkSupport(EXT_texture_compression_rgtc);
                return queryFormat(GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
            case kTexFormatBC5S:
                checkSupport(EXT_texture_compression_rgtc);
                return queryFormat(GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT);
            default:
                break;
        }
        assert(0);
    }

    if (tex.flags() & kTexFlagNormal)
        tex.convert<kTexFormatRG>();
    else if (tex.flags() & kTexFlagGrey)
        tex.convert<kTexFormatLuminance>();

    // Texture compression?
    if (r_texcomp && !(tex.flags() & kTexFlagNoCompress)) {
        const bool bptc = gl::has(ARB_texture_compression_bptc);
        const bool s3tc = gl::has(EXT_texture_compression_s3tc);
        const bool rgtc = gl::has(EXT_texture_compression_rgtc);
        // Deal with conversion from BGR[A] space to RGB[A] space for compression
        // While falling through to the correct internal format for the compression
        if (bptc || s3tc || rgtc) {
            switch (tex.format()) {
                case kTexFormatBGRA:
                    tex.convert<kTexFormatRGBA>();
                case kTexFormatRGBA:
                    return queryFormat(GL_RGBA, R_TEX_DATA_RGBA,
                        bptc ? GL_COMPRESSED_RGBA_BPTC_UNORM_ARB : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
                case kTexFormatBGR:
                    tex.convert<kTexFormatRGB>();
                case kTexFormatRGB:
                    if (bptc)
                        return queryFormat(GL_RGB, R_TEX_DATA_RGB, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB);
                    // Extend RGB->RGBA for DXT1
                    return queryFormat(GL_RGB, R_TEX_DATA_RGB, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
                case kTexFormatRG:
                    return queryFormat(GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
                case kTexFormatLuminance:
                    return queryFormat(GL_RED, R_TEX_DATA_LUMINANCE, GL_COMPRESSED_RED_RGTC1_EXT);
                default:
                    break;
            }
        }
    }

    // If we made it here then no compression is possible so use a raw internal
    // format.
    switch (tex.format()) {
        case kTexFormatRGBA:
            return queryFormat(GL_RGBA, R_TEX_DATA_RGBA,      GL_RGBA);
        case kTexFormatRGB:
            return queryFormat(GL_RGB,  R_TEX_DATA_RGB,       GL_RGBA);
        case kTexFormatBGRA:
            return queryFormat(GL_BGRA, R_TEX_DATA_BGRA,      GL_RGBA);
        case kTexFormatBGR:
            return queryFormat(GL_BGR,  R_TEX_DATA_BGR,       GL_RGBA);
        case kTexFormatRG:
            return queryFormat(GL_RG,   R_TEX_DATA_RG,        GL_RG8);
        case kTexFormatLuminance:
            return queryFormat(GL_RED,  R_TEX_DATA_LUMINANCE, GL_RED);
        default:
            assert(0);
            break;
    }
    return u::none;
}

texture2D::texture2D(bool mipmaps, int filter)
    : m_uploaded(false)
    , m_textureHandle(0)
    , m_mipmaps(mipmaps)
    , m_filter(filter)
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
    gl::CompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
        m_texture.width(), m_texture.height(), 0, m_texture.size(), m_texture.data());
    return true;
}

void texture2D::applyFilter() {
    if (r_bilinear && (m_filter & kFilterBilinear)) {
        GLenum min = (r_mipmaps && m_mipmaps)
            ? ((r_trilinear && (m_filter & kFilterTrilinear))
                ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR)
                : GL_LINEAR;
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    } else {
        GLenum min = (r_mipmaps && m_mipmaps)
            ? ((r_trilinear && (m_filter & kFilterTrilinear))
                ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR)
                : GL_NEAREST;
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    }

    // Anisotropic filtering
    if ((r_aniso & kFilterAniso) && gl::has(EXT_texture_filter_anisotropic)) {
        GLfloat largest;
        gl::GetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest);
        gl::TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest);
    }
}

bool texture2D::cache(GLuint internal) {
    return writeCache(m_texture, internal, m_textureHandle);
}

bool texture2D::load(const u::string &file) {
    return m_texture.load(file, r_texquality);
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

        if (blockSize != 8 && blockSize != 16)
            return false;

        auto query = getBestFormat(m_texture);
        if (!query)
            return false;

        queryFormat format = *query;

        // Load all mip levels
        for (size_t i = 0; i < m_texture.mips(); i++) {
            size_t mipSize = ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * blockSize;
            gl::CompressedTexImage2D(GL_TEXTURE_2D, i, format.internal, mipWidth,
                mipHeight, 0, mipSize, m_texture.data() + offset);
            mipWidth = u::max(mipWidth >> 1, size_t(1));
            mipHeight = u::max(mipHeight >> 1, size_t(1));
            offset += mipSize;
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
            gl::PixelStorei(GL_UNPACK_ALIGNMENT, textureAlignment(m_texture));
            gl::PixelStorei(GL_UNPACK_ROW_LENGTH, m_texture.pitch() / m_texture.bpp());
            gl::TexImage2D(GL_TEXTURE_2D, 0, format.internal, m_texture.width(),
                m_texture.height(), 0, format.format, format.data, m_texture.data());
            gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            gl::PixelStorei(GL_UNPACK_ALIGNMENT, 8);

            if (format.internal == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
                format.internal == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
            {
                needsCache = false;
                cache(format.internal);
                if (!useCache())
                    neoFatal("failed to cache");
            }
        }

        if (r_mipmaps)
            gl::GenerateMipmap(GL_TEXTURE_2D);
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
    if (r_aniso && gl::has(EXT_texture_filter_anisotropic)) {
        GLfloat largest;
        gl::GetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest);
        gl::TexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest);
    }
}

bool texture3D::load(const u::string &ft, const u::string &bk, const u::string &up,
                     const u::string &dn, const u::string &rt, const u::string &lf)
{
    if (!m_textures[0].load(ft, r_texquality)) return false;
    if (!m_textures[1].load(bk, r_texquality)) return false;
    if (!m_textures[2].load(up, r_texquality)) return false;
    if (!m_textures[3].load(dn, r_texquality)) return false;
    if (!m_textures[4].load(rt, r_texquality)) return false;
    if (!m_textures[5].load(lf, r_texquality)) return false;
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
    size_t mi = 0;
    for (size_t i = 0; i < 6; i++) {
        if (m_textures[i].width() > mw && m_textures[i].height() > mh)
            mi = i;
    }

    const size_t fw = m_textures[mi].width();
    const size_t fh = m_textures[mi].height();
    for (size_t i = 0; i < 6; i++) {
        if (m_textures[i].width() != fw || m_textures[i].height() != fh)
            m_textures[i].resize(fw, fh);
        auto query = getBestFormat(m_textures[i]);
        if (!query)
            return false;
        const auto format = *query;
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, textureAlignment(m_textures[i]));
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, m_textures[i].pitch() / m_textures[i].bpp());
        gl::TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            format.internal, fw, fh, 0, format.format, format.data, m_textures[i].data());
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

}
