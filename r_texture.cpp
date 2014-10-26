#include "r_texture.h"

#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"

#include "c_var.h"

#include "engine.h"

VAR(int, r_texcomp, "texture compression", 0, 1, 1);
VAR(int, r_texcompcache, "cache compressed textures", 0, 1, 1);
VAR(int, r_aniso, "anisotropic filtering", 0, 1, 1);
VAR(int, r_bilinear, "bilinear filtering", 0, 1, 1);
VAR(int, r_trilinear, "trilinear filtering", 0, 1, 1);
VAR(int, r_mipmaps, "mipmaps", 0, 1, 1);
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

static const unsigned char kTextureCacheVersion = 0xFA;

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
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            return "RGB_S3TC_DXT1";
        case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
            return "RED_GREEN_RGTC2";
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
    return u::format("%.2f %s", float(size) + float(r) / 1024.0f, sizes[i]);
}

static bool readCache(texture &tex, GLuint &internal) {
    if (!r_texcomp || !tex.disk())
        return false;

    // Do we even have it in cache?
    const u::string cacheString = "cache/" + tex.hashString();
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
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            if (!gl::has(EXT_texture_compression_s3tc))
                return false;
        case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
            if (!gl::has(EXT_texture_compression_rgtc))
                return false;
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

    // Only cache compressed textures
    switch (internal) {
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
        case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
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
    const u::string cacheString =  "cache/" + tex.hashString();
    const u::string file = neoUserPath() + cacheString;
    if (u::exists(file))
        return false;

    // Build the header
    textureCacheHeader head;
    head.version = kTextureCacheVersion;
    head.width = u::endianSwap(tex.width());
    head.height = u::endianSwap(tex.height());
    head.internal = u::endianSwap(internal);
    head.format = u::endianSwap(tex.format());

    // Query the compressed texture size
    gl::BindTexture(GL_TEXTURE_2D, handle);
    GLint compressedSize;
    gl::GetTexLevelParameteriv(GL_TEXTURE_2D, 0,
        GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressedSize);

    // Prepare the data
    u::vector<unsigned char> data;
    data.resize(sizeof(head) + compressedSize);
    memcpy(&data[0], &head, sizeof(head));

    // Read the compressed image
    gl::GetCompressedTexImage(GL_TEXTURE_2D, 0, &data[0] + sizeof(head));

    // Write it to disk!
    u::print("[cache] => wrote %.50s... %s (compressed %s to %s)\n",
        cacheString,
        cacheFormat(head.internal),
        sizeMetric(tex.size()),
        sizeMetric(compressedSize)
    );
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

// Given a source texture the following function finds the best way to present
// that texture to the hardware. This function will also favor texture compression
// if the hardware supports it by converting the texture if it needs to.
static u::optional<queryFormat> getBestFormat(texture &tex) {
    if (tex.normal())
        tex.convert<TEX_RG>();

    // Texture compression?
    if (r_texcomp) {
        const bool bptc = gl::has(ARB_texture_compression_bptc);
        const bool s3tc = gl::has(EXT_texture_compression_s3tc);
        const bool rgtc = gl::has(EXT_texture_compression_rgtc);
        // Deal with conversion from BGR[A] space to RGB[A] space for compression
        // While falling through to the correct internal format for the compression
        if (bptc || s3tc || rgtc) {
            switch (tex.format()) {
                case TEX_BGRA:
                    tex.convert<TEX_RGBA>();
                case TEX_RGBA:
                    return queryFormat(GL_RGBA, R_TEX_DATA_RGBA,
                        bptc ? GL_COMPRESSED_RGBA_BPTC_UNORM_ARB : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
                case TEX_BGR:
                    tex.convert<TEX_RGB>();
                case TEX_RGB:
                    return queryFormat(GL_RGB, R_TEX_DATA_RGB,
                        bptc ? GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB : GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
                case TEX_RG:
                    return queryFormat(GL_RG, R_TEX_DATA_RG, GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
                default:
                    break;
            }
        }
    }

    // If we made it here then no compression is possible so use a raw internal
    // format.
    switch (tex.format()) {
        case TEX_RGBA:
            return queryFormat(GL_RGBA, R_TEX_DATA_RGBA,      GL_RGBA);
        case TEX_RGB:
            return queryFormat(GL_RGB,  R_TEX_DATA_RGB,       GL_RGBA);
        case TEX_BGRA:
            return queryFormat(GL_BGRA, R_TEX_DATA_BGRA,      GL_RGBA);
        case TEX_BGR:
            return queryFormat(GL_BGR,  R_TEX_DATA_BGR,       GL_RGBA);
        case TEX_RG:
            return queryFormat(GL_RG,   R_TEX_DATA_RG,        GL_RG8);
        case TEX_LUMINANCE:
            return queryFormat(GL_RED,  R_TEX_DATA_LUMINANCE, GL_RED);
    }
    return u::none;
}

texture2D::texture2D() :
    m_uploaded(false),
    m_textureHandle(0)
{
    //
}

texture2D::texture2D(texture &tex) :
    texture2D::texture2D()
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
    if (r_bilinear) {
        GLenum min = r_mipmaps
            ? (r_trilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR)
            : GL_LINEAR;
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    } else {
        GLenum min = r_mipmaps
            ? (r_trilinear ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR)
            : GL_NEAREST;
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    }

    // Anisotropic filtering
    if (r_aniso && gl::has(EXT_texture_filter_anisotropic)) {
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

    queryFormat format;
    bool needsCache = !useCache();
    if (needsCache) {
        auto query = getBestFormat(m_texture);
        if (!query)
            return false;
        format = *query;
        gl::TexImage2D(GL_TEXTURE_2D, 0, format.internal, m_texture.width(),
            m_texture.height(), 0, format.format, format.data, m_texture.data());
    }

    if (r_mipmaps)
        gl::GenerateMipmap(GL_TEXTURE_2D);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    applyFilter();

    if (needsCache)
        cache(format.internal);

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
        gl::TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            format.internal, fw, fh, 0, format.format, format.data, m_textures[i].data());
    }

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
