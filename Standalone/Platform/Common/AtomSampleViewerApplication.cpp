/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerApplication.h>
#include <SampleComponentManagerBus.h>

#include <AzCore/PlatformIncl.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Debug/Trace.h>

#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Network/AssetProcessorConnection.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <GridMate/Drillers/CarrierDriller.h>
#include <GridMate/Drillers/ReplicaDriller.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <AzCore/Math/Random.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/Settings/SettingsRegistryMergeUtils.h>
#include <AzCore/Utils/Utils.h>
#include <AzGameFramework/Application/GameApplication.h>
#include <GridMate/GridMate.h>

namespace AtomSampleViewer
{
    //! This function returns the build system target name of "AtomSampleViewerStandalone"
    AZStd::string_view GetBuildTargetName()
    {
#if !defined (LY_CMAKE_TARGET)
#error "LY_CMAKE_TARGET must be defined in order to add this source file to a CMake executable target"
#endif
        return AZStd::string_view{ LY_CMAKE_TARGET };
    }

    AtomSampleViewerApplication::AtomSampleViewerApplication()
    {
        AZ::Debug::Trace::Instance().Init();
#if defined(AZ_DEBUG_BUILD) || defined(AZ_PROFILE_BUILD)
        AZ::Debug::Trace::SetAssertVerbosityLevel(2);   //enable break on AZ_Assert()
#endif
        m_startupLogSink = AZStd::make_unique<AZStd::vector<LogMessage>>();
        AZ::Debug::TraceMessageBus::Handler::BusConnect();
        AtomSampleViewerRequestsBus::Handler::BusConnect();
        SampleComponentManagerNotificationBus::Handler::BusConnect();

        AZ::SettingsRegistryMergeUtils::MergeSettingsToRegistry_AddBuildSystemTargetSpecialization(
            *AZ::SettingsRegistry::Get(), GetBuildTargetName());
    }

    AtomSampleViewerApplication::AtomSampleViewerApplication(int* argc, char*** argv)
        : Application(argc, argv)
    {
        AZ::Debug::Trace::Instance().Init();
#if defined(AZ_DEBUG_BUILD) || defined(AZ_PROFILE_BUILD)
        AZ::Debug::Trace::SetAssertVerbosityLevel(2);   //enable break on AZ_Assert()
#endif

        m_startupLogSink = AZStd::make_unique<AZStd::vector<LogMessage>>();
        AZ::Debug::TraceMessageBus::Handler::BusConnect();
        AtomSampleViewerRequestsBus::Handler::BusConnect();
        SampleComponentManagerNotificationBus::Handler::BusConnect();

        SetupConsoleHandlerRoutine();

        AZ::SettingsRegistryMergeUtils::MergeSettingsToRegistry_AddBuildSystemTargetSpecialization(
            *AZ::SettingsRegistry::Get(), GetBuildTargetName());
    }

    AtomSampleViewerApplication::~AtomSampleViewerApplication()
    {
        AZ::Debug::TraceMessageBus::Handler::BusDisconnect();
        AtomSampleViewerRequestsBus::Handler::BusDisconnect();
        SampleComponentManagerNotificationBus::Handler::BusDisconnect();
        if (s_connectToAssetProcessor)
        {
            AzFramework::AssetSystemRequestBus::Broadcast(&AzFramework::AssetSystem::AssetSystemRequests::StartDisconnectingAssetProcessor);
        }
    }

    void AtomSampleViewerApplication::StartCommon(AZ::Entity* systemEntity)
    {
        AzFramework::AssetSystemStatusBus::Handler::BusConnect();

        AzFramework::Application::StartCommon(systemEntity);

        if (GetDrillerManager())
        {
            GetDrillerManager()->Register(aznew GridMate::Debug::CarrierDriller());
            GetDrillerManager()->Register(aznew GridMate::Debug::ReplicaDriller());
        }

        ReadTimeoutShutdown();

        WriteStartupLog();
    }

    void AtomSampleViewerApplication::OnSampleManagerActivated()
    {
        ReadAutomatedTestOptions();
    }

    void AtomSampleViewerApplication::WriteStartupLog()
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        AZ_Assert(fileIO != nullptr, "FileIO should be running at this point");

        // There is no log system online so we have to create your own log file.
        char resolveBuffer[AZ_MAX_PATH_LEN] = { 0 };
        fileIO->ResolvePath("@user@", resolveBuffer, AZ_MAX_PATH_LEN);

