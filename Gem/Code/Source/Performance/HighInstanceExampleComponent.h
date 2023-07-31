/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <EntityLatticeTestComponent.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Color.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/ShadowConstants.h>
#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>

namespace AtomSampleViewer
{
    //! This class is used as base of a set of simple cpu performance stress test for Atom.
    //! This test loads a X*Y*Z lattice of entities with randomized meshes and materials and creates N shadow casting spotlights plus 0-1 Directional lights
   struct HighInstanceTestParameters
   {
       int m_latticeSize[3] = {22, 22, 22};
       float m_latticeSpacing[3] = {5.0f, 5.0f, 5.0f};
       float m_entityScale = 1.0f;

       AZ::Render::ShadowmapSize m_shadowmapSize = AZ::Render::ShadowmapSize::Size256;
       AZ::Render::ShadowFilterMethod m_shadowFilterMethod = AZ::Render::ShadowFilterMethod::None;
       int m_numShadowCastingSpotLights = 0;
       float m_shadowSpotlightInnerAngleDeg = 10.0f;
       float m_shadowSpotlightOuterAngleDeg = 30.0f;
       float m_shadowSpotlightMaxDistance = 200.0f;
       float m_shadowSpotlightIntensity = 500.f; // Value in Candela

       bool m_activateDirectionalLight = false;
       uint16_t m_numDirectionalLightShadowCascades = 4;
       float m_directionalLightIntensity = 5.0f; // Value in Lux

       float m_cameraPosition[3] = {0.0f, 0.0f, 0.0f};
       float m_cameraHeadingDeg = -44.7f;
       float m_cameraPitchDeg = 25.0f;
       float m_iblExposure = 0.0f;
   };
    class HighInstanceTestComponent
        : public EntityLatticeTestComponent
        , public AZ::TickBus::Handler
    {
        using Base = EntityLatticeTestComponent;

    public:
        AZ_RTTI(HighInstanceTestComponent, "{DAA2B63B-7CC0-4696-A44F-49E53C6390B9}", EntityLatticeTestComponent);

        static void Reflect(AZ::ReflectContext* context);

        HighInstanceTestComponent();
        
        //! AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(HighInstanceTestComponent);

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        //! EntityLatticeTestComponent overrides...
        void PrepareCreateLatticeInstances(uint32_t instanceCount) override;
        void CreateLatticeInstance(const AZ::Transform& transform) override;
        void FinalizeLatticeInstances() override;
        void DestroyLatticeInstances() override;
        void DestroyLights();

        void DestroyHandles();

        AZ::Data::AssetId GetRandomModelId() const;
        AZ::Data::AssetId GetRandomMaterialId() const;

        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        void ResetNoClipController();
        void SaveCameraConfiguration();
        void RestoreCameraConfiguration();

        const AZ::Color& GetNextLightColor();
        AZ::Vector3 GetRandomDirection();
        void BuildDiskLightParameters();
        void CreateSpotLights();
        void CreateSpotLight(int index);
        void DrawDiskLightDebugObjects();

        void CreateDirectionalLight();

    protected:
        HighInstanceTestParameters m_testParameters;

    private:
        struct ModelInstanceData
        {
            AZ::Transform m_transform;
            AZ::Data::AssetId m_modelAssetId;
            AZ::Data::AssetId m_materialAssetId;
            AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        };

        ImGuiSidebar m_imguiSidebar;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiAssetBrowser m_modelBrowser;
        
        AZStd::vector<ModelInstanceData> m_modelInstanceData;

        struct Compare
        {
            bool operator()(const AZ::Data::Asset<AZ::RPI::MaterialAsset>& lhs, const AZ::Data::Asset<AZ::RPI::MaterialAsset>& rhs) const
            {
                if (lhs.GetId().m_guid == rhs.GetId().m_guid)
                {
                    return lhs.GetId().m_subId > rhs.GetId().m_subId;
                }
                return lhs.GetId().m_guid > rhs.GetId().m_guid;
            }
        };

        using MaterialAssetSet = AZStd::set<AZ::Data::Asset<AZ::RPI::MaterialAsset>, Compare>;
        MaterialAssetSet m_cachedMaterials;
        uint32_t m_pinnedMaterialCount = 0;
        uint32_t m_preActivateVSyncInterval = 0;
        AZ::SimpleLcgRandom m_random;

        AZStd::vector<AZStd::string> m_expandedModelList; // has models that are more expensive on the gpu
        AZStd::vector<AZStd::string> m_simpleModelList; // Aims to keep the test cpu bottlenecked by using trivial geometry such as a cube

        float m_originalFarClipDistance;
        bool m_updateTransformEnabled = false;
        bool m_useSimpleModels = true;

        // light settings
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;

        class DiskLight
        {
        public:
            DiskLight() = delete;
            explicit DiskLight(
                const AZ::Color& color,
                const AZ::Vector3& relativePosition,
                AZ::Render::ShadowmapSize shadowmapSize)
                : m_color{ color }
                , m_relativePosition{ relativePosition }
                , m_shadowmapSize{ shadowmapSize }
            {}
            ~DiskLight() = default;

            const AZ::Color m_color;
            const AZ::Vector3 m_relativePosition;
            const AZ::Render::ShadowmapSize m_shadowmapSize;
            DiskLightHandle m_handle;
        };

        static constexpr float CutoffIntensity = 0.1f;

        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        DirectionalLightHandle m_directionalLightHandle;
        AZStd::vector<DiskLight> m_diskLights;
        bool m_drawDiskLightDebug = false;
        bool m_diskLightsEnabled = true; // combined with test parameters to determine final state.
        bool m_directionalLightEnabled = true; // combined with test parameters to determine final state.
    };
} // namespace AtomSampleViewer
