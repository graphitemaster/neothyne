#include <SDL2/SDL.h>
#include "r_common.h"

// Things not exposed via PFNGL
#ifndef APIENTRYP
#   define APIENTRYP *
#endif
typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (APIENTRYP PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLCULLFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLDISABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRYP PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalFormat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLTEXPARAMETERFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLBLENDEQUATIONPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (APIENTRYP PFNGLCOLORMASKPROC)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *data);

static PFNGLCREATESHADERPROC             glCreateShader_             = nullptr;
static PFNGLSHADERSOURCEPROC             glShaderSource_             = nullptr;
static PFNGLCOMPILESHADERPROC            glCompileShader_            = nullptr;
static PFNGLATTACHSHADERPROC             glAttachShader_             = nullptr;
static PFNGLCREATEPROGRAMPROC            glCreateProgram_            = nullptr;
static PFNGLLINKPROGRAMPROC              glLinkProgram_              = nullptr;
static PFNGLUSEPROGRAMPROC               glUseProgram_               = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation_       = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_  = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv_         = nullptr;
static PFNGLBINDBUFFERPROC               glBindBuffer_               = nullptr;
static PFNGLGENBUFFERSPROC               glGenBuffers_               = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_      = nullptr;
static PFNGLBUFFERDATAPROC               glBufferData_               = nullptr;
static PFNGLVALIDATEPROGRAMPROC          glValidateProgram_          = nullptr;
static PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays_          = nullptr;
static PFNGLBINDVERTEXARRAYPROC          glBindVertexArray_          = nullptr;
static PFNGLDELETEPROGRAMPROC            glDeleteProgram_            = nullptr;
static PFNGLDELETEBUFFERSPROC            glDeleteBuffers_            = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays_       = nullptr;
static PFNGLUNIFORM1IPROC                glUniform1i_                = nullptr;
static PFNGLUNIFORM1FPROC                glUniform1f_                = nullptr;
static PFNGLUNIFORM2FPROC                glUniform2f_                = nullptr;
static PFNGLUNIFORM3FVPROC               glUniform3fv_               = nullptr;
static PFNGLGENERATEMIPMAPPROC           glGenerateMipmap_           = nullptr;
static PFNGLDELETESHADERPROC             glDeleteShader_             = nullptr;
static PFNGLGETSHADERIVPROC              glGetShaderiv_              = nullptr;
static PFNGLGETPROGRAMIVPROC             glGetProgramiv_             = nullptr;
static PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog_         = nullptr;
static PFNGLACTIVETEXTUREPROC            glActiveTexture_            = nullptr;
static PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers_          = nullptr;
static PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer_          = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D_     = nullptr;
static PFNGLDRAWBUFFERSPROC              glDrawBuffers_              = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus_   = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers_       = nullptr;
static PFNGLCLEARPROC                    glClear_                    = nullptr;
static PFNGLCLEARCOLORPROC               glClearColor_               = nullptr;
static PFNGLFRONTFACEPROC                glFrontFace_                = nullptr;
static PFNGLCULLFACEPROC                 glCullFace_                 = nullptr;
static PFNGLENABLEPROC                   glEnable_                   = nullptr;
static PFNGLDISABLEPROC                  glDisable_                  = nullptr;
static PFNGLDRAWELEMENTSPROC             glDrawElements_             = nullptr;
static PFNGLDEPTHMASKPROC                glDepthMask_                = nullptr;
static PFNGLBINDTEXTUREPROC              glBindTexture_              = nullptr;
static PFNGLTEXIMAGE2DPROC               glTexImage2D_               = nullptr;
static PFNGLDELETETEXTURESPROC           glDeleteTextures_           = nullptr;
static PFNGLGENTEXTURESPROC              glGenTextures_              = nullptr;
static PFNGLTEXPARAMETERFPROC            glTexParameterf_            = nullptr;
static PFNGLTEXPARAMETERIPROC            glTexParameteri_            = nullptr;
static PFNGLDRAWARRAYSPROC               glDrawArrays_               = nullptr;
static PFNGLBLENDEQUATIONPROC            glBlendEquation_            = nullptr;
static PFNGLBLENDFUNCPROC                glBlendFunc_                = nullptr;
static PFNGLDEPTHFUNCPROC                glDepthFunc_                = nullptr;
static PFNGLCOLORMASKPROC                glColorMask_                = nullptr;
static PFNGLREADPIXELSPROC               glReadPixels_               = nullptr;

namespace gl {
    // Generate function wrapper around them now. We will eventually add trace
    // points here for debugging.

