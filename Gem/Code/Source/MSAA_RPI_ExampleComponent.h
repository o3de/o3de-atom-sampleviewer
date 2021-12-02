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
#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>
#include <ExampleComponentBus.h>

namespace AtomSampleViewer
{
    //!
    //! This component creates a simple scene that tests the MSAA pipeline. It can test both MSAA enabled and disabled with the same scene
    //!
    class MSAA_RPI_ExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AZ::TickBus::Handler
        , public ExampleComponentRequestBus::Handler
    {
    public:

        AZ_COMPONENT(MSAA_RPI_ExampleComponent, "{2BDCA64E-7E5F-49EE-ACF5-65C79C40840D}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:

        // DefaultWindowNotificationBus::Handler
        void DefaultWindowCreated() override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // ExampleComponentRequestBus::Handler
        void ResetCamera() override;

        // helper functions
        void ChangeRenderPipeline();
        void ActivateIbl();
        void ActivateModel();
        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);
        AZ::Data::Asset<AZ::RPI::MaterialAsset> GetMaterialAsset();
        AZ::Data::Asset<AZ::RPI::ModelAsset> GetModelAsset();
        void DrawSidebar();
        bool DrawSidebarModeChooser(bool refresh);
        bool DrawSideBarModelChooser(bool refresh);

        // sample mesh
        int m_modelType = 0;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model)
            {
                OnModelReady(model);
            }
        };

        // original render pipeline when the sample was started
        AZ::RPI::RenderPipelinePtr m_originalPipeline;

        // sample render pipeline, can be MSAA 2x/4x/8x or non-MSAA
        AZ::RPI::RenderPipelinePtr m_samplePipeline;

        // number of MSAA samples in the pipeline, controlled by the user
        int m_numSamples = 1;

        // flag indicating if the current pipeline is a non-MSAA pipeline
        bool m_isNonMsaaPipeline = false;

        // other state
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        Utils::DefaultIBL m_defaultIbl;
        ImGuiSidebar m_imguiSidebar;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;
    };
} // namespace AtomSampleViewer
