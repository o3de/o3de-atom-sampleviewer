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

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Random.h>
#include <Utils/ImGuiSidebar.h>

#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/SpotLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/CapsuleLightFeatureProcessorInterface.h>
#include <Atom/Feature/Decals/DecalFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/QuadLightFeatureProcessorInterface.h>

namespace AZ
{
    namespace RPI
    {
        using AuxGeomDrawPtr = AZStd::shared_ptr<class AuxGeomDraw>;
    }

    namespace Data
    {
        class AssetData;
    }
}

namespace AtomSampleViewer
{
    class LightCullingExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(LightCullingExampleComponent, "56B28789-4104-49B1-9C67-1DFC440DD800", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        LightCullingExampleComponent();
        ~LightCullingExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;


    private:

        enum class LightType
        {
            Point,
            Spot,
            Disk,
            Capsule,
            Quad,
            Decal,
            Count
        };

        struct LightSettings
        {
            bool m_enableDebugDraws = false;
            float m_intensity = 40.0f;
            float m_attenuationRadius = 3.0f;
            bool m_enableAutomaticFalloff = true;
            int m_numActive = 0;
        };

        using PointLightHandle = AZ::Render::PointLightFeatureProcessorInterface::LightHandle;
        using SpotLightHandle = AZ::Render::SpotLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;
        using CapsuleLightHandle = AZ::Render::CapsuleLightFeatureProcessorInterface::LightHandle;
        using QuadLightHandle = AZ::Render::QuadLightFeatureProcessorInterface::LightHandle;

        template<typename LightHandle>
        struct Light
        {
            AZ::Vector3 m_position;
            AZ::Vector3 m_direction;
            AZ::Color m_color;
            LightHandle m_lightHandle;
        };

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        void DrawDebuggingHelpers();
        void DrawPointLightDebugSpheres(AZ::RPI::AuxGeomDrawPtr auxGeom);
        void DrawSpotLightDebugCones(AZ::RPI::AuxGeomDrawPtr auxGeom);
        void DrawDiskLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom);
        void DrawCapsuleLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom);
        void DrawDecalDebugBoxes(AZ::RPI::AuxGeomDrawPtr auxGeom);

        void SaveCameraConfiguration();
        void RestoreCameraConfiguration();
        void SetupScene();

        void CreateOpaqueModels();
        void DestroyOpaqueModels();

        void CreateTransparentModels();
        void DestroyTransparentModels();

        void SetupCamera();

        void CreatePointLights();
        void CreatePointLight(int index);

        void CreateSpotLights();
        void CreateSpotLight(int index);

        void CreateDiskLights();
        void CreateDiskLight(int index);

        void CreateCapsuleLights();
        void CreateCapsuleLight(int index);

        void CreateQuadLights();
        void CreateQuadLight(int index);

        template<typename FP, typename LA>
        void DestroyLights(FP* fp, LA& lightArray);

        void CreateDecals();
        void CreateDecal(int index);
        void DestroyDecals();

        AZ::Color GetRandomColor();

        void DrawSidebar();
        void DrawSidebarTimingSection();
        void DrawSidebarTimingSectionCPU();

        void UpdateLights();

        void CreateLightsAndDecals();

        void DestroyLightsAndDecals();

        void DrawSidebarPointLightsSection(LightSettings* lightSettings);
        void DrawSidebarSpotLightsSection(LightSettings* lightSettings);
        void DrawSidebarDiskLightsSection(LightSettings* lightSettings);
        void DrawSidebarCapsuleLightSection(LightSettings* lightSettings);
        void DrawSidebarDecalSection(LightSettings* lightSettings);
        void DrawSidebarQuadLightsSections(LightSettings* lightSettings);

        void DrawSidebarHeatmapOpacity();

        using DecalHandle = AZ::Render::DecalFeatureProcessorInterface::DecalHandle;

        struct Decal
        {
            AZ::Vector3 m_position;
            AZ::Quaternion m_quaternion;
            float m_opacity;
            float m_angleAttenuation;
            DecalHandle m_decalHandle;
        };

        void CalculateSmoothedFPS(float deltaTime);
        AZ::Vector3 GetRandomPositionInsideWorldModel();
        AZ::Vector3 GetRandomDirection();
        float GetRandomNumber(float low, float high);
        void InitLightArrays();
        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);
        void GetFeatureProcessors();

        static float AutoCalculateAttenuationRadius(const AZ::Color& color, float intensity);
        void MoveCameraToStartPosition();
        void UpdateHeatmapOpacity();
        void DisableHeatmap();
        void DrawQuadLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom);
        void LoadDecalMaterial();
        AZStd::array<LightSettings, (size_t)LightType::Count> m_settings;

        AZStd::vector<Light<PointLightHandle>> m_pointLights;
        AZStd::vector<Light<SpotLightHandle>> m_spotLights;
        AZStd::vector<Light<DiskLightHandle>> m_diskLights;
        AZStd::vector<Light<CapsuleLightHandle>> m_capsuleLights;
        AZStd::vector<Light<QuadLightHandle>> m_quadLights;
        AZStd::vector<Decal> m_decals;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_transparentMeshHandles;

        float m_originalFarClipDistance = 0.f;

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model) { OnModelReady(model); }
        };
        bool m_worldModelAssetLoaded = false;

        AZ::Aabb m_worldModelAABB;
        AZ::SimpleLcgRandom m_random;

        ImGuiSidebar m_imguiSidebar;

        float m_smoothedFPS = 0.0f;
        float m_bulbRadius = 3.0f;

        float m_spotInnerConeDegrees = 22.0f;
        float m_spotOuterConeDegrees = 90.0f;

        float m_capsuleRadius = 0.1f;
        float m_capsuleLength = 3.0f;

        float m_diskRadius = 3.0f;
        bool m_isDiskDoubleSided = false;
        bool m_isQuadLightDoubleSided = false;
        bool m_quadLightsUseFastApproximation = false;

        AZStd::array<float, 3> m_decalSize = {
            { 5, 5, 5 }
        };
        float m_decalAngleAttenuation = 0.0f;
        float m_decalOpacity = 1.0f;

        bool m_refreshLights = false;
        float m_heatmapOpacity = 0.0f;
        AZStd::array<float, 2> m_quadLightSize = { 4, 2 };
        AZ::Data::Asset<AZ::Data::AssetData> m_decalMaterial;

        AZ::Render::PointLightFeatureProcessorInterface* m_pointLightFeatureProcessor = nullptr;
        AZ::Render::SpotLightFeatureProcessorInterface* m_spotLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        AZ::Render::CapsuleLightFeatureProcessorInterface* m_capsuleLightFeatureProcessor = nullptr;
        AZ::Render::QuadLightFeatureProcessorInterface* m_quadLightFeatureProcessor = nullptr;
        AZ::Render::DecalFeatureProcessorInterface* m_decalFeatureProcessor = nullptr;
    };

    template<typename FP, typename LA>
    inline void AtomSampleViewer::LightCullingExampleComponent::DestroyLights(FP* fp, LA& lightArray)
    {
        for (auto& elem : lightArray)
        {
            fp->ReleaseLight(elem.m_lightHandle);
        }
    }
} // namespace AtomSampleViewer
