#include "r_texture.h"

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

static void getTextureFormat(const texture &tex, GLenum &tf, GLenum &df) {
    switch (tex.format()) {
        case TEX_RGBA:
            tf = GL_RGBA;
            df = R_TEX_DATA_RGBA;
            break;
        case TEX_BGRA:
            tf = GL_BGRA;
            df = R_TEX_DATA_BGRA;
            break;
        case TEX_RGB:
            tf = GL_RGB;
            df = GL_UNSIGNED_BYTE;
            break;
        case TEX_BGR:
            tf = GL_BGR;
            df = GL_UNSIGNED_BYTE;
            break;
        default:
            // to silent possible uninitialized warnings
            tf = 0;
            df = GL_UNSIGNED_BYTE;
            break;
    }
}

texture2D::texture2D(void) :
    m_uploaded(false),
    m_textureHandle(0)
{
    //
}

texture2D::~texture2D(void) {
    if (m_uploaded)
        gl::DeleteTextures(1, &m_textureHandle);
}

bool texture2D::load(const u::string &file) {
    return m_texture.load(file);
}

bool texture2D::upload(void) {
    gl::GenTextures(1, &m_textureHandle);
    gl::BindTexture(GL_TEXTURE_2D, m_textureHandle);

    GLenum tf;
    GLenum df;
    getTextureFormat(m_texture, tf, df);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_texture.width(),
        m_texture.height(), 0, tf, df, m_texture.data());

    gl::GenerateMipmap(GL_TEXTURE_2D);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    return m_uploaded = true;
}

void texture2D::bind(GLenum unit) {
    gl::ActiveTexture(unit);
    gl::BindTexture(GL_TEXTURE_2D, m_textureHandle);
}

void texture2D::resize(size_t width, size_t height) {
    m_texture.resize(width, height);
}

///! textureCubemap
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
        GLuint tf, df;
        getTextureFormat(m_textures[i], tf, df);
        gl::TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, fw, fh, 0, tf, df, m_textures[i].data());
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
