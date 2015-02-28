#ifndef R_PARTICLES_HDR
#define R_PARTICLES_HDR
#include "r_method.h"
#include "r_texture.h"

#include "m_vec3.h"

namespace m {
    struct mat4;
}

namespace r {

struct pipeline;

struct particle {
    particle();
    m::vec3 startOrigin;
    m::vec3 currentOrigin;
    m::vec3 velocity;
    m::vec3 color;
    float currentSize;
    float startSize;
    float currentAlpha;
    float startAlpha;
    float lifeTime;
    float totalLifeTime;
    bool respawn;

};

struct particleSystemMethod : method {
    particleSystemMethod();
    bool init();
    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
private:
    GLint m_WVPLocation;
    GLint m_colorTextureUnitLocation;
};

struct particleSystem {
    typedef void (*initParticleFunction)(particle &p, const m::vec3 &);

    particleSystem();
    ~particleSystem();

    bool load(const u::string &file, initParticleFunction initFunction);
    bool upload();

    void render(const pipeline &p);
    void update(const pipeline &p);

    void addParticle(particle &&p);

    void initParticle(particle &p, const m::vec3 &ownerPosition);

private:
    struct vertex {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };
    u::vector<vertex> m_vertices;
    u::vector<particle> m_particles;
    initParticleFunction m_initFunction;
    particleSystemMethod m_method;
    float m_gravity;
    GLuint m_vao;
    GLuint m_vbo;
    texture2D m_texture;
};

}

#endif
