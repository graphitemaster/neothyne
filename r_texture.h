#ifndef R_TEXTURE_HDR
#define R_TEXTURE_HDR
#include "texture.h"
#include "r_common.h"

namespace r {

struct texture2D {
    texture2D();
    ~texture2D();

    texture2D(texture &tex);

    bool load(const u::string &file);
    bool upload();
    bool cache(GLuint internal);
    void bind(GLenum unit);
    void resize(size_t width, size_t height);

private:
    bool useCache();
    void applyFilter();
    bool m_uploaded;
    GLuint m_textureHandle;
    texture m_texture;
};

struct texture3D {
    texture3D();
    ~texture3D();

    bool load(const u::string &ft, const u::string &bk, const u::string &up,
              const u::string &dn, const u::string &rt, const u::string &lf);
    bool upload();
    void bind(GLenum unit);
    void resize(size_t width, size_t height);

private:
    void applyFilter();
    bool m_uploaded;
    GLuint m_textureHandle;
    texture m_textures[6];
};

}

#endif
