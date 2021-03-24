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
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/ShadowConstants.h>
#include <Atom/Feature/CoreLights/SpotLightFeatureProcessorInterface.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Random.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    /*
     * This component creates a scene of Bistro with shadowing.
     */
    class ShadowedBistroExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ShadowedBistroExampleComponent, "AAA320C7-1CF7-4CBA-9279-D29BB04B9CA9", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ShadowedBistroExampleComponent() = default;
        ~ShadowedBistroExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        using DirectionalLightHandle = AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle;
        using SpotLightHandle = AZ::Render::SpotLightFeatureProcessorInterface::LightHandle;

        class SpotLight
        {
        public:
            SpotLight() = delete;
            explicit SpotLight(
                const AZ::Color& color,
                const AZ::Vector3& relativePosition,
                AZ::Render::ShadowmapSize shadowmapSize)
                : m_color{ color }
                , m_relativePosition{ relativePosition }
                , m_shadowmapSize{ shadowmapSize }
            {}
            ~SpotLight() = default;

            const AZ::Color m_color;
            const AZ::Vector3 m_relativePosition;
            const AZ::Render::ShadowmapSize m_shadowmapSize;
            SpotLightHandle m_handle;
        };

        static constexpr int SpotLightCountMax = 50;
        static constexpr int SpotLightCountDefault = 10;
        static constexpr float CutoffIntensity = 0.1f;

        static const AZ::Color DirectionalLightColor;
        
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);

        void SaveCameraConfiguration();
        void RestoreCameraConfiguration();
        void SetInitialCameraTransform();

        void SetupScene();
        void BuildSpotLightParameters();
        void UpdateSpotLightCount(uint16_t count);
        const AZ::Color& GetRandomColor();
        AZ::Vector3 GetRandomPosition();
        AZ::Render::ShadowmapSize GetRandomShadowmapSize();

        void DrawSidebar();
        void UpdateSpotLightShadowmapSize();
        void UpdateSpotLightPositions();
        void UpdateSpotLightPosition(int index);
        void SetupDebugFlags();

        float m_originalFarClipDistance = 0.f;

        // lights
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::SpotLightFeatureProcessorInterface* m_spotLightFeatureProcessor = nullptr;
        DirectionalLightHandle m_directionalLightHandle;
        AZStd::vector<SpotLight> m_spotLights;

        // scene setup
        AZ::SimpleLcgRandom m_random;
        AZ::Aabb m_worldAabb = AZ::Aabb::CreateNull();

        // model
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler
        {
            [&](AZ::Data::Instance<AZ::RPI::Model> model) { OnModelReady(model); }
        };
        bool m_bistroExteriorAssetLoaded = false;

        // GUI
        ImGuiSidebar m_imguiSidebar;
        float m_directionalLightPitch = -AZ::Constants::QuarterPi;
        float m_directionalLightYaw = 0.f;
        float m_directionalLightIntensity = 5.f;
        float m_spotLightIntensity = 500.f;
        int m_spotLightCount = 0;
        float m_spotLightsBasePosition[3] = {0.f, 0.f, 0.f};
        float m_spotLightsPositionScatteringRatio = 0.0f;
        bool m_cameraTransformInitialized = false;

        // Shadowmap
        static const AZ::Render::ShadowmapSize s_shadowmapSizes[];
        static const char* s_directionalLightShadowmapSizeLabels[];
        static constexpr int s_shadowmapSizeIndexDefault = 3;
        static constexpr int s_cascadesCountDefault = 4;
        static constexpr float s_ratioLogarithmUniformDefault = 0.8f;
        int m_directionalLightShadowmapSizeIndex = 0;
        int m_cascadeCount = 0;
        float m_ratioLogarithmUniform = 0.f;
        AZ::Render::ShadowmapSize m_spotLightShadowmapSize = AZ::Render::ShadowmapSize::None;
        bool m_spotLightShadowEnabled = true;

        // Edge-softening of directional light shadows
        static const AZ::Render::ShadowFilterMethod s_shadowFilterMethods[];
        static const char* s_shadowFilterMethodLabels[];
        int m_shadowFilterMethodIndexDirectional = 0; // filter method is None.
        int m_shadowFilterMethodIndexSpot = 0; // filter method is None
        float m_boundaryWidthDirectional = 0.03f; // 3cm
        float m_boundaryWidthSpot = 0.25f; // 0.25 degrees
        int m_predictionSampleCountDirectional = 4;
        int m_predictionSampleCountSpot = 4;
        int m_filteringSampleCountDirectional = 16;
        int m_filteringSampleCountSpot = 16;

        bool m_isCascadeCorrectionEnabled = false;
        bool m_isDebugColoringEnabled = false;
        bool m_isDebugBoundingBoxEnabled = false;
    };
} // namespace AtomSampleViewer
