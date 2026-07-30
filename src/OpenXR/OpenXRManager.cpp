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

    // initialise() is reached only from VRManager after an explicit button
    // request. It creates a session, not a running session.
    m_running = false;
    m_exitRequested = false;

    JavaVM* vm = cocos2d::JniHelper::getJavaVM();
    if (!vm) {
        log::error("OpenXR: No JavaVM");
        return false;
    }

    JNIEnv* env = nullptr;
    bool attached = false;

    jint result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    if (result == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            log::error("OpenXR: Failed attaching JNI");
            return false;
        }
        attached = true;
    }

    if (!env) {
        log::error("OpenXR: Invalid JNIEnv");
        return false;
    }


    // Geode 5.8.2 exposes getJavaVM(), but not JniHelper::getActivity().
    // Cocos2dxHelper keeps the real Activity in a static field after startup.
    jobject activity = nullptr;
    bool hasActivity = false;

    // Older Cocos2d-x builds keep the activity in this field.
    jclass helperClass = env->FindClass("org/cocos2dx/lib/Cocos2dxHelper");
    if (helperClass && !env->ExceptionCheck()) {
        jfieldID activityField = env->GetStaticFieldID(
            helperClass, "sActivity", "Landroid/app/Activity;"
        );
        if (activityField && !env->ExceptionCheck()) {
            activity = env->GetStaticObjectField(helperClass, activityField);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(helperClass);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    // Geometry Dash's Cocos build does not expose Cocos2dxHelper.sActivity.
    // Its public getContext() returns the actual Cocos2dxActivity instance.
    if (!activity) {
        jclass activityClass = env->FindClass("org/cocos2dx/lib/Cocos2dxActivity");
        if (activityClass && !env->ExceptionCheck()) {
            jmethodID getContext = env->GetStaticMethodID(
                activityClass, "getContext", "()Landroid/content/Context;"
            );
            if (getContext && !env->ExceptionCheck()) {
                activity = env->CallStaticObjectMethod(activityClass, getContext);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(activityClass);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    if (activity) {
        jclass androidActivityClass = env->FindClass("android/app/Activity");
        hasActivity = androidActivityClass && env->IsInstanceOf(activity, androidActivityClass);
        if (androidActivityClass) env->DeleteLocalRef(androidActivityClass);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!hasActivity) {
            env->DeleteLocalRef(activity);
            activity = nullptr;
        }
    }

    // Geometry Dash's bundled Cocos Java classes expose neither of the usual
    // activity accessors above. Obtain its currently-created Activity from
    // ActivityThread before falling back to the process application object.
    if (!activity) {
        jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
        if (activityThreadClass && !env->ExceptionCheck()) {
            jmethodID currentActivityThread = env->GetStaticMethodID(
                activityThreadClass,
                "currentActivityThread",
                "()Landroid/app/ActivityThread;"
            );
            jobject activityThread = nullptr;
            if (currentActivityThread && !env->ExceptionCheck()) {
                activityThread = env->CallStaticObjectMethod(
                    activityThreadClass, currentActivityThread
                );
            }
            if (env->ExceptionCheck()) env->ExceptionClear();

            // Use java.lang.reflect.Field so this continues to work whether
            // mActivities is backed by ArrayMap or another Map implementation.
            auto getPrivateField = [&](jobject object, const char* name) -> jobject {
                if (!object) return nullptr;

                jclass objectClass = env->GetObjectClass(object);
                jclass classClass = env->FindClass("java/lang/Class");
                jclass fieldClass = env->FindClass("java/lang/reflect/Field");
                if (!objectClass || !classClass || !fieldClass || env->ExceptionCheck()) {
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (objectClass) env->DeleteLocalRef(objectClass);
                    if (classClass) env->DeleteLocalRef(classClass);
                    if (fieldClass) env->DeleteLocalRef(fieldClass);
                    return nullptr;
                }

                jmethodID getDeclaredField = env->GetMethodID(
                    classClass, "getDeclaredField",
                    "(Ljava/lang/String;)Ljava/lang/reflect/Field;"
                );
                jmethodID setAccessible = env->GetMethodID(fieldClass, "setAccessible", "(Z)V");
                jmethodID get = env->GetMethodID(fieldClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
                jstring fieldName = env->NewStringUTF(name);
                jobject field = nullptr;
                jobject value = nullptr;
                if (getDeclaredField && setAccessible && get && fieldName && !env->ExceptionCheck()) {
                    field = env->CallObjectMethod(objectClass, getDeclaredField, fieldName);
                    if (field && !env->ExceptionCheck()) {
                        env->CallVoidMethod(field, setAccessible, JNI_TRUE);
                        if (!env->ExceptionCheck()) value = env->CallObjectMethod(field, get, object);
                    }
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (field) env->DeleteLocalRef(field);
                if (fieldName) env->DeleteLocalRef(fieldName);
                env->DeleteLocalRef(objectClass);
                env->DeleteLocalRef(classClass);
                env->DeleteLocalRef(fieldClass);
                return value;
            };

            jobject activityMap = getPrivateField(activityThread, "mActivities");
            if (activityMap) {
                jclass mapClass = env->FindClass("java/util/Map");
                jclass collectionClass = env->FindClass("java/util/Collection");
                jclass iteratorClass = env->FindClass("java/util/Iterator");
                jclass androidActivityClass = env->FindClass("android/app/Activity");
                if (mapClass && collectionClass && iteratorClass && androidActivityClass &&
                    env->IsInstanceOf(activityMap, mapClass)) {
                    jmethodID values = env->GetMethodID(mapClass, "values", "()Ljava/util/Collection;");
                    jmethodID iterator = env->GetMethodID(collectionClass, "iterator", "()Ljava/util/Iterator;");
                    jmethodID hasNext = env->GetMethodID(iteratorClass, "hasNext", "()Z");
                    jmethodID next = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
                    if (values && iterator && hasNext && next && !env->ExceptionCheck()) {
                        jobject records = env->CallObjectMethod(activityMap, values);
                        jobject recordIterator = records ? env->CallObjectMethod(records, iterator) : nullptr;
                        while (recordIterator && !env->ExceptionCheck() &&
                               env->CallBooleanMethod(recordIterator, hasNext) == JNI_TRUE) {
                            jobject record = env->CallObjectMethod(recordIterator, next);
                            jobject candidate = getPrivateField(record, "activity");
                            if (!candidate) candidate = getPrivateField(record, "mActivity");
                            if (candidate && env->IsInstanceOf(candidate, androidActivityClass)) {
                                activity = candidate;
                                hasActivity = true;
                                if (record) env->DeleteLocalRef(record);
                                break;
                            }
                            if (candidate) env->DeleteLocalRef(candidate);
                            if (record) env->DeleteLocalRef(record);
                        }
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (recordIterator) env->DeleteLocalRef(recordIterator);
                        if (records) env->DeleteLocalRef(records);
                    } else if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                    }
                } else if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                if (mapClass) env->DeleteLocalRef(mapClass);
                if (collectionClass) env->DeleteLocalRef(collectionClass);
                if (iteratorClass) env->DeleteLocalRef(iteratorClass);
                if (androidActivityClass) env->DeleteLocalRef(androidActivityClass);
                env->DeleteLocalRef(activityMap);
            }
            if (activityThread) env->DeleteLocalRef(activityThread);

            // Preserve the old, working initialization path if no Activity
            // record was available on this Android version.
            if (!activity) {
                jmethodID currentApplication = env->GetStaticMethodID(
                    activityThreadClass,
                    "currentApplication",
                    "()Landroid/app/Application;"
                );
                if (currentApplication && !env->ExceptionCheck()) {
                    activity = env->CallStaticObjectMethod(activityThreadClass, currentApplication);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(activityThreadClass);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    if (!activity) {
        log::error("OpenXR: Could not obtain an Android context");
        if (attached) vm->DetachCurrentThread();
        return false;
    }

    if (hasActivity) {
        log::info("OpenXR: Android Activity acquired");
    } else {
        log::info("OpenXR: Using Android application context fallback");
    }


    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;

    XrResult loaderResult =
        xrGetInstanceProcAddr(
            XR_NULL_HANDLE,
            "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)
        );


    if (XR_FAILED(loaderResult) || !initializeLoader) {
        log::error("OpenXR: Missing xrInitializeLoaderKHR");
        return false;
    }


    XrLoaderInitInfoAndroidKHR loaderInfo{
        XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR
    };

    loaderInfo.applicationVM = vm;
    loaderInfo.applicationContext = activity;


    XrResult initResult =
        initializeLoader(
            reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)
        );


    log::info(
        "OpenXR: Loader init result {}",
        (int)initResult
    );


    if (XR_FAILED(initResult)) {
        log::error("OpenXR: Loader failed");
        env->DeleteLocalRef(activity);
        if (attached) vm->DetachCurrentThread();
        return false;
    }


    const char* extensions[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME
    };


    XrInstanceCreateInfo createInfo{
        XR_TYPE_INSTANCE_CREATE_INFO
    };


    XrInstanceCreateInfoAndroidKHR androidInfo{
        XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR
    };


    androidInfo.applicationVM = vm;
    androidInfo.applicationActivity = activity;


    createInfo.next = &androidInfo;

    strcpy(
        createInfo.applicationInfo.applicationName,
        "Geometry Dash VR"
    );

    createInfo.applicationInfo.apiVersion =
        XR_CURRENT_API_VERSION;


    createInfo.enabledExtensionCount =
        sizeof(extensions) / sizeof(char*);

    createInfo.enabledExtensionNames =
        extensions;



    XrResult res =
        xrCreateInstance(
            &createInfo,
            &m_instance
        );


    env->DeleteLocalRef(activity);
    if (attached) vm->DetachCurrentThread();


    if (XR_FAILED(res)) {
        log::error(
            "OpenXR: xrCreateInstance failed {}",
            (int)res
        );
        return false;
    }


    log::info("OpenXR: Instance created");


    XrSystemGetInfo systemInfo{
        XR_TYPE_SYSTEM_GET_INFO
    };

    systemInfo.formFactor =
        XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;


    res =
        xrGetSystem(
            m_instance,
            &systemInfo,
            &m_systemId
        );


    if (XR_FAILED(res)) {
        log::error("OpenXR: xrGetSystem failed");
        return false;
    }


    if (!createSession())
        return false;


    if (!createSwapchain())
        return false;



    XrReferenceSpaceCreateInfo spaceInfo{
        XR_TYPE_REFERENCE_SPACE_CREATE_INFO
    };


    spaceInfo.referenceSpaceType =
        XR_REFERENCE_SPACE_TYPE_LOCAL;


    spaceInfo.poseInReferenceSpace.orientation.w = 1;


    res =
        xrCreateReferenceSpace(
            m_session,
            &spaceInfo,
            &m_localSpace
        );


    if (XR_FAILED(res)) {
        log::error("OpenXR: Failed creating space");
        return false;
    }


    log::info(
        "OpenXR: Initialised successfully"
    );


    return true;


#else

    return false;

#endif
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
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data());
    
    int64_t selectedFormat = 0;
    for (int64_t f : formats) {
        if (f == 0x8058 /* GL_RGBA8 */ || f == 0x8C43 /* GL_SRGB8_ALPHA8 */) {
            selectedFormat = f;
            break;
        }
    }
    if (selectedFormat == 0 && formatCount > 0) {
        selectedFormat = formats[0]; // fallback to first supported
    }

    XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainCreateInfo.width = 2048; 
    swapchainCreateInfo.height = 2048;
    swapchainCreateInfo.format = selectedFormat;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.sampleCount = 1;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.arraySize = 1;
    swapchainCreateInfo.mipCount = 1;
    
    for (int i = 0; i < 2; ++i) {
        XrResult res = xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_eyes[i].swapchain);
        if (XR_FAILED(res)) {
            log::error("OpenXR: Failed to create swapchain for eye {}, error code: {}", i, (int)res);
            return false;
        }
        
        uint32_t imageCount;
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, 0, &imageCount, nullptr);
        m_eyes[i].images.clear();
        m_eyes[i].images.reserve(imageCount);
        
        std::vector<XrSwapchainImageOpenGLESKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        xrEnumerateSwapchainImages(m_eyes[i].swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());
        for(auto& img : images) m_eyes[i].images.push_back(img.image);
    }
    log::info("OpenXR: Swapchain created successfully");
    return true;
}

bool OpenXRManager::waitFrame(bool* shouldRender, XrTime* displayTime) {
    if (!m_running) return false;

    XrFrameWaitInfo info{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState state{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(m_session, &info, &state))) return false;
    m_predictedDisplayTime = state.predictedDisplayTime;
    *displayTime = m_predictedDisplayTime;
    *shouldRender = (state.shouldRender == XR_TRUE);
    return true;
}

bool OpenXRManager::beginFrame() {
    XrFrameBeginInfo info{XR_TYPE_FRAME_BEGIN_INFO};
    const XrResult result = xrBeginFrame(m_session, &info);
    if (XR_FAILED(result)) {
        log::error("OpenXR: xrBeginFrame failed: {}", static_cast<int>(result));
        return false;
    }
    return true;
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

void OpenXRManager::submitEmptyFrame() {
    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 0;
    endInfo.layers = nullptr;

    xrEndFrame(m_session, &endInfo);
}

void OpenXRManager::shutdown() {
    if (m_localSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_localSpace);
        m_localSpace = XR_NULL_HANDLE;
    }
    for (auto& eye : m_eyes) {
        if (eye.swapchain != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.swapchain);
            eye.swapchain = XR_NULL_HANDLE;
        }
    }
    m_eyes.clear();
    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    if (m_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
    }
    m_systemId = XR_NULL_SYSTEM_ID;
    m_predictedDisplayTime = 0;
    m_running = false;
    m_exitRequested = false;
}

void OpenXRManager::pollEvents(bool wantsRunning) {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    XrResult pollRes;
    
    while ((pollRes = xrPollEvent(m_instance, &event)) == XR_SUCCESS) {
        log::info("OpenXR: Received event type: {}", (int)event.type);
        
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto* sessionEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
            log::info("OpenXR: Session state changed to {}", (int)sessionEvent->state);
            
            if (sessionEvent->state == XR_SESSION_STATE_READY && wantsRunning && !m_running) {
                log::info("OpenXR: Session state READY, beginning session...");
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrResult beginRes = xrBeginSession(m_session, &beginInfo);
                if (XR_FAILED(beginRes)) {
                    log::error("OpenXR: Failed to begin session, error code: {}", (int)beginRes);
                } else {
                    log::info("OpenXR: Session begun successfully");
                    m_running = true;
                }
            } else if (sessionEvent->state == XR_SESSION_STATE_READY) {
                log::info("OpenXR: READY received with VR disabled; not beginning session");
            } else if (sessionEvent->state == XR_SESSION_STATE_VISIBLE) {
                log::info("OpenXR: Session is visible");
            } else if (sessionEvent->state == XR_SESSION_STATE_FOCUSED) {
                log::info("OpenXR: Session is focused");
            } else if (sessionEvent->state == XR_SESSION_STATE_STOPPING) {
                log::info("OpenXR: Session state STOPPING, ending session...");
                if (m_running) {
                    XrResult endRes = xrEndSession(m_session);
                    if (XR_FAILED(endRes)) {
                        log::error("OpenXR: xrEndSession failed: {}", static_cast<int>(endRes));
                    }
                }
                m_running = false;
            } else if (sessionEvent->state == XR_SESSION_STATE_EXITING || sessionEvent->state == XR_SESSION_STATE_LOSS_PENDING) {
                log::info("OpenXR: Session exiting or loss pending");
                m_running = false;
                m_exitRequested = true;
            }
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
    
    if (pollRes != XR_EVENT_UNAVAILABLE) {
        log::error("OpenXR: xrPollEvent failed with error code: {}", (int)pollRes);
    }
    
    static int frameCounter = 0;
    if (!m_running) {
        if (++frameCounter % 60 == 0) {
            log::info("OpenXR: Event loop pumping, waiting for session to become READY... (Currently not active)");
        }
    } else {
        frameCounter = 0;
    }
}
