/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AtomSampleViewerRequestBus.h>
#include <SampleComponentManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Asset/AssetSystemBus.h>

#include <AzFramework/Application/Application.h>
#include <AzFramework/Logging/LogFile.h>
#include <AzCore/Debug/TraceMessageBus.h>

namespace AtomSampleViewer
{
    class AtomSampleViewerApplication final
        : public AzFramework::Application
        , public AzFramework::AssetSystemStatusBus::Handler
        , public AZ::Debug::TraceMessageBus::Handler
        , public AtomSampleViewerRequestsBus::Handler
        , public SampleComponentManagerNotificationBus::Handler
    {
    public:
        AtomSampleViewerApplication();
        AtomSampleViewerApplication(int* argc, char*** argv);
        ~AtomSampleViewerApplication() override;

        void Destroy() override;

        int GetExitCode() const { return m_exitCode; }

    private:
        //////////////////////////////////////////////////////////////////////////
        // AzFramework::AssetSystemStatusBus::Handler
        void AssetSystemAvailable() override;

        bool ConnectAssetProcessor(AZStd::chrono::duration<float> timeout);
        void DisconnectAssetProcessor();

        void CompileCriticalAssets();

        void StartCommon(AZ::Entity* systemEntity) override;
        void Tick(float deltaOverride) override;

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::ApplicationRequests::Bus
        void QueryApplicationType(AZ::ApplicationTypeQuery& appType) const override;
        //////////////////////////////////////////////////////////////////////////

        static const bool s_connectToAssetProcessor;

        void ReadTimeoutShutdown();
        void TickTimeoutShutdown(float deltaTimeInSeconds);
        float m_secondsBeforeShutdown = 0.0f; // >0.0f If timeout shutdown is enabled, this will count down the time until quit() is called.

        //////////////////////////// Logging section /////////////////////////////

        // TraceMessageBus ...
        bool OnOutput(const char* window, const char* message) override;

        // AtomSampleViewerRequestBus ...
        void SetExitCode(int exitCode) override { m_exitCode = exitCode; }

        // SampleComponentManagerNotificationBus ...
        void OnSampleManagerActivated() override;

        void WriteStartupLog();
        void ReadAutomatedTestOptions();

        struct LogMessage
        {
            AZStd::string window;
            AZStd::string message;
        };

        AZStd::unique_ptr<AZStd::vector<LogMessage>> m_startupLogSink;
        AZStd::unique_ptr<AzFramework::LogFile> m_logFile;
        static constexpr const char* s_logFileBaseName = "AtomSampleViewer.log";

        int m_exitCode = 0;

        void SetupConsoleHandlerRoutine();
    };

    int RunGameCommon(int argc, char** argv, AZStd::function<void()> customRunCode = nullptr);
} // namespace AtomSampleViewer
