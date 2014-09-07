#ifndef R_COMMON_HDR
#define R_COMMON_HDR
#include <SDL2/SDL_opengl.h>

namespace gl {
    void init(void);

    GLuint CreateShader(GLenum shaderType);
    void ShaderSource(GLuint shader, GLsizei count, const GLchar **string,
        const GLint *length);
    void CompileShader(GLuint shader);
    void AttachShader(GLuint program, GLuint shader);
    GLuint CreateProgram(void);
    void LinkProgram(GLuint program);
    void UseProgram(GLuint program);
    GLint GetUniformLocation(GLuint program, const GLchar *name);
    void EnableVertexAttribArray(GLuint index);
    void DisableVertexAttribArray(GLuint index);
    void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void BindBuffer(GLenum target, GLuint buffer);
    void GenBuffers(GLsizei n, GLuint *buffers);
    void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
        GLsizei stride, const GLvoid *pointer);
    void BufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
    void ValidateProgram(GLuint program);
    void GenVertexArrays(GLsizei n, GLuint *arrays);
    void BindVertexArray(GLuint array);
    void DeleteProgram(GLuint program);
    void DeleteBuffers(GLsizei n, const GLuint *buffers);
    void DeleteVertexArrays(GLsizei n, const GLuint *arrays);
    void Uniform1i(GLint location, GLint v0);
    void Uniform1f(GLint location, GLfloat v0);
    void Uniform2f(GLint location, GLfloat v0, GLfloat v1);
    void Uniform3fv(GLint location, GLsizei count, const GLfloat *value);
    void GenerateMipmap(GLenum target);
    void DeleteShader(GLuint shader);
    void GetShaderiv(GLuint shader, GLenum pname, GLint *params);
    void GetProgramiv(GLuint program, GLenum pname, GLint *params);
    void GetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length,
        GLchar *infoLog);
    void ActiveTexture(GLenum texture);
    void GenFramebuffers(GLsizei n, GLuint *ids);
    void BindFramebuffer(GLenum target, GLuint framebuffer);
    void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
        GLuint texture, GLint level);
    void DrawBuffers(GLsizei n, const GLenum *bufs);
    GLenum CheckFramebufferStatus(GLenum target);
    void DeleteFramebuffers(GLsizei n, GLuint *framebuffers);
    void Clear(GLbitfield mask);
    void ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void FrontFace(GLenum mode);
    void CullFace(GLenum mode);
    void Enable(GLenum cap);
    void Disable(GLenum cap);
    void DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
    void DepthMask(GLboolean flag);
    void BindTexture(GLenum target, GLuint texture);
    void TexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width,
        GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data);
    void DeleteTextures(GLsizei n, const GLuint *textures);
    void GenTextures(GLsizei n, GLuint *textures);
    void TexParameterf(GLenum target, GLenum pname, GLfloat param);
    void TexParameteri(GLenum target, GLenum pname, GLint param);
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void BlendEquation(GLenum mode);
    void BlendFunc(GLenum sfactor, GLenum dfactor);
    void DepthFunc(GLenum func);
    void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
        GLvoid *data);
}

#endif
