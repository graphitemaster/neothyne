#ifndef RENDERER_HDR
#define RENDERER_HDR
#include <SDL2/SDL_opengl.h>

#include "math.h"
#include "util.h"
#include "kdmap.h"

struct rendererPipeline {
    rendererPipeline(void);

    void scale(float scaleX, float scaleY, float scaleZ);
    void position(float x, float y, float z);
    void rotate(float rotateX, float rotateY, float rotateZ);
    void setPerspectiveProjection(float fov, float width, float height, float near, float far);
    void setCamera(const m::vec3 &position, const m::vec3 &target, const m::vec3 &up);
    const m::mat4 &getWorldTransform(void);
    const m::mat4 &getWVPTransform(void);
    const m::vec3 &getPosition(void);

private:

    m::vec3 m_scale;
    m::vec3 m_position;
    m::vec3 m_rotate;

    struct {
        float fov;
        float width;
        float height;
        float near;
        float far;
    } m_projection;

    struct {
        m::vec3 position;
        m::vec3 target;
        m::vec3 up;
    } m_camera;

    m::mat4 m_worldTransform;
    m::mat4 m_WVPTransform;
};

struct texture {
    texture();
    ~texture();
    void load(const u::string &file);
    void bind(GLenum unit);

private:
    bool m_loaded;
    GLuint m_textureHandle;
};

struct method {
    method();
    ~method();

    void enable(void);
    virtual bool init(void);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);
    bool finalize(void);

    GLint getUniformLocation(const char *name);
private:
    GLuint m_program;
    u::list<GLuint> m_shaders;
};

struct light {
    m::vec3 color;
    m::vec3 direction;
    float ambient;
    float diffuse;
};

struct lightMethod : method {
    virtual bool init(void);

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setTextureUnit(GLuint unit);
    void setNormalUnit(GLuint unit);
    void setLight(const light &l);
    void setEyeWorldPos(const m::vec3 &eyeWorldPos);
    void setMatSpecIntensity(float intensity);
    void setMatSpecPower(float power);

private:
    friend struct renderer;

    GLuint m_WVPLocation;
    GLuint m_worldInverse;
    GLuint m_sampler;
    GLuint m_normalMap;
    GLuint m_eyeWorldPosLocation;
    GLuint m_matSpecIntensityLocation;
    GLuint m_matSpecPowerLocation;

    struct {
        GLuint color;
        GLuint direction;
        GLuint ambient;
        GLuint diffuse;
    } m_light;
};

struct renderTextueBatch {
    size_t start;
    size_t count;
    size_t index;
    texture tex;
    texture bump;
};

struct renderer {
    renderer(void);
    ~renderer(void);

    void draw(rendererPipeline &p);
    void load(const kdMap &map);

    void screenShot(const u::string &file);

private:
    // called once to get function pointers for GL
    void once(void);

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };

    GLuint m_vao;
    size_t m_drawElements;
    lightMethod m_method; // the rendering method
    light m_light;
    u::vector<renderTextueBatch> m_textureBatches;
};

#endif
