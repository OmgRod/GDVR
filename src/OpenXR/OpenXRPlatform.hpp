#pragma once

// Android/Quest platform requirements
#if defined(GEODE_IS_ANDROID)
    #include <jni.h>
    #include <EGL/egl.h>
    // Do NOT include GLES3/gl3.h here to avoid Cocos macro conflicts.
    // Use OpenXR types directly which don't require the GL headers.
    #define XR_USE_PLATFORM_ANDROID
    #define XR_USE_GRAPHICS_API_OPENGL_ES
#elif defined(GEODE_IS_WINDOWS)
    #define XR_USE_PLATFORM_WIN32
    #define XR_USE_GRAPHICS_API_OPENGL
    #include <windows.h>
    #include <GL/gl.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
