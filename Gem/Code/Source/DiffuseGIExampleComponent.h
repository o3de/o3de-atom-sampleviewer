/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Components/CameraBus.h>
#include <CommonSampleComponentBase.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>
#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiMaterialDetails.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <Atom/RPI.Public/WindowContext.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/DiffuseGlobalIllumination/DiffuseProbeGridFeatureProcessorInterface.h>
#include <Atom/Feature/DiffuseGlobalIllumination/DiffuseGlobalIlluminationFeatureProcessorInterface.h>

namespace AtomSampleViewer
{
    //! This sample demonstrates diffuse global illumination using the DiffuseProbeGrid.
    class DiffuseGIExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AtomSampleViewer::DiffuseGIExampleComponent, "{46E0E36D-707D-42D6-B4CB-08A19F576299}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        DiffuseGIExampleComponent() = default;
        ~DiffuseGIExampleComponent() override = default;

        // Component
        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(DiffuseGIExampleComponent);

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // EntityBus::MultiHandler
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        float ComputePointLightAttenuationRadius(AZ::Color color, float intensity);
        void LoadSampleSceneAssets();
        void CreateCornellBoxScene(bool geometryOnly);
        void CreateSponzaScene();
        void UpdateImGui();
        void SetSampleScene(bool geometryOnly = false);
        void UnloadSampleScene(bool geometryOnly);
        void DisableGlobalIbl();
        void EnableDiffuseIbl();

        // camera
        bool m_resetCamera = true;
        float m_originalFarClipDistance = 0.0f;
        AZ::Vector3 m_cameraTranslation = AZ::Vector3(0.0f);
        float m_cameraHeading = 0.0f;

        Utils::DefaultIBL m_defaultIbl;

        // directional light
        float m_directionalLightPitch = -AZ::Constants::QuarterPi;
        float m_directionalLightYaw = 0.f;
        float m_directionalLightIntensity = 20.0f;
        AZ::Color m_directionalLightColor;

        // point light
        AZ::Vector3 m_pointLightPos;
        AZ::Color m_pointLightColor;
        float m_pointLightIntensity = 20.0f;

        // ImGui
        ImGuiSidebar m_imguiSidebar;

        enum SampleScene
        {
            None,
            CornellBox,
            Sponza
        };

        SampleScene m_sampleScene = SampleScene::None;

        // CornellBox scene
        enum class CornellBoxMeshes
        {
            LeftWall,
            RightWall,
            BackWall,
            Ceiling,
            Floor,
            LargeBox,
            SmallBox,
            Count
        };

        enum class CornellBoxColors
        {
            Red,
            Green,
            Blue,
            Yellow,
            White,
            Count
        };

        static const char* CornellBoxColorNames[aznumeric_cast<uint32_t>(CornellBoxColors::Count)];
        static const uint32_t CornellBoxColorNamesCount = sizeof(CornellBoxColorNames) / sizeof(CornellBoxColorNames[0]);

        AZ::Data::Asset<AZ::RPI::MaterialAsset>& GetCornellBoxMaterialAsset(CornellBoxColors color);

        // models
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_planeModelAsset;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_cubeModelAsset;

        // materials
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_redMaterialAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_greenMaterialAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_blueMaterialAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_yellowMaterialAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_whiteMaterialAsset;

        // wall visibility
        bool m_leftWallVisible = true;
        bool m_rightWallVisible = true;
        bool m_backWallVisible = true;
        bool m_floorVisible = true;
        bool m_ceilingVisible = true;

        // wall material colors
        CornellBoxColors m_leftWallColor = CornellBoxColors::Red;
        CornellBoxColors m_rightWallColor = CornellBoxColors::Green;
        CornellBoxColors m_backWallColor = CornellBoxColors::White;
        CornellBoxColors m_floorColor = CornellBoxColors::White;
        CornellBoxColors m_ceilingColor = CornellBoxColors::White;

        // Sponza scene
        enum class SponzaMeshes
        {
            Inside,
            Count
        };

        // models
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_sponzaModelAsset;

        // scene
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandles;
        AZ::Render::PointLightFeatureProcessorInterface::LightHandle m_pointLightHandle;
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;
        AZ::Render::DiffuseProbeGridHandle m_diffuseProbeGrid;

        // diffuse IBL (Sponza only)
        bool m_useDiffuseIbl = true;
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_diffuseImageAsset;
        float m_diffuseIblExposure = 2.0f;

        // shadow settings
        static const AZ::Render::ShadowmapSize s_shadowmapSizes[];
        static const char* s_directionalLightShadowmapSizeLabels[];
        static constexpr int s_shadowmapSizeIndexDefault = 3;
        static constexpr int s_cascadesCountDefault = 4;

        // GI settings
        bool m_enableDiffuseGI = true;
        float m_viewBias = 0.4f;
        float m_normalBias = 0.1f;
        float m_ambientMultiplier = 1.0f;
        AZ::Vector3 m_origin;
        bool m_giShadows = true;
    };
} // namespace AtomSampleViewer
