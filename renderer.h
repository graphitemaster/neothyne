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
    const m::mat4 *getTransform(void);

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

    m::mat4 m_transform;
};

struct texture {
    texture();
    ~texture();
    void load(const u::string &file);
    void bind(GLuint unit);
private:
    GLuint m_textureHandle;
};

struct renderer {
    renderer(void);
    ~renderer(void);

    void draw(const GLfloat *transform);
    void load(const kdMap &map);

private:
    // called once to get function pointer for GL
    void once(void);

    // uniforms
    GLuint m_modelViewProjection;
    GLuint m_sampler;

    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };

    GLuint m_program;
    GLuint m_vao;

    size_t m_drawElements;
    texture m_texture;
};

#endif
