#ifndef RENDERER_HDR
#define RENDERER_HDR

#include <SDL2/SDL_opengl.h>
#include "math.h"
#include "util.h"

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

struct renderer {
    renderer(void);

    void draw(const GLfloat *transform);
    void load(const u::vector<unsigned char> &mapData);

private:
    // called once to get function pointer for GL
    void once(void);

    GLuint m_program;
    GLuint m_vbo;
    GLuint m_ibo;
    GLuint m_vao;
    GLuint m_modelViewProjection;
    size_t m_drawElements;
};

#endif