        // Note: @log@ hasn't been set at this point in the standalone AtomSampleViewer
        AZStd::string logDirectory;
        AzFramework::StringFunc::Path::Join(resolveBuffer, "log", logDirectory);
        fileIO->SetAlias("@log@", logDirectory.c_str());

        fileIO->CreatePath("@root@");
        fileIO->CreatePath("@user@");
        fileIO->CreatePath("@log@");

        AZStd::string logPath;
        AzFramework::StringFunc::Path::Join(logDirectory.c_str(), s_logFileBaseName, logPath);

        using namespace AzFramework;
        m_logFile.reset(aznew LogFile(logPath.c_str()));
        if (m_logFile)
        {
            m_logFile->SetMachineReadable(false);
            for (const LogMessage& message : *m_startupLogSink)
            {
                m_logFile->AppendLog(AzFramework::LogFile::SEV_NORMAL, message.window.c_str(), message.message.c_str());
            }
            m_startupLogSink->clear();
            m_logFile->FlushLog();
        }
    }

    void AtomSampleViewerApplication::ReadAutomatedTestOptions()
    {
        if (GetArgC() != nullptr && GetArgV() != nullptr)
        {
            AzFramework::CommandLine commandLine;
            commandLine.Parse(*GetArgC(), *GetArgV());

            constexpr const char* testSuiteSwitch = "runtestsuite";
            constexpr const char* testExitSwitch = "exitontestend";
            constexpr const char* testRandomSeed = "randomtestseed";

            bool exitOnTestEnd = commandLine.HasSwitch(testExitSwitch);

            if (commandLine.HasSwitch(testSuiteSwitch))
            {
                const AZStd::string& testSuitePath = commandLine.GetSwitchValue(testSuiteSwitch, 0);

                int randomSeed = 0;
                if (commandLine.HasSwitch(testRandomSeed))
                {
                    randomSeed = atoi(commandLine.GetSwitchValue(testRandomSeed, 0).c_str());
                }

                SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequestBus::Events::RunMainTestSuite, testSuitePath, exitOnTestEnd, randomSeed);
            }
        }
    }

    bool AtomSampleViewerApplication::OnOutput(const char* window, const char* message)
    {
        if (m_logFile)
        {
            m_logFile->AppendLog(AzFramework::LogFile::SEV_NORMAL, window, message);
        }
        else if (m_startupLogSink)
        {
            m_startupLogSink->push_back({ window, message });
        }

        return false;
    }

    void AtomSampleViewerApplication::Destroy()
    {
        m_logFile.reset();
        m_startupLogSink.reset();

        if (s_connectToAssetProcessor)
        {
            AzFramework::AssetSystemRequestBus::Broadcast(&AzFramework::AssetSystem::AssetSystemRequests::StartDisconnectingAssetProcessor);
        }

        Application::Destroy();
    }

    void AtomSampleViewerApplication::Tick(float deltaOverride)
    {
        TickSystem();
        Application::Tick(deltaOverride);
        TickTimeoutShutdown(m_deltaTime); // deltaOverride comes in as -1.f in AtomSampleViewerApplication
    }

    void AtomSampleViewerApplication::ReadTimeoutShutdown()
    {
        if (m_commandLine.HasSwitch("timeout"))
        {
            const AZStd::string& timeoutValue = m_commandLine.GetSwitchValue("timeout", 0);
            const float timeoutInSeconds = static_cast<float>(atoi(timeoutValue.c_str()));
            AZ_Printf("AtomSampleViewer", "starting up with timeout shutdown of %f seconds", timeoutInSeconds);
            m_secondsBeforeShutdown = timeoutInSeconds;
        }
    }

    void AtomSampleViewerApplication::TickTimeoutShutdown(float deltaTimeInSeconds)
    {
        if (m_secondsBeforeShutdown > 0.f)
        {
            m_secondsBeforeShutdown -= deltaTimeInSeconds;
            if (m_secondsBeforeShutdown <= 0.f)
            {
                AZ_Printf("AtomSampleViewer", "Timeout reached, shutting down");
                AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::ExitMainLoop); // or ::TerminateOnError for a more forceful option
            }
        }
    }

    void AtomSampleViewerApplication::AssetSystemAvailable()
    {
        if (s_connectToAssetProcessor)
        {
            bool connectedToAssetProcessor = false;

            // When the AssetProcessor is already launched it should take less than a second to perform a connection
            // but when the AssetProcessor needs to be launch it could take up to 15 seconds to have the AssetProcessor initialize
            // and able to negotiate a connection when running a debug build
            // and to negotiate a connection

            AzFramework::AssetSystem::ConnectionSettings connectionSettings;
            AzFramework::AssetSystem::ReadConnectionSettingsFromSettingsRegistry(connectionSettings);
            connectionSettings.m_connectionDirection = AzFramework::AssetSystem::ConnectionSettings::ConnectionDirection::ConnectToAssetProcessor;
            connectionSettings.m_connectionIdentifier = "AtomSampleViewer";
            connectionSettings.m_loggingCallback = []([[maybe_unused]] AZStd::string_view logData)
            {
                AZ_TracePrintf("AtomSampleViewer", "%.*s", aznumeric_cast<int>(logData.size()), logData.data());
            };
            AzFramework::AssetSystemRequestBus::BroadcastResult(connectedToAssetProcessor,
                &AzFramework::AssetSystemRequestBus::Events::EstablishAssetProcessorConnection, connectionSettings);
            if (connectedToAssetProcessor)
            {
                CompileCriticalAssets();
            }
        }

        AzFramework::AssetSystemStatusBus::Handler::BusDisconnect();
    }

    void AtomSampleViewerApplication::CompileCriticalAssets()
    {
        // Critical assets for AtomSampleViewer application. They only matter if the application run with AP connection.
        const char AssetPaths[][128] =
        {
            "Shaders/imgui/imgui.azshader",
            "Shaders/auxgeom/auxgeomworld.azshader",
            "Shaders/auxgeom/auxgeomobject.azshader",
            "Shaders/auxgeom/auxgeomobjectlit.azshader"
        };

        const uint32_t AssetCount = sizeof(AssetPaths) / sizeof(AssetPaths[0]);

        // Wait for AP initial scan before we can request compile and sync asset
        // The following implementation is similar to CSystem::WaitForAssetProcessorToBeReady()
        bool isAssetProcessorReady = false;
        AzFramework::AssetSystem::RequestAssetProcessorStatus request;
        request.m_platform = "pc";
        while (!isAssetProcessorReady)
        {
            AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::PumpSystemEventLoopUntilEmpty);

            AzFramework::AssetSystem::ResponseAssetProcessorStatus response;
            if (!AzFramework::AssetSystem::SendRequest(request, response))
            {
                AZ_Warning("AtomSampleViewerApplication", false, "Failed to send Asset Processor Status request for platform %s.", request.m_platform.c_str());
                return;
            }
            else
            {
                if (response.m_isAssetProcessorReady)
                {
                    isAssetProcessorReady = true;
                }
            }

            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
        }

        // Force AP to compile assets and wait for them
        // Note: with AssetManager's current implementation, a compiled asset won't be added in asset registry until next system tick.
        // So the asset id won't be found right after CompileAssetSync call.
        for (uint32_t assetIdx = 0; assetIdx < AssetCount; assetIdx++)
        {
            // Wait for the shader asset be compiled
            AzFramework::AssetSystem::AssetStatus status = AzFramework::AssetSystem::AssetStatus_Unknown;
            AzFramework::AssetSystemRequestBus::BroadcastResult(
                status, &AzFramework::AssetSystemRequestBus::Events::CompileAssetSync, AssetPaths[assetIdx]);

            if (status != AzFramework::AssetSystem::AssetStatus_Compiled)
            {
                AZ_Error("AtomSampleViewerApplication", false, "Shader asset [%s] error %d", AssetPaths[assetIdx], status);
            }
        }
    }

    void AtomSampleViewerApplication::QueryApplicationType(AZ::ApplicationTypeQuery& appType) const
    {
        appType.m_maskValue = AZ::ApplicationTypeQuery::Masks::Game;
    };

    int RunGameCommon(int argc, char** argv, AZStd::function<void()> customRunCode)
    {
        AtomSampleViewer::AtomSampleViewerApplication app(&argc, &argv);

        const AzGameFramework::GameApplication::StartupParameters gameAppParams;
        app.Start({}, gameAppParams);

        if (customRunCode)
        {
            customRunCode();
        }

        //GridMate allocator is created in StartCommon
        const GridMate::GridMateDesc desc;
        GridMate::IGridMate* gridMate = GridMate::GridMateCreate(desc);
        AZ_Assert(gridMate, "Failed to create gridmate!");

        app.RunMainLoop();

        GridMate::GridMateDestroy(gridMate);

        app.Stop();

        return app.GetExitCode();
    }
} // namespace AtomSampleViewer
