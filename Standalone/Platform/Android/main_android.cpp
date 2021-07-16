/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AtomSampleViewerApplication.h"

#include <AzGameFramework/Application/GameApplication.h>

#include <GridMate/GridMate.h>
#include <GridMate/Session/LANSession.h>

#include <AzCore/Android/Utils.h>

#include <AzCore/Android/AndroidEnv.h>

#include <AzFramework/API/ApplicationAPI_android.h>

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <android_native_app_glue.h>

#include <errno.h>

#if defined(AZ_ENABLE_TRACING)
    #define LOG_TAG "LMBR"
    #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
    #define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#else
    #define LOGI(...)
    #define LOGE(...)
#endif // defined(AZ_ENABLE_TRACING)

#define MAIN_EXIT_FAILURE(_appState, ...) \
    LOGE("****************************************************************"); \
    LOGE("STARTUP FAILURE - EXITING"); \
    LOGE("REASON:"); \
    LOGE(__VA_ARGS__); \
    LOGE("****************************************************************"); \
    _appState->userData = nullptr; \
    ANativeActivity_finish(_appState->activity); \
    while (_appState->destroyRequested == 0) { \
        PumpEvents(_appState); \
    } \
    return;


namespace
{
    bool g_windowInitialized = false;

    //////////////////////////////////////////////////////////////////////////
    void PumpEvents(android_app* appState)
    {
        int events;
        android_poll_source* source;
        while (ALooper_pollAll(0, NULL, &events, reinterpret_cast<void**>(&source)) >= 0)
        {
            if (source != NULL)
            {
                source->process(appState, source);
            }

            if (appState->destroyRequested != 0)
            {
                break;
            }
        }
    }

    // ----
    // System handlers
    // ----

    //////////////////////////////////////////////////////////////////////////
    // this callback is triggered on the same thread the events are pumped
    void HandleApplicationLifecycleEvents(android_app* appState, int32_t command)
    {
    #if defined(AZ_ENABLE_TRACING)
        const char* commandNames[] = {
            "APP_CMD_INPUT_CHANGED",
            "APP_CMD_INIT_WINDOW",
            "APP_CMD_TERM_WINDOW",
            "APP_CMD_WINDOW_RESIZED",
            "APP_CMD_WINDOW_REDRAW_NEEDED",
            "APP_CMD_CONTENT_RECT_CHANGED",
            "APP_CMD_GAINED_FOCUS",
            "APP_CMD_LOST_FOCUS",
            "APP_CMD_CONFIG_CHANGED",
            "APP_CMD_LOW_MEMORY",
            "APP_CMD_START",
            "APP_CMD_RESUME",
            "APP_CMD_SAVE_STATE",
            "APP_CMD_PAUSE",
            "APP_CMD_STOP",
            "APP_CMD_DESTROY",
        };
        LOGI("Engine command received: %s", commandNames[command]);
    #endif

        AZ::Android::AndroidEnv* androidEnv = static_cast<AZ::Android::AndroidEnv*>(appState->userData);
        if (!androidEnv)
        {
            return;
        }

        switch (command)
        {
            case APP_CMD_GAINED_FOCUS:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnGainedFocus);
            }
            break;

            case APP_CMD_LOST_FOCUS:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnLostFocus);
            }
            break;

            case APP_CMD_PAUSE:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnPause);
                androidEnv->SetIsRunning(false);
            }
            break;

            case APP_CMD_RESUME:
            {
                androidEnv->SetIsRunning(true);
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnResume);
            }
            break;

            case APP_CMD_DESTROY:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnDestroy);
            }
            break;

            case APP_CMD_INIT_WINDOW:
            {
                g_windowInitialized = true;
                androidEnv->SetWindow(appState->window);

                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnWindowInit);
            }
            break;

            case APP_CMD_TERM_WINDOW:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnWindowDestroy);

                androidEnv->SetWindow(nullptr);
            }
            break;

            case APP_CMD_LOW_MEMORY:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnLowMemory);
            }
            break;

            case APP_CMD_CONFIG_CHANGED:
            {
                androidEnv->UpdateConfiguration();
            }
            break;

            case APP_CMD_WINDOW_REDRAW_NEEDED:
            {
                EBUS_EVENT(AzFramework::AndroidLifecycleEvents::Bus, OnWindowRedrawNeeded);
            }
            break;

            default:
            {
                LOGE("Invalid command received %d", command);
            }
            break;
        }
    }

    void OnWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* rect)
    {
        android_app* app = static_cast<android_app*>(activity->instance);
        int8_t cmd = APP_CMD_WINDOW_REDRAW_NEEDED;
        if (write(app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
        {
            LOGE("Failure writing android_app cmd: %s\n", strerror(errno));
        }
    }
}

void android_init(android_app* appState)
{
    // setup the system command handler
    appState->onAppCmd = HandleApplicationLifecycleEvents;

    // This callback will notify us when the orientation of the device changes.
    // While Android does have an onNativeWindowResized callback, it is never called in android_native_app_glue when the window size changes.
    // The onNativeConfigChanged callback is called too early(before the window size has changed), so we won't have the correct window size at that point.
    appState->activity->callbacks->onNativeWindowRedrawNeeded = OnWindowRedrawNeeded;

    // setup the android environment
    AZ::AllocatorInstance<AZ::OSAllocator>::Create();
    {
        AZ::Android::AndroidEnv::Descriptor descriptor;

        descriptor.m_jvm = appState->activity->vm;
        descriptor.m_activityRef = appState->activity->clazz;
        descriptor.m_assetManager = appState->activity->assetManager;

        descriptor.m_configuration = appState->config;

        descriptor.m_appPrivateStoragePath = appState->activity->internalDataPath;
        descriptor.m_appPublicStoragePath = appState->activity->externalDataPath;
        descriptor.m_obbStoragePath = appState->activity->obbPath;

        if (!AZ::Android::AndroidEnv::Create(descriptor))
        {
            AZ::Android::AndroidEnv::Destroy();
            AZ::AllocatorInstance<AZ::OSAllocator>::Destroy();
            MAIN_EXIT_FAILURE(appState, "Failed to create the AndroidEnv");
        }

        AZ::Android::AndroidEnv* androidEnv = AZ::Android::AndroidEnv::Get();
        appState->userData = androidEnv;
        androidEnv->SetIsRunning(true);
    }

    // sync the window creation
    {
        // While not ideal to have the event pump code duplicated here and in AzFramework, this
        // at least solves the splash screen issues when the window creation sync happened later
        // in initialization.  It's also the lesser of 2 evils, the other requiring a change in
        // how the platform specific private Application implementation is created for ALL
        // platforms
        while (!g_windowInitialized)
        {
            PumpEvents(appState);
        }
    }
}

void android_shutdown(android_app* appState)
{
    AZ::Android::AndroidEnv::Destroy();
    AZ::AllocatorInstance<AZ::OSAllocator>::Destroy();
}

void android_main(android_app* appState)
{
    android_init(appState);
    // Android doesn't have command line arguments.
    RunGameCommon(0, nullptr, [appState](){
        // This need to called after the AzFramework is started.
        AzFramework::AndroidAppRequests::Bus::Broadcast(&AzFramework::AndroidAppRequests::SetAppState, reinterpret_cast<android_app*>(appData));
    });
    android_shutdown(appState);
}
