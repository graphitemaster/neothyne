#ifndef R_PARTICLES_HDR
#define R_PARTICLES_HDR
#include "r_geom.h"
#include "r_method.h"
#include "r_texture.h"

#include "m_vec.h"

namespace m {
    struct mat4;
}

namespace r {

struct pipeline;

struct particle {
    m::vec3 origin;
    m::vec3 velocity;
    float size;
    float startSize;
    m::vec3 color;
    float alpha;
    float startAlpha;
    float lifeTime;
    float totalLifeTime;
    bool respawn;
};

struct particleSystemMethod : method {
    particleSystemMethod();
    bool init();
    void setVP(const m::mat4 &vp);
    void setColorTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
private:
    uniform *m_VP;
    uniform *m_colorTextureUnit;
    uniform *m_depthTextureUnit;
};

struct particleSystem : geom {
    particleSystem();
    virtual ~particleSystem();

    bool load(const u::string &file);
    bool upload();

    void render(const pipeline &p);
    virtual void update(const pipeline &p);

    void addParticle(particle &&p);

protected:
    virtual void initParticle(particle &p, const m::vec3 &ownerPosition) = 0;
    virtual float getGravity() { return 25.0f; };

    u::vector<particle> m_particles;

private:
    struct vertex {
        m::vec3 p;
        float u, v;
        float r, g, b, a;
    };
    u::vector<vertex> m_vertices;
    particleSystemMethod m_method;
    texture2D m_texture;
};

}

#endif
