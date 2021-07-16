/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    /*
    * This component creates a simple scene to demonstrate the exposure feature.
    */
    class ExposureExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ExposureExampleComponent, "{90D874F5-6DE8-4CF3-A6DC-754E903544FF}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ExposureExampleComponent() = default;
        ~ExposureExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        using PointLightHandle = AZ::Render::PointLightFeatureProcessorInterface::LightHandle;
        PointLightHandle m_pointLight;

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        // AZ::EntityBus::MultiHandler...
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        void SetInitialCameraTransform();
        void EnableCameraController();
        void DisableCameraController();

        void CreateExposureEntity();
        void SetupScene();
        void SetupLights();
        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);

        void DrawSidebar();

        // owning entity
        AZ::Entity* m_exposureEntity = nullptr;
        bool m_cameraTransformInitialized = false;
        
        // GUI
        ImGuiSidebar m_imguiSidebar;

        // initialize flag
        bool m_isInitCamera = false;
        bool m_isInitParameters = false;

        // model
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model) { OnModelReady(model); }
        };
        bool m_sponzaAssetLoaded = false;

        // feature processors
        AZ::Render::PointLightFeatureProcessorInterface* m_pointLightFeatureProcessor = nullptr;
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;

        AZ::Render::ExposureControlSettingsInterface* m_exposureControlSettings = nullptr;
    };
} // namespace AtomSampleViewer
