#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "VRManager.hpp"
#ifdef GEODE_IS_ANDROID
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#endif

using namespace geode::prelude;

class $modify(MyEGLView, CCEGLView) {
    void swapBuffers() {
        // Only run the OpenXR render loop after the user has explicitly
        // enabled VR via the button. This avoids initialising OpenXR before
        // the Android activity lifecycle is ready.
        if (VRManager::get().isEnabled()) {
            VRManager::get().update();
        }

        // Always call the original so the flat Cocos2d view keeps rendering.
        CCEGLView::swapBuffers();
    }
};

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto menu = CCMenu::create();
        menu->setID("gdvr-menu");

        auto btnSprite = ButtonSprite::create("VR", "bigFont.fnt", "GJ_button_01.png");
        btnSprite->setScale(0.6f);
        
        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(MyMenuLayer::onToggleVR)
        );
        btn->setID("toggle-vr-button");
        menu->addChild(btn);
        
        // Position at the bottom center, slightly above the edge
        auto winSize = CCDirector::get()->getWinSize();
        menu->setPosition({ winSize.width / 2.f, 25.f });
        
        this->addChild(menu);

        return true;
    }

    void onToggleVR(CCObject* sender) {
        log::info("User clicked the Toggle VR button in MenuLayer!");
#ifdef GEODE_IS_ANDROID
        // The VR activity runs in a separate Activity so we don't break the original
        // GLSurfaceView rendering path. Tell the launcher to start the Intent.
        JNIEnv* env = nullptr;
        JavaVM* vm = cocos2d::JniHelper::getJavaVM();
        if (vm) {
            vm->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (!env) {
                vm->AttachCurrentThread(&env, nullptr);
            }
        }
        
        jobject activity = nullptr;
        if (env) {
            jclass cocosActivityClass = env->FindClass("org/cocos2dx/lib/Cocos2dxActivity");
            if (cocosActivityClass) {
                jmethodID getContext = env->GetStaticMethodID(cocosActivityClass, "getContext", "()Landroid/content/Context;");
                if (getContext) {
                    activity = env->CallStaticObjectMethod(cocosActivityClass, getContext);
                }
                env->DeleteLocalRef(cocosActivityClass);
            }
        }
        
        if (env && activity) {
            jclass activityClass = env->GetObjectClass(activity);
            jmethodID launchQuestVRMode = env->GetMethodID(activityClass, "launchQuestVRMode", "()V");
            
            if (launchQuestVRMode && !env->ExceptionCheck()) {
                log::info("Found launchQuestVRMode, calling it...");
                env->CallVoidMethod(activity, launchQuestVRMode);
            } else {
                if (env->ExceptionCheck()) env->ExceptionClear();
                log::error("Could not find launchQuestVRMode on the current Activity. Is the launcher updated?");
            }
            env->DeleteLocalRef(activityClass);
            env->DeleteLocalRef(activity);
        } else {
            log::error("Could not get JNIEnv or Activity to launch VR Intent.");
        }
        
        // This will put VRManager into a pending state where it will poll
        // until the launcher bridge successfully provides the new VR Activity.
        VRManager::get().startVR();
#else
        log::info("Toggle VR is only supported on Android (Meta Quest).");
#endif
    }
};