#ifndef R_PARTICLES_HDR
#define R_PARTICLES_HDR
#include "r_geom.h"
#include "r_method.h"
#include "r_texture.h"

#include "m_vec.h"
#include "m_half.h"

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
    void setPower(float power);
private:
    uniform *m_VP;
    uniform *m_colorTextureUnit;
    uniform *m_depthTextureUnit;
    uniform *m_power;
};

struct particleSystem : geom {
    particleSystem() = default;
    virtual ~particleSystem() = default;

    bool load(const u::string &file);
    bool upload();

    void render(const pipeline &p);
    virtual void update(const pipeline &p);

    void addParticle(particle &&p);

protected:
    virtual void initParticle(particle &p, const m::vec3 &ownerPosition) = 0;
    virtual float gravity() { return 25.0f; }
    virtual float power() { return 5.0f; }

    u::vector<particle> m_particles;

private:
    struct singleVertex {
        // 16 bytes
        m::vec3 position;
        unsigned char color[4];
    };
    struct halfVertex {
        // 10 bytes
        m::half position[3];
        unsigned char color[4];
    };
    u::vector<singleVertex> m_singleVertices;
    u::vector<halfVertex> m_halfVertices;

    particleSystemMethod m_method;
    texture2D m_texture;
    u::vector<GLuint> m_indices;
};

}

#endif
