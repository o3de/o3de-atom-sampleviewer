/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
#include <Atom/Feature/CoreLights/SpotLightFeatureProcessorInterface.h>
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
    class MultiRenderPipelineExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public AzFramework::WindowNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(MultiRenderPipelineExampleComponent, "{A3654684-DB33-4B2C-B7AB-9B1D6BF3FCF1}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MultiRenderPipelineExampleComponent();
        ~MultiRenderPipelineExampleComponent() final = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzFramework::WindowNotificationBus::Handler
        void OnWindowClosed() override;
        
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

        void AddSpotLight();
        void RemoveSpotLight();

        void EnableSkybox();
        void DisableSkybox();

        void AddIBL();
        void RemoveIBL();
        
        void AddSecondRenderPipeline();
        void RemoveSecondRenderPipeline();
        
        // For draw menus of selecting pipelines
        ImGuiSidebar m_imguiSidebar;
        
        // For scene content
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using SpotLightHandle = AZ::Render::SpotLightFeatureProcessorInterface::LightHandle;
        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;

        // Directional light
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        DirectionalLightHandle m_directionalLightHandle;
        // Spot light
        AZ::Render::SpotLightFeatureProcessorInterface* m_spotLightFeatureProcessor = nullptr;
        SpotLightHandle m_spotLightHandle;
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
        bool m_hasSpotLight = true;
        bool m_enabledSkybox = true;
        bool m_hasIBL = true;
        bool m_enableSecondRenderPipeline = true;
        bool m_useSecondCamera = false;

        // for directional light
        int m_shadowFilteringMethod = aznumeric_cast<int>(AZ::Render::ShadowFilterMethod::EsmPcf);

        // For second render pipeline
        AZStd::shared_ptr<AzFramework::NativeWindow> m_secondWindow;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_secondWindowContext;
        AZ::RPI::RenderPipelinePtr m_secondPipeline;

        // camera for the second render pipeline
        AZ::Entity* m_secondViewCameraEntity = nullptr;

        // Reference to current scene
        AZ::RPI::ScenePtr m_scene;
    };

} // namespace AtomSampleViewer
