#pragma once

#ifdef USE_TRACY_GPU

#include "Log.hpp"

#include <GL/gl.h>
#include <GLES2/gl2ext.h>

inline PFNGLQUERYCOUNTEREXTPROC        glQueryCounter;
inline PFNGLGETQUERYOBJECTIVEXTPROC    glGetQueryObjectiv;
inline PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64v;

#include "../../subprojects/tracy/public/tracy/TracyOpenGL.hpp"

#define TRACY_GPU_CONTEXT TracyGpuContext
#define TRACY_GPU_ZONE(e) TracyGpuZone(e)
#define TRACY_GPU_COLLECT TracyGpuCollect

#else

#define TRACY_GPU_CONTEXT
#define TRACY_GPU_ZONE(e)
#define TRACY_GPU_COLLECT

#endif