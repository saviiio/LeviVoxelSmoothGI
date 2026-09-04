#pragma once
using EGLDisplay=void*; using EGLSurface=void*; using EGLContext=void*; using EGLBoolean=unsigned int;
using __eglMustCastToProperFunctionPointerType = void (*)();
#define EGL_FALSE 0
#define EGL_NO_CONTEXT ((EGLContext)0)
extern "C" EGLContext eglGetCurrentContext();
extern "C" __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char*);
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface);
