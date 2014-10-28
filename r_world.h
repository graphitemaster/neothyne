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
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setSpecTextureUnit(int unit);
    void setSpecPower(float power);
    void setSpecIntensity(float intensity);

private:
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_specTextureUnitLocation;
    GLint m_specPowerLocation;
    GLint m_specIntensityLocation;
};

struct lightMethod : method {
    bool init(const char *vs, const char *fs);

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setSpecTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setPerspectiveProjection(const m::perspectiveProjection &project);
    void setDepthTextureUnit(int unit);

private:
    GLint m_WVPLocation;
    GLint m_inverseLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_specTextureUnitLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_eyeWorldPositionLocation;
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

struct finalMethod : method {
    bool init();

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspectiveProjection(const m::perspectiveProjection &project);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_screenSizeLocation;
};


struct finalComposite {
    finalComposite();
    ~finalComposite();

    bool init(const m::perspectiveProjection &project, GLuint depth);

    void update(const m::perspectiveProjection &project, GLuint depth);
    void bindReading();
    void bindWriting();

private:
    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

struct renderTextureBatch {
    size_t start;
    size_t count;
    size_t index;
    texture2D *diffuse;
    texture2D *normal;
    texture2D *spec;
    bool specParams;
    float specPower;
    float specIntensity;
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
    void geometryPass(const rendererPipeline &pipeline);
    void directionalLightPass(const rendererPipeline &pipelines);

    bool loadMaterial(const kdMap &map, renderTextureBatch *batch);

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;

    // world shading methods and permutations

    // 0 = pass-through shader (vertex normals)
    // 1 = diffuse only permutation
    // 2 = diffuse and normal permutation
    // 3 = diffuse and spec permutation
    // 4 = diffuse and spec param permutation
    // 5 = diffuse and normal and spec permutation
    // 6 = diffuse and normal and spec param permutation
    geomMethod m_geomMethods[7];

    // final composite method
    finalMethod m_finalMethod;

    directionalLightMethod m_directionalLightMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    u::vector<billboard> m_billboards;

    // The world itself
    u::vector<uint32_t> m_indices;
    u::vector<kdBinVertex> m_vertices;
    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    // World lights
    directionalLight m_directionalLight;

    gBuffer m_gBuffer;
    finalComposite m_final;

    m::mat4 m_identity;
};

}

#endif
