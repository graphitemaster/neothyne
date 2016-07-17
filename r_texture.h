#ifndef R_TEXTURE_HDR
#define R_TEXTURE_HDR
#include "texture.h"
#include "r_common.h"

namespace r {

enum {
    kFilterBilinear  = 1 << 0,
    kFilterTrilinear = 1 << 1,
    kFilterAniso     = 1 << 2,
    kFilterDefault   = kFilterBilinear | kFilterTrilinear | kFilterAniso
};

struct texture2D {
    texture2D(bool mipmaps = true, int filter = kFilterDefault);
    ~texture2D();

    texture2D(Texture &tex, bool mipmaps = true, int filter = kFilterDefault);

    void colorize(uint32_t color); // must be called before upload
    bool load(const u::string &file, bool preserveQuality = true, bool mipmaps = true, bool debug = false);
    bool upload(GLint wrap = GL_REPEAT);
    bool cache(GLuint internal);
    void bind(GLenum unit);
    void resize(size_t width, size_t height);
    TextureFormat format() const;
    size_t width() const;
    size_t height() const;
    size_t memory() const;
    const Texture &get() const;

private:
    bool useCache();
    void applyFilter();
    bool m_uploaded;
    GLuint m_textureHandle;
    Texture m_texture;
    size_t m_mipmaps;
    size_t m_memory;
    int m_filter;
};

inline const Texture &texture2D::get() const {
    return m_texture;
}

inline size_t texture2D::width() const {
    return m_texture.width();
}

inline size_t texture2D::height() const {
    return m_texture.height();
}

}

#endif
