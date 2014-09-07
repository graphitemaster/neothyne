#ifndef RENDERER_HDR
#define RENDERER_HDR
#include "r_methods.h"

//#include "math.h"
#include "util.h"
#include "kdmap.h"
#include "resource.h"
#include "texture.h"

struct rendererPipeline {
    rendererPipeline(void);

    void setScale(const m::vec3 &scale);
    void setWorldPosition(const m::vec3 &worldPosition);

    void setRotate(const m::vec3 &rotate);

    void setPosition(const m::vec3 &position);
    void setRotation(const m::quat &rotation);

    void setPerspectiveProjection(const m::perspectiveProjection &projection);

    const m::mat4 &getWorldTransform(void);
    const m::mat4 &getWVPTransform(void);
    const m::mat4 &getVPTransform(void);

    // camera accessors.
    const m::vec3 &getPosition(void) const;
    const m::vec3 getTarget(void) const;
    const m::vec3 getUp(void) const;
    const m::quat &getRotation(void) const;

    const m::perspectiveProjection &getPerspectiveProjection(void) const;

private:
    m::perspectiveProjection m_perspectiveProjection;

    m::vec3 m_scale;
    m::vec3 m_worldPosition;
    m::vec3 m_rotate;
    m::vec3 m_position;

    m::mat4 m_worldTransform;
    m::mat4 m_WVPTransform;
    m::mat4 m_VPTransform;

    m::quat m_rotation;
};

// gbuffer
struct gBuffer {
    // also the texture unit order
    enum textureType {
        kPosition,
        kDiffuse,
        kNormal,
        //kTexCoord,
        kMax
    };

    gBuffer();
    ~gBuffer();

    bool init(const m::perspectiveProjection &project);

    void bindReading(void);
    void bindWriting(void);
    void bindAccumulate(void);

private:
    GLuint m_fbo;
    GLuint m_textures[kMax];
    GLuint m_depthTexture;
};

///! textures
struct texture2D {
    texture2D();
    ~texture2D();

    bool load(const u::string &file);
    bool upload(void);
    void bind(GLenum unit);
    void resize(size_t width, size_t height);

private:
    bool m_uploaded;
    GLuint m_textureHandle;
    texture m_texture;
};

struct texture3D {
    texture3D();
    ~texture3D();

    bool load(const u::string &ft, const u::string &bk, const u::string &up,
              const u::string &dn, const u::string &rt, const u::string &lf);
    bool upload(void);
    void bind(GLenum unit);
    void resize(size_t width, size_t height);

private:
    bool m_uploaded;
    GLuint m_textureHandle;
    texture m_textures[6];
};

enum fogType {
    kFogNone,
    kFogLinear,
    kFogExp,
    kFogExp2
};

// a sphere
struct sphere {
    ~sphere();

    bool load(float radius, size_t rings, size_t sectors);
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
    m::sphere m_sphere;
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

void screenShot(const u::string &file, const m::perspectiveProjection &project);

#endif
