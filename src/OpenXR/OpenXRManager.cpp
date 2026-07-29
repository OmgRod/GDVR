#include "OpenXRManager.hpp"

#if defined(GEODE_IS_WINDOWS)
#include <windows.h>
#include <GL/gl.h>
#elif defined(GEODE_IS_ANDROID)
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#endif

bool OpenXRManager::initialise() {
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(createInfo.applicationInfo.applicationName, "Geometry Dash VR");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    if (XR_FAILED(xrCreateInstance(&createInfo, &m_instance))) return false;

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(m_instance, &systemInfo, &m_systemId))) return false;

    if (!createSession()) return false;
    if (!createSwapchain()) return false;

    m_running = true;
    return true;
}

bool OpenXRManager::createSession() {
#if defined(GEODE_IS_WINDOWS)
    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    binding.hDC = wglGetCurrentDC();
    binding.hGLRC = wglGetCurrentContext();
#elif defined(GEODE_IS_ANDROID)
    XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    binding.display = eglGetCurrentDisplay();
    binding.config = (EGLConfig)0; 
    binding.context = eglGetCurrentContext();
#endif
    
    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &binding;
    createInfo.systemId = m_systemId;
    return XR_SUCCEEDED(xrCreateSession(m_instance, &createInfo, &m_session));
}

bool OpenXRManager::createSwapchain() {
    m_eyes.resize(2);
    XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainCreateInfo.width = 2048; 
    swapchainCreateInfo.height = 2048;
    swapchainCreateInfo.format = GL_RGBA8;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    
    for (int i = 0; i < 2; ++i) {
        if (XR_FAILED(xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_eyes[i].swapchain))) return false;
        
        uint32_t imageCount;
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, 0, &imageCount, nullptr);
        m_eyes[i].images.resize(imageCount);
        
#if defined(GEODE_IS_WINDOWS)
        std::vector<XrSwapchainImageOpenGLKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
#else
        std::vector<XrSwapchainImageOpenGLESKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
#endif
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());
        for(auto& img : images) m_eyes[i].images.push_back(img.image);
    }
    return true;
}

bool OpenXRManager::waitFrame(XrTime* displayTime) {
    XrFrameWaitInfo info{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState state{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(m_session, &info, &state))) return false;
    m_predictedDisplayTime = state.predictedDisplayTime;
    *displayTime = m_predictedDisplayTime;
    return true;
}

void OpenXRManager::beginFrame() {
    XrFrameBeginInfo info{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(m_session, &info);
}

GLuint OpenXRManager::acquireImage(const EyeData& eye) {
    XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t index;
    xrAcquireSwapchainImage(eye.swapchain, &acquire, &index);
    XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(eye.swapchain, &wait);
    return eye.images[index];
}

void OpenXRManager::releaseImage(const EyeData& eye) {
    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(eye.swapchain, &release);
}

void OpenXRManager::submitFrame(const std::vector<EyeData>& eyes) {
    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    xrEndFrame(m_session, &endInfo);
}

void OpenXRManager::shutdown() {
    for (auto& eye : m_eyes) if (eye.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(eye.swapchain);
    if (m_session != XR_NULL_HANDLE) xrDestroySession(m_session);
    if (m_instance != XR_NULL_HANDLE) xrDestroyInstance(m_instance);
    m_running = false;
}

void OpenXRManager::pollEvents() {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* sessionEvent = (XrEventDataSessionStateChanged*)&event;
            if (sessionEvent->state == XR_SESSION_STATE_STOPPING) xrEndSession(m_session);
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}
