#define GLM_ENABLE_EXPERIMENTAL
#include "OpenXRManager.hpp"
#include <EGL/egl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#if defined(GEODE_IS_ANDROID)
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#endif

using namespace geode::prelude;

bool OpenXRManager::initialise() {
#if defined(GEODE_IS_ANDROID)
    // Get JNIEnv safely from the JavaVM
    JavaVM* vm = cocos2d::JniHelper::getJavaVM();
    JNIEnv* env = nullptr;
    bool didAttach = false;
    
    jint envResult = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (envResult == JNI_EDETACHED) {
        log::info("OpenXR: JNI thread not attached, attaching...");
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            log::error("OpenXR: Failed to attach JNI thread");
            return false;
        }
        didAttach = true;
    } else if (envResult != JNI_OK || env == nullptr) {
        log::error("OpenXR: Failed to get JNIEnv, result: {}", (int)envResult);
        return false;
    }
    log::info("OpenXR: JNIEnv acquired successfully");
    
    // Use android.app.ActivityThread.currentApplication() to get the app context.
    // This is a standard Android API that works regardless of what Activity class
    // the Geode launcher uses (unlike Cocos2dxActivity.getContext() which doesn't exist).
    jobject appContext = nullptr;
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (activityThreadClass && !env->ExceptionCheck()) {
        jmethodID currentAppMethod = env->GetStaticMethodID(activityThreadClass, "currentApplication", "()Landroid/app/Application;");
        if (currentAppMethod && !env->ExceptionCheck()) {
            appContext = env->CallStaticObjectMethod(activityThreadClass, currentAppMethod);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                appContext = nullptr;
                log::error("OpenXR: Exception calling ActivityThread.currentApplication()");
            } else if (appContext != nullptr) {
                log::info("OpenXR: Got Android Application context via ActivityThread");
            } else {
                log::error("OpenXR: ActivityThread.currentApplication() returned null");
            }
        } else {
            env->ExceptionClear();
            log::error("OpenXR: Could not find currentApplication method");
        }
        env->DeleteLocalRef(activityThreadClass);
    } else {
        if (env->ExceptionCheck()) env->ExceptionClear();
        log::error("OpenXR: Could not find ActivityThread class");
    }
    
    if (appContext == nullptr) {
        log::error("OpenXR: No Android context available, cannot initialize OpenXR");
        if (didAttach) vm->DetachCurrentThread();
        return false;
    }
    
    // Initialize the OpenXR loader with the Android context
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&initializeLoader)))) {
        log::info("OpenXR: Found xrInitializeLoaderKHR");
        XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfoAndroid.applicationVM = vm;
        loaderInitInfoAndroid.applicationContext = appContext;
        
        XrResult initRes = initializeLoader((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
        log::info("OpenXR: initializeLoader result: {}", (int)initRes);
        if (XR_FAILED(initRes)) {
            log::error("OpenXR: Loader initialization failed");
            env->DeleteLocalRef(appContext);
            if (didAttach) vm->DetachCurrentThread();
            return false;
        }
    } else {
        log::error("OpenXR: Failed to get xrInitializeLoaderKHR proc addr");
        env->DeleteLocalRef(appContext);
        if (didAttach) vm->DetachCurrentThread();
        return false;
    }
#endif

    const char* extensions[] = {
        "XR_KHR_opengl_es_enable",
#if defined(GEODE_IS_ANDROID)
        "XR_KHR_android_create_instance"
#endif
    };

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    
#if defined(GEODE_IS_ANDROID)
    XrInstanceCreateInfoAndroidKHR androidCreateInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidCreateInfo.applicationVM = vm;
    androidCreateInfo.applicationActivity = appContext;
    
    createInfo.next = &androidCreateInfo;
#endif

    strcpy(createInfo.applicationInfo.applicationName, "Geometry Dash VR");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.enabledExtensionNames = extensions;

    XrResult res = xrCreateInstance(&createInfo, &m_instance);
    
#if defined(GEODE_IS_ANDROID)
    // Clean up JNI references now that xrCreateInstance has consumed them
    env->DeleteLocalRef(appContext);
    if (didAttach) vm->DetachCurrentThread();
#endif
    
    if (XR_FAILED(res)) {
        log::error("OpenXR: Failed to create instance, result code: {}", (int)res);
        return false;
    }
    log::info("OpenXR: Instance created successfully");

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(m_instance, &systemInfo, &m_systemId))) {
        log::error("OpenXR: Failed to get system");
        return false;
    }
    log::info("OpenXR: System retrieved successfully (ID: {})", m_systemId);

    log::info("OpenXR: Creating session...");
    if (!createSession()) {
        log::error("OpenXR: Failed to create session");
        return false;
    }
    
    log::info("OpenXR: Creating swapchain...");
    if (!createSwapchain()) {
        log::error("OpenXR: Failed to create swapchain");
        return false;
    }

    // Create reference space for tracking
    XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceCreateInfo.poseInReferenceSpace = {{0,0,0,1}, {0,0,0}};
    if (XR_FAILED(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_localSpace))) {
        log::error("Failed to create OpenXR reference space");
        return false;
    }

    m_running = true;
    return true;
}

