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

struct guiMethod : method {
    guiMethod();

    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setPerspective(const m::perspective &p);
    void setColorTextureUnit(int unit);

private:
    uniform *m_screenSize;
    uniform *m_colorMap;
};

struct guiModelMethod : method {
    guiModelMethod();

    bool init(const u::vector<const char *> &defines = u::vector<const char *>());
    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &world);
    void setColorTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &pos);
    void setScroll(int u, int v, float millis);

private:
    uniform *m_WVP;
    uniform *m_world;
    uniform *m_colorTextureUnit;
    uniform *m_eyeWorldPosition;
    uniform *m_scrollRate;
    uniform *m_scrollMillis;
};

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
    void drawTexture(float x, float y, float w, float h, const unsigned char *data);

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
        int method;
    };

    void addBatch(batch &b);

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
    guiMethod m_methods[3];
    guiModelMethod m_modelMethod;
    guiModelMethod m_modelScrollMethod;

    atlas::node *atlasPack(const u::string &file);
    atlas m_atlas;
    unsigned char *m_atlasData;
    GLuint m_atlasTexture;
    GLuint m_miscTexture; // miscellaneous texture for drawTexture

    m::vec2 m_resolution;
    size_t m_coalesced;
};

}

#endif
