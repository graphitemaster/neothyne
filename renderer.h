#ifndef RENDERER_HDR
#define RENDERER_HDR
#include "r_methods.h"
#include "r_pipeline.h"
#include "r_texture.h"

#include "kdmap.h"
#include "resource.h"

namespace r {

// gbuffer
struct gBuffer {
    // also the texture unit order
    enum textureType {
        kDiffuse,
        kNormal,
        kDepth,
        kMax
    };

    gBuffer();
    ~gBuffer();

    bool init(const m::perspectiveProjection &project);

    void update(const m::perspectiveProjection &project);
    void bindReading(void);
    void bindWriting(void);

private:
    void destroy(void);

    GLuint m_fbo;
    GLuint m_textures[kMax];
    size_t m_width;
    size_t m_height;
};

enum fogType {
    kFogNone,
    kFogLinear,
    kFogExp,
    kFogExp2
};

// a quad
struct quad {
    ~quad();

    bool upload(void);
    void render(void);

private:
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
};

/// splash screen renderer
struct splashScreen : splashMethod {
    bool load(const u::string &splashScreen);
    bool upload(void);

    void render(float dt, const rendererPipeline &pipeline);
private:
    quad m_quad;
    texture2D m_texture;
};

/// billboard renderer
struct billboard : billboardMethod {
    billboard();
    ~billboard();

    bool load(const u::string &billboardTexture);
    bool upload(void);

    void render(const rendererPipeline &pipeline);

    // you must add all positions for this billboard before calling `upload'
    void add(const m::vec3 &position);

private:
    GLuint m_vbo;
    GLuint m_vao;
    u::vector<m::vec3> m_positions;
    texture2D m_texture;
};

/// skybox renderer
struct skybox : skyboxMethod {
    ~skybox();

    bool load(const u::string &skyboxName);
    bool upload(void);

    void render(const rendererPipeline &pipeline);

private:
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
    texture3D m_cubemap; // skybox cubemap
};

///! world renderer
struct renderTextueBatch {
    size_t start;
    size_t count;
    size_t index;
    texture2D *diffuse;
    texture2D *normal;
};

struct world {
    world();
    ~world();

    enum billboardType {
        kBillboardRail,
        kBillboardLightning,
        kBillboardRocket,
        kBillboardShotgun,
        kBillboardMax
    };

    bool load(const kdMap &map);
    bool upload(const m::perspectiveProjection &p);

    void render(const rendererPipeline &p);

private:
    void depthPass(const rendererPipeline &pipeline);
    void depthPrePass(const rendererPipeline &pipeline);
    void geometryPass(const rendererPipeline &pipeline);
    void beginLightPass(void);
    void pointLightPass(const rendererPipeline &pipeline);
    void directionalLightPass(const rendererPipeline &pipeline);

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;

    // The following rendering methods / passes for the world
    depthMethod m_depthMethod;
    geomMethod m_geomMethod;
    directionalLightMethod m_directionalLightMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_directionalLightQuad;
    u::vector<billboard> m_billboards;

    // The world itself
    u::vector<uint32_t> m_indices;
    u::vector<kdBinVertex> m_vertices;
    u::vector<renderTextueBatch> m_textureBatches;
    resourceManager<u::string, texture2D> m_textures2D;

    // World lights
    directionalLight m_directionalLight;
    gBuffer m_gBuffer;
};

}

#endif
