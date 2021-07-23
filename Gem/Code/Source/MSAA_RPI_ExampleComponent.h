/*
 * Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
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

        void ActivateMSAAPipeline();
        void DeactivateMSAAPipeline();

        void ActivateIbl();
        void DeactivateIbl();

        void CreateMSAAPipeline(int numSamples);
        void DestroyMSAAPipeline();

        void ActivateModel();
        void DeactivateModel();
        
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // ExampleComponentRequestBus::Handler
        void ResetCamera() override;

        void DefaultWindowCreated() override;

        void EnableArcBallCameraController();
        void DisableArcBallCameraController();

        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);
        AZ::Data::Asset<AZ::RPI::MaterialAsset> GetMaterialAsset();
        AZ::Data::Asset<AZ::RPI::ModelAsset> GetModelAsset();
        void DrawSidebar();

        bool DrawSidebarModeChooser(bool refresh);
        bool DrawSideBarModelChooser(bool refresh);

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model)
            {
                OnModelReady(model);
            }
        };
        
        AZ::RPI::RenderPipelinePtr m_msaaPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        Utils::DefaultIBL m_defaultIbl;
        ImGuiSidebar m_imguiSidebar;
        int m_numSamples = 1;
        int m_modelType = 0;

        AZ::Render::ImGuiActiveContextScope m_imguiScope;
    };
} // namespace AtomSampleViewer
