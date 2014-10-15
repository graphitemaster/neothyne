#ifndef R_WORLD_HDR
#define R_WORLD_HDR

#include "r_common.h"
#include "r_texture.h"
#include "r_skybox.h"
#include "r_billboard.h"
#include "r_gbuffer.h"
#include "r_method.h"
#include "r_quad.h"

#include "u_map.h"

#include "kdmap.h"

namespace r {

struct baseLight {
    m::vec3 color;
    float ambient;
    float diffuse;
};

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

struct geomMethod : method {
    bool init();

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);

private:
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
};

struct lightMethod : method {
    bool init(const char *vs, const char *fs);

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setMatSpecIntensity(float intensity);
    void setMatSpecPower(float power);
    void setPerspectiveProjection(const m::perspectiveProjection &project);
    void setDepthTextureUnit(int unit);

private:
    GLint m_WVPLocation;
    GLint m_inverseLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_eyeWorldPositionLocation;
    GLint m_matSpecularIntensityLocation;
    GLint m_matSpecularPowerLocation;
    GLint m_screenSizeLocation;
    GLint m_screenFrustumLocation;
    GLint m_depthTextureUnitLocation;
};

struct directionalLightMethod : lightMethod {
    bool init();

    void setDirectionalLight(const directionalLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint direction;
    } m_directionalLightLocation;
};

struct depthMethod : method {
    virtual bool init();

    void setWVP(const m::mat4 &wvp);

private:
    GLint m_WVPLocation;
};

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
    void beginLightPass();
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
    u::map<u::string, texture2D*> m_textures2D;

    texture2D *m_noTexture;
    texture2D *m_noNormal;

    // World lights
    directionalLight m_directionalLight;
    gBuffer m_gBuffer;
};

}

#endif
