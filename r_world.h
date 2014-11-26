#ifndef R_WORLD_HDR
#define R_WORLD_HDR

#include "r_ssao.h"
#include "r_skybox.h"
#include "r_billboard.h"
#include "r_gbuffer.h"
#include "r_geom.h"
#include "r_model.h"

#include "u_map.h"

#include "kdmap.h"

namespace r {

struct baseLight {
    m::vec3 color;
    float ambient;
    union {
        float diffuse;
        float intensity;
    };
};

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

// a point light
struct pointLight : baseLight {
    pointLight()
        : radius(0.0f)
    {
    }
    m::vec3 position;
    float radius;
};

struct spotLight : pointLight {
    spotLight()
        : cutOff(0.0f)
    {
    }
    m::vec3 direction;
    float cutOff;
};

struct geomMethod : method {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setSpecTextureUnit(int unit);
    void setDispTextureUnit(int unit);
    void setSpecPower(float power);
    void setSpecIntensity(float intensity);
    void setEyeWorldPos(const m::vec3 &position);
    void setParallax(float scale, float bias);

private:
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_specTextureUnitLocation;
    GLint m_dispTextureUnitLocation;
    GLint m_specPowerLocation;
    GLint m_specIntensityLocation;
    GLint m_eyeWorldPositionLocation;
    GLint m_parallaxLocation;
};

struct lightMethod : method {
    bool init(const char *vs, const char *fs, const u::vector<const char *> &defines = u::vector<const char *>());

    enum {
        // First three must have same layout as gBuffer
        kColor  = gBuffer::kColor,
        kNormal = gBuffer::kNormal,
        kDepth  = gBuffer::kDepth,
        kOcclusion
    };

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setOcclusionTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setPerspectiveProjection(const m::perspectiveProjection &project);

private:
    GLint m_WVPLocation;
    GLint m_inverseLocation;

    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_depthTextureUnitLocation;
    GLint m_occlusionTextureUnitLocation;

    GLint m_eyeWorldPositionLocation;

    GLint m_screenSizeLocation;
    GLint m_screenFrustumLocation;
};

struct directionalLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const directionalLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint direction;
    } m_directionalLightLocation;
};

struct pointLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const pointLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint position;
        GLint radius;
    } m_pointLightLocation;
};

struct spotLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const spotLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint position;
        GLint radius;
        GLint direction;
        GLint cutOff;
    } m_spotLightLocation;
};

struct finalMethod : method {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspectiveProjection(const m::perspectiveProjection &project);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_screenSizeLocation;
};

struct ssaoMethod : method {
    bool init();

    // Note if you change this you'll need to update the shader since all
    // of the `kKernelSize' iterations are unrolled
    static constexpr size_t kKernelSize = 4;

    enum {
        kNormal,
        kDepth,
        kRandom
    };

    void setOccluderBias(float bias);
    void setSamplingRadius(float radius);
    void setAttenuation(float constant, float linear);
    void setInverse(const m::mat4 &inverse);
    void setWVP(const m::mat4 &wvp);
    void setPerspectiveProjection(const m::perspectiveProjection &project);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setRandomTextureUnit(int unit);

private:
    GLint m_occluderBiasLocation;
    GLint m_samplingRadiusLocation;
    GLint m_attenuationLocation;
    GLint m_inverseLocation;
    GLint m_WVPLocation;
    GLint m_screenFrustumLocation;
    GLint m_screenSizeLocation;
    GLint m_normalTextureLocation;
    GLint m_depthTextureLocation;
    GLint m_randomTextureLocation;
    GLint m_kernelLocation[kKernelSize];
};

struct finalComposite {
    finalComposite();
    ~finalComposite();

    bool init(const m::perspectiveProjection &project, GLuint depth);
    void update(const m::perspectiveProjection &project, GLuint depth);
    void bindWriting();

    GLuint texture() const;

private:
    enum {
        kBuffer,
        kDepth
    };

    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

struct renderTextureBatch {
    int permute;
    size_t start;
    size_t count;
    size_t index;
    material mat; // Rendering material (world and models share this)
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
    void scenePass(const rendererPipeline &pipeline);
    void lightPass(const rendererPipeline &pipeline);
    void finalPass(const rendererPipeline &pipeline);
    void otherPass(const rendererPipeline &pipeline);

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;

    // world shading methods and permutations
    u::vector<geomMethod> m_geomMethods;
    u::vector<finalMethod> m_finalMethods;
    u::vector<directionalLightMethod> m_directionalLightMethods;
    pointLightMethod m_pointLightMethod;
    spotLightMethod m_spotLightMethod;
    ssaoMethod m_ssaoMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    sphere m_sphere;
    u::vector<billboard> m_billboards;

    // The world itself
    u::vector<uint32_t> m_indices;
    u::vector<kdBinVertex> m_vertices;
    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    // World lights
    directionalLight m_directionalLight;
    u::vector<pointLight> m_pointLights;
    u::vector<spotLight> m_spotLights;

    gBuffer m_gBuffer;
    ssao m_ssao;
    finalComposite m_final;

    m::mat4 m_identity;
    m::frustum m_frustum;
    u::vector<model> m_models;

    bool m_uploaded;
};

}

#endif
