#ifndef R_GUI_HDR
#define R_GUI_HDR
#include "r_method.h"
#include "r_texture.h"
#include "r_pipeline.h"

#include "m_mat4.h"

#include "u_map.h"

#include "gui.h"

namespace r {

struct guiMethod : method {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setPerspectiveProjection(const m::perspectiveProjection &project);
    void setColorTextureUnit(int unit);

private:
    GLint m_screenSizeLocation;
    GLint m_colorMapLocation;
};

struct gui {
    gui();
    ~gui();
    bool load(const u::string &font);
    bool upload();
    void render(const rendererPipeline &p);

protected:
    template <size_t E>
    void drawPolygon(const float (&coords)[E], float r, uint32_t color);
    void drawRectangle(float x, float y, float w, float h, float fth, uint32_t color);
    void drawRectangle(float x, float y, float w, float h, float r, float fth, uint32_t color);
    void drawLine(float x0, float y0, float x1, float y1, float r, float fth, uint32_t color);
    void drawText(float x, float y, const u::string &contents, int align, uint32_t color);
    void drawImage(float x, float y, float w, float h, const u::string &path);

private:
    struct glyphQuad {
        float x0, y0;
        float x1, y1;
        float s0, s1;
        float t0, t1;
    };

public:
    glyphQuad getGlyphQuad(int pw, int ph, size_t index, float &xpos, float &ypos);

private:
    enum {
        kMethodNormal,
        kMethodFont,
        kMethodImage
    };

    struct glyph {
        int x0, y0;
        int x1, y1;
        float xoff;
        float yoff;
        float xadvance;
    };

    struct vertex {
        float x, y;
        float s, t;
        float r, g, b, a;
    };

    u::vector<glyph> m_glyphs;

    static constexpr size_t kCoordCount = 100;
    static constexpr size_t kCircleVertices = 8 * 4;

    float m_coords[kCoordCount * 2];
    float m_normals[kCoordCount * 2];
    float m_circleVertices[kCircleVertices * 2];

    GLuint m_vbo;
    GLuint m_vao;
    u::map<u::string, texture2D*> m_textures;
    texture2D m_font;
    texture2D m_notex;
    guiMethod m_methods[3];
};

}

#endif
