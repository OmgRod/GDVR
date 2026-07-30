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
        static bool firstFrame = true;
        if (firstFrame) {
            log::info("VRManager: First frame swapBuffers called");
            firstFrame = false;
        }

        // Initialize VRManager lazily on the first frame
        VRManager::get().init();

        // Run VR rendering before swapping buffers
        VRManager::get().update();
        
        // Call the original swapBuffers to show the flat mirror
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
        log::info("Attempting JNI transition to IMMERSIVE_HMD...");
        cocos2d::JniMethodInfo methodInfo;
        // 1. Get the Activity (Context) from Cocos2dxActivity
        if (cocos2d::JniHelper::getStaticMethodInfo(methodInfo, "org/cocos2dx/lib/Cocos2dxActivity", "getContext", "()Landroid/content/Context;")) {
            jobject activityObj = methodInfo.env->CallStaticObjectMethod(methodInfo.classID, methodInfo.methodID);
            methodInfo.env->DeleteLocalRef(methodInfo.classID);
            
            if (activityObj) {
                // Intent intent = new Intent(activityObj, activityObj.getClass());
                jclass intentClass = methodInfo.env->FindClass("android/content/Intent");
                jmethodID intentConstructor = methodInfo.env->GetMethodID(intentClass, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V");
                jclass activityClass = methodInfo.env->GetObjectClass(activityObj);
                
                jobject intentObj = methodInfo.env->NewObject(intentClass, intentConstructor, activityObj, activityClass);
                
                // intent.addCategory("org.khronos.openxr.intent.category.IMMERSIVE_HMD");
                jmethodID addCategoryMethod = methodInfo.env->GetMethodID(intentClass, "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;");
                jstring categoryStr = methodInfo.env->NewStringUTF("org.khronos.openxr.intent.category.IMMERSIVE_HMD");
                methodInfo.env->CallObjectMethod(intentObj, addCategoryMethod, categoryStr);
                
                // intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP); // 0x20000000
                jmethodID addFlagsMethod = methodInfo.env->GetMethodID(intentClass, "addFlags", "(I)Landroid/content/Intent;");
                methodInfo.env->CallObjectMethod(intentObj, addFlagsMethod, 0x20000000);
                
                // activityObj.startActivity(intent);
                jmethodID startActivityMethod = methodInfo.env->GetMethodID(activityClass, "startActivity", "(Landroid/content/Intent;)V");
                methodInfo.env->CallVoidMethod(activityObj, startActivityMethod, intentObj);
                
                // Clean up local references
                methodInfo.env->DeleteLocalRef(intentObj);
                methodInfo.env->DeleteLocalRef(categoryStr);
                methodInfo.env->DeleteLocalRef(activityClass);
                methodInfo.env->DeleteLocalRef(intentClass);
                methodInfo.env->DeleteLocalRef(activityObj);
                
                log::info("Successfully fired Intent to transition to IMMERSIVE_HMD");
            } else {
                log::error("getContext() returned null.");
            }
        } else {
            log::error("Failed to find getContext() on Cocos2dxActivity.");
        }
#else
        log::info("Toggle VR is only supported on Android (Meta Quest).");
#endif
    }
};