    void init(void) {
        #define GL_RESOLVE(NAME) \
            do { \
                union { \
                    void *p; \
                    decltype(NAME##_) gl; \
                } cast = { SDL_GL_GetProcAddress(#NAME) }; \
                if (!(NAME##_ = cast.gl)) \
                    abort(); \
            } while (0)

        GL_RESOLVE(glClear);
        GL_RESOLVE(glClearColor);
        GL_RESOLVE(glFrontFace);
        GL_RESOLVE(glCullFace);
        GL_RESOLVE(glEnable);
        GL_RESOLVE(glDisable);
        GL_RESOLVE(glDrawElements);
        GL_RESOLVE(glDepthMask);
        GL_RESOLVE(glBindTexture);
        GL_RESOLVE(glTexImage2D);
        GL_RESOLVE(glCreateShader);
        GL_RESOLVE(glShaderSource);
        GL_RESOLVE(glCompileShader);
        GL_RESOLVE(glAttachShader);
        GL_RESOLVE(glCreateProgram);
        GL_RESOLVE(glLinkProgram);
        GL_RESOLVE(glUseProgram);
        GL_RESOLVE(glGetUniformLocation);
        GL_RESOLVE(glEnableVertexAttribArray);
        GL_RESOLVE(glDisableVertexAttribArray);
        GL_RESOLVE(glUniformMatrix4fv);
        GL_RESOLVE(glBindBuffer);
        GL_RESOLVE(glGenBuffers);
        GL_RESOLVE(glVertexAttribPointer);
        GL_RESOLVE(glBufferData);
        GL_RESOLVE(glValidateProgram);
        GL_RESOLVE(glGenVertexArrays);
        GL_RESOLVE(glBindVertexArray);
        GL_RESOLVE(glDeleteProgram);
        GL_RESOLVE(glDeleteBuffers);
        GL_RESOLVE(glDeleteVertexArrays);
        GL_RESOLVE(glUniform1i);
        GL_RESOLVE(glUniform1f);
        GL_RESOLVE(glUniform2f);
        GL_RESOLVE(glUniform3fv);
        GL_RESOLVE(glGenerateMipmap);
        GL_RESOLVE(glDeleteShader);
        GL_RESOLVE(glGetShaderiv);
        GL_RESOLVE(glGetProgramiv);
        GL_RESOLVE(glGetShaderInfoLog);
        GL_RESOLVE(glActiveTexture);
        GL_RESOLVE(glGenFramebuffers);
        GL_RESOLVE(glBindFramebuffer);
        GL_RESOLVE(glFramebufferTexture2D);
        GL_RESOLVE(glDrawBuffers);
        GL_RESOLVE(glCheckFramebufferStatus);
        GL_RESOLVE(glDeleteFramebuffers);
        GL_RESOLVE(glDeleteTextures);
        GL_RESOLVE(glGenTextures);
        GL_RESOLVE(glTexParameterf);
        GL_RESOLVE(glTexParameteri);
        GL_RESOLVE(glDrawArrays);
        GL_RESOLVE(glBlendEquation);
        GL_RESOLVE(glBlendFunc);
        GL_RESOLVE(glDepthFunc);
        GL_RESOLVE(glColorMask);
        GL_RESOLVE(glReadPixels);

        ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        // back face culling
        FrontFace(GL_CW);
        CullFace(GL_BACK);
        Enable(GL_CULL_FACE);
    }

    GLuint CreateShader(GLenum shaderType) {
        return glCreateShader_(shaderType);
    }

    void ShaderSource(GLuint shader, GLsizei count, const GLchar **string,
        const GLint *length)
    {
        glShaderSource_(shader, count, string, length);
    }

    void CompileShader(GLuint shader) {
        glCompileShader_(shader);
    }

    void AttachShader(GLuint program, GLuint shader) {
        glAttachShader_(program, shader);
    }

    GLuint CreateProgram(void) {
        return glCreateProgram_();
    }

    void LinkProgram(GLuint program) {
        glLinkProgram_(program);
    }

    void UseProgram(GLuint program) {
        glUseProgram_(program);
    }

    GLint GetUniformLocation(GLuint program, const GLchar *name) {
        return glGetUniformLocation_(program, name);
    }

    void EnableVertexAttribArray(GLuint index) {
        glEnableVertexAttribArray_(index);
    }

    void DisableVertexAttribArray(GLuint index) {
        glDisableVertexAttribArray_(index);
    }

    void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
        glUniformMatrix4fv_(location, count, transpose, value);
    }

    void BindBuffer(GLenum target, GLuint buffer) {
        glBindBuffer_(target, buffer);
    }

    void GenBuffers(GLsizei n, GLuint *buffers) {
        glGenBuffers_(n, buffers);
    }

    void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
        GLsizei stride, const GLvoid *pointer)
    {
        glVertexAttribPointer_(index, size, type, normalized, stride, pointer);
    }

    void BufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
        glBufferData_(target, size, data, usage);
    }

    void ValidateProgram(GLuint program) {
        glValidateProgram_(program);
    }

    void GenVertexArrays(GLsizei n, GLuint *arrays) {
        glGenVertexArrays_(n, arrays);
    }

    void BindVertexArray(GLuint array) {
        glBindVertexArray_(array);
    }

    void DeleteProgram(GLuint program) {
        glDeleteProgram_(program);
    }

    void DeleteBuffers(GLsizei n, const GLuint *buffers) {
        glDeleteBuffers_(n, buffers);
    }

    void DeleteVertexArrays(GLsizei n, const GLuint *arrays) {
        glDeleteVertexArrays_(n, arrays);
    }

    void Uniform1i(GLint location, GLint v0) {
        glUniform1i_(location, v0);
    }

    void Uniform1f(GLint location, GLfloat v0) {
        glUniform1f_(location, v0);
    }

    void Uniform2f(GLint location, GLfloat v0, GLfloat v1) {
        glUniform2f_(location, v0, v1);
    }

    void Uniform3fv(GLint location, GLsizei count, const GLfloat *value) {
        glUniform3fv_(location, count, value);
    }

    void GenerateMipmap(GLenum target) {
        glGenerateMipmap_(target);
    }

    void DeleteShader(GLuint shader) {
        glDeleteShader_(shader);
    }

    void GetShaderiv(GLuint shader, GLenum pname, GLint *params) {
        glGetShaderiv_(shader, pname, params);
    }

    void GetProgramiv(GLuint program, GLenum pname, GLint *params) {
        glGetProgramiv_(program, pname, params);
    }

    void GetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length,
        GLchar *infoLog)
    {
        glGetShaderInfoLog_(shader, maxLength, length, infoLog);
    }

    void ActiveTexture(GLenum texture) {
        glActiveTexture_(texture);
    }

    void GenFramebuffers(GLsizei n, GLuint *ids) {
        glGenFramebuffers_(n, ids);
    }

    void BindFramebuffer(GLenum target, GLuint framebuffer) {
        glBindFramebuffer_(target, framebuffer);
    }

    void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
        GLuint texture, GLint level)
    {
        glFramebufferTexture2D_(target, attachment, textarget, texture, level);
    }

