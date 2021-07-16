/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    /*
    * This component creates a simple scene to Depth of Field.
    */
    class DepthOfFieldExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(DepthOfFieldExampleComponent, "{F2CE0FCC-F3CE-4900-929D-74DCF554DF97}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        DepthOfFieldExampleComponent() = default;
        ~DepthOfFieldExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        // AZ::EntityBus::MultiHandler...
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        void UseArcBallCameraController();
        void RemoveController();

        void CreateMeshes();
        void CreateLight();
        void CreateDepthOfFieldEntity();

        void DrawSidebar();

        static constexpr uint32_t BunnyNumber = 32;
        static constexpr float DistanceBetweenUnits = 1.0f;
        static constexpr float ModelScaleRatio = DistanceBetweenUnits * 1.3f;
        static constexpr float ViewNear = 0.2f;
        static constexpr float ViewFar = ViewNear + DistanceBetweenUnits * BunnyNumber;
        static constexpr float FocusDefault = ViewNear + (ViewFar - ViewNear) * 0.35f;
        static constexpr float ViewFovDegrees = 20.0f;

        // owning entity
        AZ::Entity* m_depthOfFieldEntity = nullptr;
        
        // asset
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_bunnyModelAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;

        // Mesh handles
        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;
        AZStd::array<MeshHandle, BunnyNumber> m_meshHandles;

        // Light handle
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        DirectionalLightHandle m_directionalLightHandle;

        // GUI
        ImGuiSidebar m_imguiSidebar;

        // initialize flag
        bool m_isInitCamera = false;
        bool m_isInitParameters = false;

        // save default camera parameter on base viewer.
        // When deactivate this component, return the parameters to camera.
        float m_saveDefaultNearOnAtomSampleViewer = 0.1f;
        float m_saveDefaultFarOnAtomSampleViewer = 10.0f;
        float m_saveDefaultFovDegreesOnAtomSampleViewer = 20.0f;

        // feature processors
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;

        AZ::Render::DepthOfFieldSettingsInterface* m_depthOfFieldSettings = nullptr;
        AZ::Render::PostProcessSettingsInterface* m_postProcessSettings = nullptr;
    };
} // namespace AtomSampleViewer