bool OpenXRManager::createSession() {
    XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    binding.display = eglGetCurrentDisplay();
    binding.context = eglGetCurrentContext();
    
    EGLint configId;
    eglQueryContext(binding.display, binding.context, EGL_CONFIG_ID, &configId);
    
    // According to OpenXR spec, we must call xrGetOpenGLESGraphicsRequirementsKHR before xrCreateSession
    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetOpenGLESGraphicsRequirementsKHR);
    
    if (pfnGetOpenGLESGraphicsRequirementsKHR) {
        XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        XrResult reqRes = pfnGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
        if (XR_FAILED(reqRes)) {
            log::error("OpenXR: xrGetOpenGLESGraphicsRequirementsKHR failed: {}", (int)reqRes);
            return false;
        }
        log::info("OpenXR: Fetched graphics requirements successfully");
    } else {
        log::error("OpenXR: Failed to get xrGetOpenGLESGraphicsRequirementsKHR proc addr");
        return false;
    }
    
    EGLint numConfigs = 0;
    EGLConfig config = 0;
    EGLint attribs[] = { EGL_CONFIG_ID, configId, EGL_NONE };
    eglChooseConfig(binding.display, attribs, &config, 1, &numConfigs);
    
    binding.config = config;
    
    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &binding;
    createInfo.systemId = m_systemId;
    XrResult res = xrCreateSession(m_instance, &createInfo, &m_session);
    if (XR_FAILED(res)) {
        log::error("OpenXR: Failed to create session, error code: {}", (int)res);
        return false;
    }
    
    log::info("OpenXR: Session created successfully");
    return true;
}

bool OpenXRManager::createSwapchain() {
    m_eyes.resize(2);
    XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainCreateInfo.width = 2048; 
    swapchainCreateInfo.height = 2048;
    swapchainCreateInfo.format = 0x1908; // GL_RGBA
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    
    for (int i = 0; i < 2; ++i) {
        if (XR_FAILED(xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_eyes[i].swapchain))) {
            log::error("OpenXR: Failed to create swapchain for eye {}", i);
            return false;
        }
        
        uint32_t imageCount;
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, 0, &imageCount, nullptr);
        m_eyes[i].images.resize(imageCount);
        
        std::vector<XrSwapchainImageOpenGLESKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());
        for(auto& img : images) m_eyes[i].images.push_back(img.image);
    }
    log::info("OpenXR: Swapchain created successfully");
    return true;
}

bool OpenXRManager::waitFrame(XrTime* displayTime) {
    if (!m_sessionActive) return false;

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

void OpenXRManager::locateViews() {
    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = m_predictedDisplayTime;
    locateInfo.space = m_localSpace;

    XrViewState viewState{XR_TYPE_VIEW_STATE};
    uint32_t viewCountOutput;
    std::vector<XrView> views(2, {XR_TYPE_VIEW});
    xrLocateViews(m_session, &locateInfo, &viewState, 2, &viewCountOutput, views.data());

    for (int i = 0; i < 2; ++i) {
        m_eyes[i].view = views[i];
        
        // Simple perspective projection from FOV
        float nearZ = 0.1f;
        float farZ = 100.0f;
        float l = tanf(views[i].fov.angleLeft) * nearZ;
        float r = tanf(views[i].fov.angleRight) * nearZ;
        float t = tanf(views[i].fov.angleUp) * nearZ;
        float b = tanf(views[i].fov.angleDown) * nearZ;
        m_eyes[i].projection = glm::frustum(l, r, b, t, nearZ, farZ);

        // Convert pose to view matrix
        XrPosef pose = views[i].pose;
        glm::vec3 pos(pose.position.x, pose.position.y, pose.position.z);
        glm::quat rot(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
        m_eyes[i].viewMatrix = glm::translate(glm::mat4_cast(glm::conjugate(rot)), -pos);
    }
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
    XrCompositionLayerProjectionView projViews[2];
    for (int i = 0; i < 2; ++i) {
        projViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[i].pose = eyes[i].view.pose;
        projViews[i].fov = eyes[i].view.fov;
        projViews[i].subImage = {eyes[i].swapchain, {{0,0}, {2048,2048}}, 0};
    }

    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.space = m_localSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projViews;

    const XrCompositionLayerBaseHeader* layers[] = {
        (const XrCompositionLayerBaseHeader*)&projectionLayer
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    xrEndFrame(m_session, &endInfo);
}

void OpenXRManager::shutdown() {
    if (m_localSpace != XR_NULL_HANDLE) xrDestroySpace(m_localSpace);
    for (auto& eye : m_eyes) if (eye.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(eye.swapchain);
    if (m_session != XR_NULL_HANDLE) xrDestroySession(m_session);
    if (m_instance != XR_NULL_HANDLE) xrDestroyInstance(m_instance);
}

void OpenXRManager::pollEvents() {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* sessionEvent = (XrEventDataSessionStateChanged*)&event;
            if (sessionEvent->state == XR_SESSION_STATE_READY) {
                log::info("OpenXR: Session state READY, beginning session...");
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrBeginSession(m_session, &beginInfo);
                m_sessionActive = true;
            } else if (sessionEvent->state == XR_SESSION_STATE_STOPPING) {
                log::info("OpenXR: Session state STOPPING, ending session...");
                xrEndSession(m_session);
                m_sessionActive = false;
            }
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}
