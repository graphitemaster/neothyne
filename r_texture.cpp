#include "r_texture.h"

#include "u_file.h"
#include "u_algorithm.h"

#include "engine.h"

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

struct textureDXTCacheHeader {
    size_t width;
    size_t height;
    // Original format (RGB or RGBA) == (DXT1 or DXT5)
    textureFormat format;
};

static bool readDXTCache(texture &tex) {
    // Do we even have it in cache?
    const u::string file = neoPath() + "cache/" + tex.hashString();
    if (!u::exists(file))
        return false;

    // Found it in cache, unload the current texture and load the cache from disk.
    auto load = u::read(file, "rb");
    if (!load)
        return false;

    // Parse header
    auto vec = *load;
    textureDXTCacheHeader head;
    memcpy(&head, &vec[0], sizeof(head));

    const unsigned char *data = &vec[0] + sizeof(head);
    const size_t length = vec.size() - sizeof(head);

    // Now swap!
    tex.unload();
    tex.from(data, length, head.width, head.height, false, head.format);
    return true;
}

static bool writeDXTCache(const texture &tex, GLuint handle) {
    // Don't bother caching if we already have it
    const u::string file = neoPath() + "cache/" + tex.hashString();
    if (u::exists(file))
        return false;

    // Build the header
    textureDXTCacheHeader head;
    head.width = tex.width();
    head.height = tex.height();
    head.format = tex.format();

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
    return u::write(data, file);
}

struct queryFormat {
    constexpr queryFormat(GLenum format, GLenum data, GLenum internal) :
        format(format),
        data(data),
        internal(internal)
    { }
    GLenum format;
    GLenum data;
    GLenum internal;
};

// Given a source texture the following function finds the best way to present
// that texture to the hardware. This function will also favor texture compression
// if the hardware supports it by converting the texture if it needs to.
static u::optional<queryFormat> getBestFormat(texture &tex) {
    textureFormat format = tex.format();

    const bool s3tc = gl::has("GL_EXT_texture_compression_s3tc") && !tex.normal();

    // Convert the textures to a format S3TC texture compression supports if
    // the hardware supports S3TC compression
    if (s3tc) {
        if (format == TEX_BGRA)
            tex.convert<TEX_RGBA>();
        else if (format == TEX_BGR)
            tex.convert<TEX_RGB>();
    }

    switch (format) {
        case TEX_RGBA:
            return queryFormat(GL_RGBA, R_TEX_DATA_RGBA, s3tc ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_RGBA);
        case TEX_RGB:
            return queryFormat(GL_RGB, GL_UNSIGNED_BYTE, s3tc ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_RGBA);
        case TEX_BGRA:
            return queryFormat(GL_BGRA, R_TEX_DATA_BGRA, GL_RGBA);
        case TEX_BGR:
            return queryFormat(GL_BGR, GL_UNSIGNED_BYTE, GL_RGBA);
        case TEX_LUMINANCE:
            return queryFormat(GL_RED, GL_UNSIGNED_BYTE, GL_RED);
    }
    return u::none;
}

texture2D::texture2D(void) :
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

texture2D::~texture2D(void) {
    if (m_uploaded)
        gl::DeleteTextures(1, &m_textureHandle);
}

bool texture2D::useDXTCache(void) {
    if (m_texture.normal())
        return false;
    if (!gl::has("GL_EXT_texture_compression_s3tc"))
        return false;
    if (!readDXTCache(m_texture))
        return false;
    switch (m_texture.format()) {
        case TEX_RGB:
            gl::CompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                m_texture.width(), m_texture.height(), 0, m_texture.size(), m_texture.data());
            break;
        case TEX_RGBA:
            gl::CompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                m_texture.width(), m_texture.height(), 0, m_texture.size(), m_texture.data());
            break;
        default:
            neoFatal("malformatted DXT cache");
    }
    return true;
}

bool texture2D::cacheDXT(void) {
    if (m_texture.normal())
        return false;
    if (gl::has("GL_EXT_texture_compression_s3tc"))
        return writeDXTCache(m_texture, m_textureHandle);
    return false;
}

bool texture2D::load(const u::string &file) {
    return m_texture.load(file);
}

bool texture2D::upload(void) {
    if (m_uploaded)
        return true;

    gl::GenTextures(1, &m_textureHandle);
    gl::BindTexture(GL_TEXTURE_2D, m_textureHandle);

    bool useCache = useDXTCache();
    if (!useCache) {
        auto query = getBestFormat(m_texture);
        if (!query)
            return false;
        const auto format = *query;
        gl::TexImage2D(GL_TEXTURE_2D, 0, format.internal, m_texture.width(),
            m_texture.height(), 0, format.format, format.data, m_texture.data());
    }

    gl::GenerateMipmap(GL_TEXTURE_2D);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    if (!useCache)
        cacheDXT();

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

texture3D::~texture3D(void) {
    if (m_uploaded)
        gl::DeleteTextures(1, &m_textureHandle);
}

bool texture3D::load(const u::string &ft, const u::string &bk, const u::string &up,
                     const u::string &dn, const u::string &rt, const u::string &lf)
{
    if (!m_textures[0].load(ft)) return false;
    if (!m_textures[1].load(bk)) return false;
    if (!m_textures[2].load(up)) return false;
    if (!m_textures[3].load(dn)) return false;
    if (!m_textures[4].load(rt)) return false;
    if (!m_textures[5].load(lf)) return false;
    return true;
}

bool texture3D::upload(void) {
    if (m_uploaded)
        return true;

    gl::GenTextures(1, &m_textureHandle);
    gl::BindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);

    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

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
