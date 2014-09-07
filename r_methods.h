#ifndef R_METHODS_HDR
#define R_METHODS_HDR

#include "r_common.h"
#include "r_light.h"

#include "util.h"

struct method {
    method();
    ~method();

    void enable(void);

    bool init(void);

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

struct geomMethod : method {
    bool init(void);

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);

private:
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_colorMapLocation;
};

///! Light rendering method
struct lightMethod : method {
    bool init(const char *vs, const char *fs);

    void setWVP(const m::mat4 &wvp);
    void setPositionTextureUnit(int unit);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setMatSpecIntensity(float intensity);
    void setMatSpecPower(float power);
    void setScreenSize(size_t width, size_t height);

private:
    GLint m_WVPLocation;
    GLint m_positionTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_eyeWorldPositionLocation;
    GLint m_matSpecularIntensityLocation;
    GLint m_matSpecularPowerLocation;
    GLint m_screenSizeLocation;
};

struct directionalLightMethod : lightMethod {
    bool init(void);

    void setDirectionalLight(const directionalLight &light);

private:
    struct {
        GLint color;
        GLint ambient;
        GLint diffuse;
        GLint direction;
    } m_directionalLightLocation;
};

struct skyboxMethod : method {
    virtual bool init(void);

    void setWVP(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setWorld(const m::mat4 &worldInverse);

private:
    GLint m_WVPLocation;
    GLint m_cubeMapLocation;
    GLint m_worldLocation;
};

struct splashMethod : method {
    virtual bool init(void);

    void setTextureUnit(int unit);
    void setScreenSize(const m::perspectiveProjection &project);
    void setTime(float dt);

private:
    GLint m_splashTextureLocation;
    GLint m_screenSizeLocation;
    GLint m_timeLocation;
};

/// Depth Pass Method
struct depthMethod : method {
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

#endif
