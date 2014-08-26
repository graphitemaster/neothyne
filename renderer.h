#ifndef RENDERER_HDR
#define RENDERER_HDR
#include <SDL2/SDL_opengl.h>

#include "math.h"
#include "util.h"
#include "kdmap.h"
#include "resource.h"
#include "texture.h"

struct perspectiveProjection {
    float fov;
    float width;
    float height;
    float near;
    float far;
};

struct rendererPipeline {
    rendererPipeline(void);

    void setScale(float scaleX, float scaleY, float scaleZ);
    void setWorldPosition(float x, float y, float z);
    void setRotate(float rotateX, float rotateY, float rotateZ);
    void setPerspectiveProjection(const perspectiveProjection &projection);
    void setCamera(const m::vec3 &position, const m::vec3 &target, const m::vec3 &up);
    const m::mat4 &getWorldTransform(void);
    const m::mat4 &getWVPTransform(void);
    const m::mat4 &getVPTransform(void);

    // camera accessors.
    const m::vec3 &getPosition(void) const;
    const m::vec3 &getTarget(void) const;
    const m::vec3 &getUp(void) const;

    const perspectiveProjection &getPerspectiveProjection(void) const;

private:

    m::vec3 m_scale;
    m::vec3 m_worldPosition;
    m::vec3 m_rotate;

    perspectiveProjection m_perspectiveProjection;

    struct {
        m::vec3 position;
        m::vec3 target;
        m::vec3 up;
    } m_camera;

    m::mat4 m_worldTransform;
    m::mat4 m_WVPTransform;
    m::mat4 m_VPTransform;
};

// inherit when writing a rendering method
struct method {
    method();
    ~method();

    void enable(void);

    virtual bool init(void);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    void addVertexPrelude(const u::string &prelude);
    void addFragmentPrelude(const u::string &prelude);
    void addGeometryPrelude(const u::string &prelude);

    bool finalize(void);

    GLint getUniformLocation(const char *name);
    GLint getUniformLocation(const u::string &name);

private:
    GLuint m_program;
    u::string m_vertexSource;
    u::string m_fragmentSource;
    u::string m_geometrySource;
    u::list<GLuint> m_shaders;
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

///! lights
struct baseLight {
    m::vec3 color;
    float ambient;
    float diffuse;
};

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

// a point light
struct pointLight : baseLight {
    pointLight()
    {
        attenuation.constant = 0.0f;
        attenuation.linear = 0.0f;
        attenuation.exp = 0.0f;
    }

    m::vec3 position;
    struct {
        float constant;
        float linear;
        float exp;
    } attenuation;
};

// a spot light
struct spotLight : pointLight {
    spotLight() :
        cutOff(0.0f)
    { }
    m::vec3 direction;
    float cutOff;
};

enum fogType {
    kFogNone,
    kFogLinear,
    kFogExp,
    kFogExp2
};

///! light rendering method
struct lightMethod : method {
    lightMethod();

    virtual bool init(void);

    static constexpr size_t kMaxPointLights = 5;
    static constexpr size_t kMaxSpotLights = 5;

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setNormalUnit(int unit);

    // lighting
    void setDirectionalLight(const directionalLight &light);
    void setPointLights(u::vector<pointLight> &pointLights);
    void setSpotLights(u::vector<spotLight> &spotLights);

    // specularity
    void setEyeWorldPos(const m::vec3 &eyeWorldPos);
    void setMatSpecIntensity(float intensity);
    void setMatSpecPower(float power);

    // fog
    void setFogType(fogType type);
    void setFogDistance(float start, float end);
    void setFogColor(const m::vec3 &color);
    void setFogDensity(float density);

private:
    friend struct world;

    // uniforms
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_samplerLocation;
    GLint m_normalMapLocation;
    GLint m_eyeWorldPosLocation;
    GLint m_matSpecIntensityLocation;
    GLint m_matSpecPowerLocation;

    struct {
        GLint colorLocation;
        GLint directionLocation;
        GLint ambientLocation;
        GLint diffuseLocation;
    } m_directionalLight;

    struct {
        GLint colorLocation;
        GLint positionLocation;
        GLint ambientLocation;
        GLint diffuseLocation;
        struct {
            GLint constantLocation;
            GLint linearLocation;
            GLint expLocation;
        } attenuation;
    } m_pointLights[kMaxPointLights];

    struct {
        GLint colorLocation;
        GLint positionLocation;
        GLint ambientLocation;
        GLint diffuseLocation;
        GLint directionLocation;
        GLint cutOffLocation;
        struct {
            GLint constantLocation;
            GLint linearLocation;
            GLint expLocation;
        } attenuation;
    } m_spotLights[kMaxSpotLights];

    struct {
        GLuint densityLocation;
        GLuint colorLocation;
        GLuint startLocation;
        GLuint endLocation;
        GLuint methodLocation;
    } m_fog;

    GLint m_numPointLightsLocation;
    GLint m_numSpotLightsLocation;
};

///! skybox rendering method
struct skyboxMethod : method {
    virtual bool init(void);

    void setWVP(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setWorld(const m::mat4 &worldInverse);

private:
    // uniforms
    GLint m_WVPLocation;
    GLint m_cubeMapLocation;
    GLint m_worldLocation;
};

/// splash screen rendering method
struct splashMethod : method {
    virtual bool init(void);

    void setTextureUnit(int unit);
    void setResolution(void);
    void setTime(float dt);

private:
    GLint m_splashTextureLocation;
    GLint m_resolutionLocation;
    GLint m_timeLocation;
};

/// depth pre pass rendering method
struct depthPrePassMethod : method {
    virtual bool init(void);

    void setWVP(const m::mat4 &wvp);

private:
    GLint m_WVPLocation;
};

/// billboard rendering method
struct billboardMethod : method {
    virtual bool init(void);

    void setVP(const m::mat4 &vp);
    void setCamera(const m::vec3 &position);
    void setTextureUnit(int unit);
    void setSize(float width, float height);

private:
    GLuint m_VPLocation;
    GLuint m_cameraPositionLocation;
    GLuint m_colorMapLocation;
    GLuint m_sizeLocation;
};

/// splash screen renderer
struct splashScreen : splashMethod {
    ~splashScreen();

    bool load(const u::string &splashScreen);
    bool upload(void);

    void render(float dt);

private:
    union {
        struct {
            GLint m_vbo;
            GLint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
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

    bool load(const kdMap &map);
    bool upload(const kdMap &map);

    void draw(const rendererPipeline &p);

private:
    void render(const rendererPipeline &p);

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;

    skybox m_skybox;
    directionalLight m_directionalLight;
    u::vector<pointLight> m_pointLights;
    u::vector<spotLight> m_spotLights;
    u::vector<renderTextueBatch> m_textureBatches;
    u::vector<uint32_t> m_indices;

    resourceManager<u::string, texture2D> m_textures2D;

    lightMethod m_lightMethod;
    depthPrePassMethod m_depthPrePassMethod;


    u::vector<billboard> m_billboards;
};

void initGL(void);
void screenShot(const u::string &file);

#endif
