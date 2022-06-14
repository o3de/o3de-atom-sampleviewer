/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

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
    class MultiRenderToTextureExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MultiRenderToTextureExampleComponent, "{C9602E16-8D75-481D-B430-6C0622AD1E6D}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MultiRenderToTextureExampleComponent();
        ~MultiRenderToTextureExampleComponent() final = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        // Add render content to the scene
        void SetupScene();
        // Clean up render content in the scene
        void CleanUpScene();

        // enable/disable different features for the second render pipeline 
        void EnableDepthOfField();
        void DisableDepthOfField();

        void AddDirectionalLight();
        void RemoveDirectionalLight();

        void AddDiskLight();
        void RemoveDiskLight();

        void EnableSkybox();
        void DisableSkybox();

        void AddIBL();
        void RemoveIBL();
        
        void AddRenderToTextureCamera();
        void DestroyRenderToTextureCamera(uint32_t index);

        // For draw menus of selecting pipelines
        ImGuiSidebar m_imguiSidebar;
        
        // For scene content
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;
        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;

        // Directional light
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        DirectionalLightHandle m_directionalLightHandle;
        // Disk light
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        DiskLightHandle m_diskLightHandle;
        // Meshes
        MeshHandle m_floorMeshHandle;

        static const uint32_t BunnyCount = 5;
        MeshHandle m_bunnyMeshHandles[BunnyCount];
        // Skybox
        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyboxFeatureProcessor = nullptr;
        // IBL
        Utils::DefaultIBL m_ibl;
        // Post Process
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Entity* m_depthOfFieldEntity = nullptr;

        // flags of features enabled
        bool m_enabledDepthOfField = true;
        bool m_hasDirectionalLight = true;
        bool m_hasDiskLight = true;
        bool m_enabledSkybox = true;
        bool m_hasIBL = true;
        bool m_enableSecondRenderPipeline = true;
        bool m_useSecondCamera = false;

        // for directional light
        int m_shadowFilteringMethod = aznumeric_cast<int>(AZ::Render::ShadowFilterMethod::EsmPcf);

        // For second render pipeline
        AZStd::shared_ptr<AzFramework::NativeWindow> m_secondWindow;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_secondWindowContext;

        struct RenderToTextureContext
        {
            AZStd::vector<AZStd::string> m_passHierarchy;
            AZ::RPI::RenderPipelinePtr m_pipeline;
            AZ::RPI::ViewPtr m_view;
            AZ::RPI::ViewPtr m_targetView;

            uint32_t m_imageCount = 0;
        };
        AZStd::vector<RenderToTextureContext> m_renderToTextureContexts;
    };

} // namespace AtomSampleViewer
