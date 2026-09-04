#pragma once
#include <cstddef>
#include <cstdint>
using GLenum=unsigned int; using GLboolean=unsigned char; using GLbitfield=unsigned int; using GLvoid=void; using GLbyte=std::int8_t; using GLshort=std::int16_t; using GLint=int; using GLsizei=int; using GLubyte=unsigned char; using GLushort=unsigned short; using GLuint=unsigned int; using GLfloat=float; using GLclampf=float; using GLdouble=double; using GLintptr=std::intptr_t; using GLsizeiptr=std::intptr_t; using GLchar=char;
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_NO_ERROR 0
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_EXTENSIONS 0x1F03
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS 0x90DD
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_SHADER_STORAGE_BUFFER_BINDING 0x90D3
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_SHADER_STORAGE_BARRIER_BIT 0x2000
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_SCISSOR_TEST 0x0C11
#define GL_VIEWPORT 0x0BA2
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_NEAREST 0x2600
extern "C" {
const GLubyte* glGetString(GLenum); void glGetIntegerv(GLenum,GLint*); void glGetIntegeri_v(GLenum,GLuint,GLint*); GLenum glGetError();
GLuint glCreateShader(GLenum); void glShaderSource(GLuint,GLsizei,const GLchar* const*,const GLint*); void glCompileShader(GLuint); void glGetShaderiv(GLuint,GLenum,GLint*); void glGetShaderInfoLog(GLuint,GLsizei,GLsizei*,GLchar*); void glDeleteShader(GLuint);
GLuint glCreateProgram(); void glAttachShader(GLuint,GLuint); void glDetachShader(GLuint,GLuint); void glLinkProgram(GLuint); void glGetProgramiv(GLuint,GLenum,GLint*); void glGetProgramInfoLog(GLuint,GLsizei,GLsizei*,GLchar*); void glDeleteProgram(GLuint); void glUseProgram(GLuint);
void glBindAttribLocation(GLuint,GLuint,const GLchar*); void glTransformFeedbackVaryings(GLuint,GLsizei,const GLchar* const*,GLenum);
void glProgramBinary(GLuint,GLenum,const void*,GLsizei); void glGetProgramBinary(GLuint,GLsizei,GLsizei*,GLenum*,void*); void glProgramParameteri(GLuint,GLenum,GLint);
void glGenBuffers(GLsizei,GLuint*); void glBindBuffer(GLenum,GLuint); void glBufferData(GLenum,GLsizeiptr,const void*,GLenum); void glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*); void* glMapBufferRange(GLenum,GLintptr,GLsizeiptr,GLbitfield); GLboolean glUnmapBuffer(GLenum); void glDeleteBuffers(GLsizei,const GLuint*); void glBindBufferBase(GLenum,GLuint,GLuint);
GLint glGetUniformLocation(GLuint,const GLchar*); void glUniform1i(GLint,GLint); void glUniform1f(GLint,GLfloat); void glUniform3i(GLint,GLint,GLint,GLint); void glUniform4i(GLint,GLint,GLint,GLint,GLint);
void glDispatchCompute(GLuint,GLuint,GLuint); void glMemoryBarrier(GLbitfield);
void glBindFramebuffer(GLenum,GLuint); GLboolean glIsEnabled(GLenum); void glDisable(GLenum); void glEnable(GLenum); void glBlitFramebuffer(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
}
