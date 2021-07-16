/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once
#include <CommonSampleComponentBase.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    //! Demostrate the effect of Parallax Mapping and Pixel Depth Offset
    class ParallaxMappingExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:

        AZ_COMPONENT(ParallaxMappingExampleComponent, "{C2530F5A-8626-49B7-8913-DDEA25C7E7CD}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ParallaxMappingExampleComponent();
        ~ParallaxMappingExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        static constexpr float ConeAngleInnerRatio = 0.9f;
        static constexpr float CutoffIntensity = 0.1f;

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle LoadMesh(
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset,
            AZ::Data::Instance<AZ::RPI::Material> material,
            AZ::Transform transform);
        void CreateDirectionalLight();
        void CreateDiskLight();
        void DrawSidebar();

        void SaveCameraConfiguration();
        void RestoreCameraConfiguration();
        void SetCameraConfiguration();
        void ConfigureCameraToLookDown();

        // Mesh Handles
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_planeHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_boxHandle;

        // Light
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;
        AZ::Render::DiskLightFeatureProcessorInterface::LightHandle m_diskLightHandle;

        float m_lightRotationAngle = 0.f; // in radian
        bool m_lightAutoRotate = true;
        int m_lightType = 0; // 0: diectionalLight, 1: diskLight

        //Assets
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_planeAsset;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_boxAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_defaultMaterialAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_parallaxMaterialAsset;
        AZ::Data::Instance<AZ::RPI::Material> m_defaultMaterial;
        AZ::Data::Instance<AZ::RPI::Material> m_parallaxMaterial;

        AZ::RPI::MaterialPropertyIndex m_parallaxEnableIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxFactorIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxOffsetIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxShowClippingIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxAlgorithmIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxQualityIndex;
        AZ::RPI::MaterialPropertyIndex m_parallaxUvIndex;
        AZ::RPI::MaterialPropertyIndex m_pdoEnableIndex;

        // Other UVs that when parallaxUv is changed, they must follow the change.
        AZ::RPI::MaterialPropertyIndex m_ambientOcclusionUvIndex;
        AZ::RPI::MaterialPropertyIndex m_baseColorUvIndex;
        AZ::RPI::MaterialPropertyIndex m_normalUvIndex;
        AZ::RPI::MaterialPropertyIndex m_roughnessUvIndex;

        AZ::RPI::MaterialPropertyIndex m_centerUVIndex;
        AZ::RPI::MaterialPropertyIndex m_tileUIndex;
        AZ::RPI::MaterialPropertyIndex m_tileVIndex;
        AZ::RPI::MaterialPropertyIndex m_scaleUVIndex;
        AZ::RPI::MaterialPropertyIndex m_offsetUIndex;
        AZ::RPI::MaterialPropertyIndex m_offsetVIndex;
        AZ::RPI::MaterialPropertyIndex m_rotationUVIndex;

        // parallax setting
        bool m_parallaxEnable = true;
        bool m_pdoEnable = true;
        float m_parallaxFactor = 0.05f;
        float m_parallaxOffset = 0.0f;
        bool m_parallaxShowClipping = false;
        // see StandardPbr.materialtype for the full enum list.
        int m_parallaxAlgorithm = 2; // POM
        int m_parallaxQuality = 2;   // High
        int m_parallaxUv = 0; // 0 = UV0, 1 = UV1

        // plane setting
        AZ::Transform m_planeTransform;
        float m_planeRotationAngle = 0.f; // in radian
        float m_planeCenterU = 0.f;
        float m_planeCenterV = 0.f;
        float m_planeTileU = 1.f;
        float m_planeTileV = 1.f;
        float m_planeScaleUV = 1.f;
        float m_planeOffsetU = 0.f;
        float m_planeOffsetV = 0.f;
        float m_planeRotateUV = 0.f; // in degrees

        // original camera configuration
        float m_originalFarClipDistance = 0.f;
        float m_originalCameraFovRadians = 0.f;

        // ImGui
        ImGuiSidebar m_imguiSidebar;
    };
} // namespace AtomSampleViewer
