/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Entity/EntityContextBus.h>

#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    // This component renders a model with pbr material using checkerboard render pipeline.
    class CheckerboardExampleComponent final
        : public AZ::Component
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(CheckerboardExampleComponent, "{0E5B3D5F-5BD2-41BF-BB1E-425AF976DFC9}");

        static void Reflect(AZ::ReflectContext* context);

        CheckerboardExampleComponent();
        ~CheckerboardExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        // AZ::Component overrides...
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;

        // DefaultWindowNotificationBus::Handler overrides...
        void DefaultWindowCreated() override;
        
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void ActivateCheckerboardPipeline();
        void DeactivateCheckerboardPipeline();

        // draw debug menu
        void DrawSidebar();
                        
        // shader ball model
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;

        // Not owned by this sample, we look this up based on the config from the
        // SampleComponentManager
        AZ::EntityId m_cameraEntityId;
        AzFramework::EntityContextId m_entityContextId;

        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        Utils::DefaultIBL m_defaultIbl;

        // for checkerboard render pipeline
        AZ::RPI::RenderPipelinePtr m_cbPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;

        // debug menu
        ImGuiSidebar m_imguiSidebar;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;
    };
} // namespace AtomSampleViewer
