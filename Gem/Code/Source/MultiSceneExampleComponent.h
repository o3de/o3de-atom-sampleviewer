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

#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/ReflectionProbe/ReflectionProbeFeatureProcessorInterface.h>
#include <Atom/Feature/ReflectionProbe/ReflectionProbeFeatureProcessor.h>

#include <Utils/Utils.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    class SecondWindowedScene 
        : public AZ::TickBus::Handler
        , public AzFramework::WindowNotificationBus::Handler
    {
        using ModelChangedHandler = AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler;
        using PointLightHandle = AZ::Render::PointLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using ReflectinoProbeHandle = AZ::Render::ReflectionProbeHandle;

        static constexpr uint32_t ShaderBallCount = 12u;

    public:
        SecondWindowedScene(AZStd::string_view sceneName, class MultiSceneExampleComponent* parent);
        ~SecondWindowedScene();

        AzFramework::NativeWindowHandle GetNativeWindowHandle();

        void MoveCamera(bool enabled);

        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // AzFramework::WindowNotificationBus::Handler overrides ...
        void OnWindowClosed() override;

    private:
        AZStd::unique_ptr<AzFramework::NativeWindow> m_nativeWindow;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZStd::unique_ptr<AzFramework::EntityContext> m_entityContext;
        AZStd::string m_sceneName;
        AZStd::shared_ptr<AzFramework::Scene> m_frameworkScene;
        AZ::RPI::ScenePtr m_scene;
        AZ::RPI::RenderPipelinePtr m_pipeline;

        // Entities
        AZ::Entity* m_cameraEntity = nullptr;
        AZ::Entity* m_depthOfFieldEntity = nullptr;

        // Feature Settings
        AZ::Render::DepthOfFieldSettingsInterface* m_depthOfFieldSettings = nullptr;

        // FeatureProcessors
        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyBoxFeatureProcessor = nullptr;
        AZ::Render::PointLightFeatureProcessorInterface* m_pointLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Render::ReflectionProbeFeatureProcessorInterface* m_reflectionProbeFeatureProcessor = nullptr;

        // Meshes
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_shaderBallMeshHandles;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_floorMeshHandle;

        // Model change handlers
        AZStd::vector<ModelChangedHandler> m_shaderBallChangedHandles;

        // Various FeatureProcessor handles
        PointLightHandle m_pointLightHandle;
        DiskLightHandle m_diskLightHandle;
        DirectionalLightHandle m_directionalLightHandle;
        ReflectinoProbeHandle m_reflectionProbeHandle;

        double m_simulateTime = 0.0f;
        float m_deltaTime = 0.0f;
        MultiSceneExampleComponent* m_parent = nullptr;
        const AZ::Vector3 m_cameraOffset{ 0.0f, -4.0f, 2.0f };
        AZ::Vector3 m_dynamicCameraOffset{ 3.73f, 0.0f, 0.0f };
        bool m_moveCamera = false;
        Utils::DefaultIBL m_defaultIbl;
    };
    
    //! A sample component to demonstrate multiple scenes.
    class MultiSceneExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MultiSceneExampleComponent, "{FB0F55AE-6708-47BE-87EB-DD1EB3EF5CD1}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MultiSceneExampleComponent();
        ~MultiSceneExampleComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void OnChildWindowClosed();

    private:        
        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // CommonSampleComponentBase overrides ...
        void OnAllAssetsReadyActivate() override;

        void OpenSecondSceneWindow();

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Component* m_mainCameraControlComponent = nullptr;

        AZStd::unique_ptr<SecondWindowedScene> m_windowedScene;

        // Lights
        Utils::DefaultIBL m_defaultIbl;
    };

} // namespace AtomSampleViewer