    void DrawBuffers(GLsizei n, const GLenum *bufs) {
        glDrawBuffers_(n, bufs);
    }

    GLenum CheckFramebufferStatus(GLenum target) {
        return glCheckFramebufferStatus_(target);
    }

    void DeleteFramebuffers(GLsizei n, GLuint *framebuffers) {
        glDeleteBuffers_(n, framebuffers);
    }

    void Clear(GLbitfield mask) {
        glClear_(mask);
    }

    void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
        glClearColor_(red, green, blue, alpha);
    }

    void FrontFace(GLenum mode) {
        glFrontFace_(mode);
    }

    void CullFace(GLenum mode) {
        glCullFace_(mode);
    }

    void Enable(GLenum cap) {
        glEnable_(cap);
    }

    void Disable(GLenum cap) {
        glDisable_(cap);
    }

    void DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        glDrawElements_(mode, count, type, indices);
    }

    void DepthMask(GLboolean flag) {
        glDepthMask_(flag);
    }

    void BindTexture(GLenum target, GLuint texture) {
        glBindTexture_(target, texture);
    }

    void TexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width,
        GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
    {
        glTexImage2D_(target, level, internalFormat, width, height, border,
            format, type, data);
    }

    void DeleteTextures(GLsizei n, const GLuint *textures) {
        glDeleteTextures_(n, textures);
    }

    void GenTextures(GLsizei n, GLuint *textures) {
        glGenTextures_(n, textures);
    }

    void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
        glTexParameterf_(target, pname, param);
    }

    void TexParameteri(GLenum target, GLenum pname, GLint param) {
        glTexParameteri_(target, pname, param);
    }

    void DrawArrays(GLenum mode, GLint first, GLsizei count) {
        glDrawArrays_(mode, first, count);
    }

    void BlendEquation(GLenum mode) {
        glBlendEquation_(mode);
    }

    void BlendFunc(GLenum sfactor, GLenum dfactor) {
        glBlendFunc_(sfactor, dfactor);
    }

    void DepthFunc(GLenum func) {
        glDepthFunc_(func);
    }

    void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
        glColorMask_(red, green, blue, alpha);
    }

    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
        GLvoid *data)
    {
        glReadPixels_(x, y, width, height, format, type, data);
    }
}
