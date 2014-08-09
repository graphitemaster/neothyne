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
    void bind(GLuint unit);
private:
    GLuint m_textureHandle;
};

struct light {
    light(const m::vec3 &color, float ambientIntensity, float diffuseIntensity, const m::vec3 &direction);

    void init(GLuint colorLocation, GLuint ambientIntensityLocation,
        GLuint diffuseIntensityLocation, GLuint directionLocation);

    void activate(void);

private:
    friend struct renderer;
    // uniforms
    GLuint m_colorLocation;
    GLuint m_ambientIntensityLocation;
    GLuint m_diffuseIntensityLocation;
    GLuint m_directionLocation;

    // properties
    m::vec3 m_color;
    float m_ambientIntensity;
    float m_diffuseIntensity;
    m::vec3 m_direction;
};

struct renderer {
    renderer(void);
    ~renderer(void);

    void draw(rendererPipeline &p);
    void load(const kdMap &map);

    void screenShot(const u::string &file);

private:
    // called once to get function pointer for GL
    void once(void);

    // uniforms
    GLuint m_modelViewProjection;
    GLuint m_sampler;
    GLuint m_directionalLightColor;
    GLuint m_directionalLightIntensity;
    GLuint m_worldTransform;

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };

    GLuint m_program;
    GLuint m_vao;

    light m_light;

    size_t m_drawElements;
    texture m_texture;
};

#endif
