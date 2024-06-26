/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>

#include <Atom/RPI.Public/Base.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/ShadowConstants.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>

#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    //! A sample component which render the same scene with different render pipelines in different windows
    //! It has a imgui menu to switch on/off the second render pipeline as well as turn on/off different graphics features
    //! There is also an option to have the second render pipeline to use the second camera. 
    class MultiGPURPIExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(MultiGPURPIExampleComponent, "{F7DD0D21-A0EF-4B66-98FA-2DB6B19A8C35}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MultiGPURPIExampleComponent();
        ~MultiGPURPIExampleComponent() final = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        
    private:
        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        // DefaultWindowNotificationBus::Handler overrides...
        void DefaultWindowCreated() override;

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;

        AZ::RPI::RenderPipelinePtr m_pipeline;
        AZ::RPI::RenderPipelinePtr m_copyPipeline;
        AZ::RPI::RenderPipelinePtr m_originalPipeline;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;

        AZ::Render::ImGuiActiveContextScope m_imguiScope;
        ImGuiSidebar m_imguiSidebar;

        bool m_useCopyPipeline = false;
        bool m_currentlyUsingCopyPipline = false;
    };

} // namespace AtomSampleViewer
