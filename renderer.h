#ifndef RENDERER_HDR
#define RENDERER_HDR
#include <SDL2/SDL_opengl.h>

#include "math.h"
#include "util.h"
#include "kdmap.h"

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
    bool finalize(void);

    GLint getUniformLocation(const char *name);
    GLint getUniformLocation(const u::string &name);

private:
    GLuint m_program;
    u::string m_vertexSource;
    u::string m_fragmentSource;
    u::list<GLuint> m_shaders;
};

///! textures
struct texture {
    texture();
    ~texture();

    bool load(const u::string &file);
    void bind(GLenum unit);

private:
    bool m_loaded;
    GLuint m_textureHandle;
};

struct textureCubemap {
    textureCubemap();
    ~textureCubemap();

    bool load(const u::string files[6]);
    void bind(GLenum unit);

private:
    bool m_loaded;
    GLuint m_textureHandle;
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

    static constexpr size_t kMaxPointLights = 20;

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setNormalUnit(int unit);
    void setDirectionalLight(const directionalLight &light);
    void setPointLights(u::vector<pointLight> &pointLights);
    void setEyeWorldPos(const m::vec3 &eyeWorldPos);
    void setMatSpecIntensity(float intensity);
    void setMatSpecPower(float power);

    // fog
    void setFogType(fogType type);
    void setFogDistance(float start, float end);
    void setFogColor(const m::vec3 &color);
    void setFogDensity(float density);

private:
    friend struct renderer;

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
        GLuint densityLocation;
        GLuint colorLocation;
        GLuint startLocation;
        GLuint endLocation;
        GLuint methodLocation;
    } m_fog;

    GLint m_numPointLightsLocation;
};

///! skybox rendering method
struct skyboxMethod : method {
    virtual bool init(void);

    void setWVP(const m::mat4 &wvp);
    void setTextureUnit(int unit);

private:
    // uniforms
    GLint m_WVPLocation;
    GLint m_cubeMapLocation;
};

/// splash screen rendering method
struct splashMethod : method {
    virtual bool init(void);

    void setTextureUnit(int unit);
    void setAspectRatio(void);

private:
    GLint m_splashTextureLocation;
    GLint m_aspectRatioLocation;
};

/// splash screen renderer
struct splashScreen {
    ~splashScreen();

    bool init(const u::string &splash);

    void render(void);

private:
    union {
        struct {
            GLint m_vbo;
            GLint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
    texture m_texture;
    splashMethod m_method;
};

/// skybox renderer
struct skybox {
    ~skybox();

    bool init(const u::string &skyboxName);

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
    skyboxMethod m_method; // rendering method
    textureCubemap m_cubemap; // skybox cubemap
};

///! world renderer
struct renderTextueBatch {
    size_t start;
    size_t count;
    size_t index;
    texture tex;
    texture bump;
};

struct world : lightMethod {
    world();
    ~world();

    bool load(const kdMap &map);
    void draw(const rendererPipeline &p);

private:
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
    u::vector<renderTextueBatch> m_textureBatches;
};

void initGL(void);
void screenShot(const u::string &file);

#endif
