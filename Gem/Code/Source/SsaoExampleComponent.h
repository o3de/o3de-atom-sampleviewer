/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/RPI.Public/Pass/Specific/SelectorPass.h>
#include <Passes/RayTracingAmbientOcclusionPass.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>
#include <ExampleComponentBus.h>

namespace AtomSampleViewer
{
    //!
    //! This component creates a simple scene that tests raw SSAO output from depth
    //!
    class SsaoExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
    public:

        AZ_COMPONENT(SsaoExampleComponent, "{5D1A0FF0-02CB-4462-8A87-27498F67BEC1}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // AZ::EntityBus::MultiHandler...
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        void ActivatePostProcessSettings();
        void DectivatePostProcessSettings();

        void ActivateSsaoPipeline();
        void DeactivateSsaoPipeline();

        void CreateSsaoPipeline();
        void DestroySsaoPipeline();

        void ActivateModel();
        void DeactivateModel();

        void ActivateCamera();
        void DeactivateCamera();
        void MoveCameraToStartPosition();

        void DefaultWindowCreated() override;

        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);
        void DrawImGUI();
        void DrawSidebar();

        void SwitchAOType();

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model)
            {
                OnModelReady(model);
            }
        };
        
        AZ::RPI::RenderPipelinePtr m_ssaoPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        ImGuiSidebar m_imguiSidebar;

        float m_originalFarClipDistance = 0.f;
        bool m_worldModelAssetLoaded = false;

        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Render::SsaoSettingsInterface* m_ssaoSettings = nullptr;
        AZ::Entity* m_ssaoEntity = nullptr;

        AZ::Render::ImGuiActiveContextScope m_imguiScope;

        enum AmbientOcclusionType
        {
            SSAO = 0,
            RTAO
        };

        // Ambient Occlusion type. 0: SSAO; 1: RTAO
        int m_aoType = AmbientOcclusionType::SSAO;

        // Ray tracing ambient occlusion
        bool m_rayTracingEnabled = false;
        AZ::RPI::Ptr<AZ::Render::RayTracingAmbientOcclusionPass> m_RTAOPass;

        // To swtich between outputs
        AZ::RPI::Ptr<AZ::RPI::SelectorPass> m_selector;
    };
} // namespace AtomSampleViewer
