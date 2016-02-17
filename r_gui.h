#ifndef R_GUI_HDR
#define R_GUI_HDR
#include "r_method.h"
#include "r_texture.h"

#include "u_map.h"
#include "u_optional.h"

namespace m {
    struct perspective;
}

namespace r {

struct model;
struct pipeline;

struct gui {
    gui();
    ~gui();
    bool load(const u::string &font);
    bool upload();
    void render(const pipeline &pl);

protected:
    template <size_t E>
    void drawPolygon(const float (&coords)[E], float r, uint32_t color);
    void drawRectangle(float x, float y, float w, float h, float fth, uint32_t color);
    void drawRectangle(float x, float y, float w, float h, float r, float fth, uint32_t color);
    void drawLine(float x0, float y0, float x1, float y1, float r, float fth, uint32_t color);
    void drawText(float x, float y, const char *contents, int align, uint32_t color);
    void drawImage(float x, float y, float w, float h, const char *path);

private:
    struct glyphQuad {
        float x0, y0;
        float x1, y1;
        float s0, s1;
        float t0, t1;
    };

public:
    u::optional<glyphQuad> getGlyphQuad(int pw, int ph, size_t index, float &xpos, float &ypos);

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

    struct atlas {
        atlas();

        struct node {
            node();
            ~node();
            node *l, *r;
            int x, y;
            int w, h;
        };

        node *insert(int w, int h);

        float occupancy() const;

        size_t width() const;
        size_t height() const;

    private:
        friend struct gui;
        node m_root;
        size_t m_width;
        size_t m_height;

    protected:
        size_t usedSurfaceArea(const node &n) const;
        node *insert(node *n, int w, int h);
    };

    struct vertex {
        m::vec2 position;
        m::vec2 coordinate;
        m::vec4 color;
    };

    u::vector<glyph> m_glyphs;

    // Batch by texture/shader
    struct batch {
        size_t start;
        size_t count;
        method *technique;
        atlas::node *texture;
    };

    u::vector<vertex> m_vertices;
    u::vector<batch> m_batches;

    static constexpr size_t kCoordCount = 100;
    static constexpr size_t kCircleVertices = 8 * 4;

    float m_coords[kCoordCount * 2];
    float m_normals[kCoordCount * 2];
    float m_circleVertices[kCircleVertices * 2];

    GLuint m_vbos[2];
    GLuint m_vao;
    unsigned char m_bufferIndex;

    u::map<u::string, texture2D*> m_modelTextures;
    u::map<u::string, atlas::node*> m_textures;
    u::map<u::string, model*> m_models;

    texture2D m_font;
    atlas::node *m_notex;

    method *m_modelMethod;
    method *m_normalMethod;
    method *m_fontMethod;
    method *m_imageMethod;

    atlas::node *atlasPack(const u::string &file);
    atlas m_atlas;
    unsigned char *m_atlasData;
    GLuint m_atlasTexture;
};

}

#endif
