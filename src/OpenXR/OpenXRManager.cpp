#include "OpenXRManager.hpp"
#include <EGL/egl.h>

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
    // EGL bindings mapped directly from current Cocos2d-x rendering thread
    XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    binding.display = eglGetCurrentDisplay();
    binding.config = (EGLConfig)0; 
    binding.context = eglGetCurrentContext();
    
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
    swapchainCreateInfo.format = 0x1908; // GL_RGBA hex value
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    
    for (int i = 0; i < 2; ++i) {
        if (XR_FAILED(xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_eyes[i].swapchain))) return false;
        
        uint32_t imageCount;
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, 0, &imageCount, nullptr);
        m_eyes[i].images.resize(imageCount);
        
        std::vector<XrSwapchainImageOpenGLESKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
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
}

void OpenXRManager::pollEvents() {}
