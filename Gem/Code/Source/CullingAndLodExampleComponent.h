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
#include <Atom/Feature/CoreLights/ShadowConstants.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessor.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Random.h>
#include <AzFramework/Entity/EntityContext.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    class CullingAndLodExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(CullingAndLodExampleComponent, "CA7AB736-5C80-425E-8DF3-E1C22971D79C", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        CullingAndLodExampleComponent() = default;
        ~CullingAndLodExampleComponent() override = default;

        // need to remove the copy constructor because Handles cannot be copied, only moved
        CullingAndLodExampleComponent(CullingAndLodExampleComponent&) = delete;

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;

        class DiskLight
        {
        public:
            DiskLight() = delete;
            explicit DiskLight(const AZ::Color& color, const AZ::Vector3& position,
                const AZ::Vector3& direction, AZ::Render::ShadowmapSize shadowmapSize)
                : m_color(color)
                , m_position(position)
                , m_direction(direction)
                , m_shadowmapSize(shadowmapSize)
            {}
            ~DiskLight() = default;

            const AZ::Color m_color;
            const AZ::Vector3 m_position;
            const AZ::Vector3 m_direction;
            const AZ::Render::ShadowmapSize m_shadowmapSize;
            DiskLightHandle m_handle;
        };

        static constexpr int DiskLightCountMax = 100;
        static constexpr int DiskLightCountDefault = 10;
        static constexpr float CutoffIntensity = 0.5f;

        static const AZ::Color DirectionalLightColor;
        
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void ResetNoClipController();

        void SaveCameraConfiguration();
        void RestoreCameraConfiguration();

        void SetupScene();
        void ClearMeshes();
        void SpawnModelsIn2DGrid(uint32_t numAlongXAxis, uint32_t numAlongYAxis);
        void SetupLights();
        void UpdateDiskLightCount(uint16_t count);

        void DrawSidebar();
        void UpdateDiskLightShadowmapSize();

        float m_originalFarClipDistance = 0.f;

        // lights
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        DirectionalLightHandle m_directionalLightHandle;
        AZStd::vector<DiskLight> m_diskLights;

        // models
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandles;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler> m_modelChangedHandlers;

        // GUI
        ImGuiSidebar m_imguiSidebar;
        float m_directionalLightPitch = -1.22;
        float m_directionalLightYaw = 0.7f;
        float m_directionalLightIntensity = 4.f;
        float m_diskLightIntensity = 2000.f;
        int m_diskLightCount = 0;

        // Shadowmap
        static const AZ::Render::ShadowmapSize s_shadowmapSizes[];
        static const char* s_directionalLightShadowmapSizeLabels[];
        static constexpr int s_shadowmapSizeIndexDefault = 3;
        static constexpr int s_cascadesCountDefault = 4;
        static constexpr float s_ratioLogarithmUniformDefault = 0.8f;
        int m_directionalLightShadowmapSizeIndex = 0;
        int m_cascadeCount = 0;
        float m_ratioLogarithmUniform = 0.f;
        AZ::Render::ShadowmapSize m_diskLightShadowmapSize = AZ::Render::ShadowmapSize::None;
        bool m_diskLightShadowEnabled = true;

        // Edge-softening of directional light shadows
        static const AZ::Render::ShadowFilterMethod s_shadowFilterMethods[];
        static const char* s_shadowFilterMethodLabels[];
        int m_shadowFilterMethodIndex = 0; // filter method is None.
        float m_boundaryWidth = 0.03f; // 3cm
        int m_predictionSampleCount = 4;
        int m_filteringSampleCount = 16;

        bool m_isCascadeCorrectionEnabled = false;
        bool m_isDebugColoringEnabled = false;
    };
} // namespace AtomSampleViewer
