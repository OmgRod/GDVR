#pragma once
#include <Geode/Geode.hpp>

#if defined(GEODE_IS_WINDOWS)
    #define XR_USE_PLATFORM_WIN32
    #define XR_USE_GRAPHICS_API_OPENGL
    #include <openxr/openxr_platform.h>
    #include <windows.h>
    #include <GL/gl.h>
#elif defined(GEODE_IS_ANDROID)
    #define XR_USE_PLATFORM_ANDROID
    #define XR_USE_GRAPHICS_API_OPENGL_ES
    #include <openxr/openxr_platform.h>
    #include <EGL/egl.h>
    #include <GLES3/gl3.h>
#endif
