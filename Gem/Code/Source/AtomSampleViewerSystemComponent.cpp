/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomSampleViewerSystemComponent.h>
#include <MaterialFunctors/StacksShaderCollectionFunctor.h>
#include <MaterialFunctors/StacksShaderInputFunctor.h>
#include <Automation/ImageComparisonConfig.h>

#include <EntityLatticeTestComponent.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/IO/SystemFile.h>

#include <AzFramework/Input/Buses/Requests/InputSystemCursorRequestBus.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

#include <Atom/Bootstrap/DefaultWindowBus.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Shader/Metrics/ShaderMetricsSystem.h>

#include <ISystem.h>
#include <IConsole.h>

#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiSaveFilePath.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    void AtomSampleViewerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AtomSampleViewerSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }

        PerfMetrics::Reflect(context);
        ImGuiAssetBrowser::Reflect(context);
        ImGuiSidebar::Reflect(context);
        ImGuiSaveFilePath::Reflect(context);
        StacksShaderCollectionFunctor::Reflect(context);
        StacksShaderInputFunctor::Reflect(context);

        ImageComparisonConfig::Reflect(context);
    }

    void AtomSampleViewerSystemComponent::PerfMetrics::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PerfMetrics>()
                ->Version(1)
                ->Field("TestDurationSeconds", &PerfMetrics::m_timingTargetSeconds)
                ->Field("AverageFramesPerSecond", &PerfMetrics::m_averageDeltaSeconds)
                ->Field("SecondsToRender", &PerfMetrics::m_timeToFirstRenderSeconds)
                ;
        }

        // Abstract base component is used by multiple components and needs to be reflected in a single location.
        EntityLatticeTestComponent::Reflect(context);
    }


    void AtomSampleViewerSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("PrototypeLmbrCentralService", 0xe35e6de0));
    }

    void AtomSampleViewerSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ::RHI::Factory::GetComponentService());
        required.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
        required.push_back(AZ_CRC("RPISystem", 0xf2add773));
        required.push_back(AZ_CRC("BootstrapSystemComponent", 0xb8f32711));
    }

    AtomSampleViewerSystemComponent::AtomSampleViewerSystemComponent()
        : m_timestamp(HighResTimer::now())
    {
        m_atomSampleViewerEntity = aznew AZ::Entity();
        m_atomSampleViewerEntity->Init();
    }

    AtomSampleViewerSystemComponent::~AtomSampleViewerSystemComponent()
    {
    }

    void AtomSampleViewerSystemComponent::Activate()
    {
        AZ::EntitySystemBus::Handler::BusConnect();

        AZ::ApplicationTypeQuery appType;
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationBus::Events::QueryApplicationType, appType);
        if (appType.IsValid() && !appType.IsEditor())
        {
            // AtomSampleViewer SampleComponentManager creates and manages its own scene and render pipelines. 
            // We disable the creation of default scene in BootStrapSystemComponent
            AZ::Render::Bootstrap::DefaultWindowBus::Broadcast(&AZ::Render::Bootstrap::DefaultWindowBus::Events::SetCreateDefaultScene, false);
        }

        AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequestBus::Events::LoadCatalog, "@assets@/assetcatalog.xml");

        m_atomSampleViewerEntity->Activate();

        AZ::TickBus::Handler::BusConnect();
        CrySystemEventBus::Handler::BusConnect();
    }

    void AtomSampleViewerSystemComponent::Deactivate()
    {
        CrySystemEventBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();

        if (m_atomSampleViewerEntity != nullptr)
        {
            m_atomSampleViewerEntity->Deactivate();
        }

        AZ::EntitySystemBus::Handler::BusDisconnect();
    }

    void AtomSampleViewerSystemComponent::OnEntityDestroyed(const AZ::EntityId& entityId)
    {
        if (m_atomSampleViewerEntity && m_atomSampleViewerEntity->GetId() == entityId)
        {
            m_atomSampleViewerEntity = nullptr;
        }
    }

    void AtomSampleViewerSystemComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        TickTimeoutShutdown(deltaTime);

#if defined(AZ_DEBUG_BUILD)
        TrackPerfMetrics(deltaTime);
#endif
    }

    void AtomSampleViewerSystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams&)
    {
        system.GetIConsole()->GetCVar("sys_asserts")->Set(2);

        // Currently CSystem::Init hides and constrains the mouse cursor.
        // For AtomSampleViewer we want it visible so that we can use the ImGui menus
        AzFramework::InputSystemCursorRequestBus::Event(AzFramework::InputDeviceMouse::Id,
                                                        &AzFramework::InputSystemCursorRequests::SetSystemCursorState,
                                                        AzFramework::SystemCursorState::UnconstrainedAndVisible);

        ReadTimeoutShutdown();
    }

    void AtomSampleViewerSystemComponent::ReadTimeoutShutdown()
    {
        const AzFramework::CommandLine* commandLine = nullptr;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(commandLine, &AzFramework::ApplicationRequests::Bus::Events::GetApplicationCommandLine);
        if (commandLine)
        {
            if (commandLine->HasSwitch("timeout"))
            {
                const AZStd::string& timeoutValue = commandLine->GetSwitchValue("timeout", 0);
                const float timeoutInSeconds = static_cast<float>(atoi(timeoutValue.c_str()));
                AZ_Printf("AtomSampleViewer", "starting up with timeout shutdown of %f seconds", timeoutInSeconds);
                m_secondsBeforeShutdown = timeoutInSeconds;
            }
        }
    }

    void AtomSampleViewerSystemComponent::TickTimeoutShutdown(float deltaTimeInSeconds)
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

    void AtomSampleViewerSystemComponent::TrackPerfMetrics(const float deltaTime)
    {
        m_frameCount++;

        if (m_frameCount != 1) // don't accumulate delta on the first frame
        {
            m_accumulatedDeltaSeconds += deltaTime;
        }

        if (!m_testsLogged)
        {
            AZStd::chrono::duration<float> elapsedTime = HighResTimer::now() - m_timestamp;

            if (m_frameCount == 1)
            {
                m_perfMetrics.m_timeToFirstRenderSeconds = elapsedTime.count();
            }

            if (elapsedTime.count() >= m_perfMetrics.m_timingTargetSeconds)
            {
                if (m_frameCount > 1)
                {
                    m_perfMetrics.m_averageDeltaSeconds = m_accumulatedDeltaSeconds / static_cast<float>(m_frameCount - 1);
                }
                LogPerfMetrics();
                m_testsLogged = true;
            }
        }
    }

    void AtomSampleViewerSystemComponent::LogPerfMetrics() const
    {
        AZ::Utils::SaveObjectToFile("metrics.xml", AZ::DataStream::ST_XML, &m_perfMetrics);
    }
} // namespace AtomSampleViewer
