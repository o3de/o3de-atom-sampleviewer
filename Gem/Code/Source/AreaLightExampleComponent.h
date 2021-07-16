/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <AzCore/Component/TickBus.h>

#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>

#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/CapsuleLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DiskLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/PolygonLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/QuadLightFeatureProcessorInterface.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>

#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    // This component renders a model with pbr material using checkerboard render pipeline.
    class AreaLightExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AreaLightExampleComponent, "{1CFEDA71-9459-44CE-A88B-F0CAE9192819}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        AreaLightExampleComponent();
        ~AreaLightExampleComponent() override = default;

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:

        static const uint32_t MaxVariants = 10;

        using MeshHandleDescriptor = AZ::Render::MeshHandleDescriptor;
        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;
        using PointLightHandle = AZ::Render::PointLightFeatureProcessorInterface::LightHandle;
        using DiskLightHandle = AZ::Render::DiskLightFeatureProcessorInterface::LightHandle;
        using CapsuleLightHandle = AZ::Render::CapsuleLightFeatureProcessorInterface::LightHandle;
        using QuadLightHandle = AZ::Render::QuadLightFeatureProcessorInterface::LightHandle;
        using PolygonLightHandle = AZ::Render::PolygonLightFeatureProcessorInterface::LightHandle;
        using MaterialInstance = AZ::Data::Instance<AZ::RPI::Material>;

        enum LightType
        {
            Point,
            Disk,
            Capsule,
            Quad,
            Polygon,
        };

        union LightHandle
        {
            LightHandle() { m_point.Reset(); };
            PointLightHandle m_point;
            DiskLightHandle m_disk;
            CapsuleLightHandle m_capsule;
            QuadLightHandle m_quad;
            PolygonLightHandle m_polygon;
        };

        // Various data the user can alter in ImGui stored in ImGui friendly types.
        struct Configuration
        {
            LightType m_lightType = Point;
            AZStd::string m_modelAssetPath = "objects/test/area_light_test_sphere.azmodel";
            uint32_t m_count = 1;

            float m_intensity = 30.0f;
            float m_color[3] = { 1.0f, 1.0f, 1.0f };

            float m_lightDistance = 3.0f;
            float m_positionOffset[3] = { 0.0f, 0.0f, 0.0f };

            float m_rotations[3] = { 0.0f, 0.0f, 0.0f };
            
            bool m_varyRadius = false;
            float m_radius[2] = { 0.1f, 1.0f };

            bool m_varyRoughness = false;
            float m_roughness[2] = { 1.0f, 0.0f };

            bool m_varyMetallic = false;
            float m_metallic[2] = { 0.0f, 1.0f };

            float m_capsuleHeight = 2.0f;

            float m_quadSize[2] = { 1.0f, 1.0f };

            int32_t m_polyStarCount = 5;
            float m_polyMinMaxRadius[2] = { 0.25f, 0.5f };

            bool m_emitsBothDirections = false;
            bool m_validation = false;
            bool m_fastApproximation = false;
            bool m_multiScattering = false;

            //! Creates a quaterion based on m_rotations
            AZ::Quaternion GetRotationQuaternion();

            //! Creates a Matrix3x3 based on m_rotations
            AZ::Matrix3x3 GetRotationMatrix();

            bool GetVaryRadius() { return m_varyRadius && m_count > 1; }
            bool GetVaryRoughness() { return m_varyRoughness && m_count > 1; }
            bool GetVaryMetallic() { return m_varyMetallic && m_count > 1; }
        };

        AZ_DISABLE_COPY_MOVE(AreaLightExampleComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        //! Creates material instances for the given material asset.
        void InitializeMaterials(AZ::Data::Asset< AZ::RPI::MaterialAsset> materialAsset);

        //! Updates the number of model and asset shown.
        void UpdateModels(AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset);

        //! Updates material instances based on user config (roughness, metalness etc)
        void UpdateMaterials();

        //! Updates the lights based on user config (light type, intensity etc)
        void UpdateLights();

        //! Handles generic properties that apply to all lights.
        template <typename FeatureProcessorType, typename HandleType>
        void UpdateLightForType(FeatureProcessorType featureProcessor, HandleType& handle, uint32_t index);

        // Specific light configuration...
        void UpdatePointLight(PointLightHandle& handle, uint32_t index, AZ::Vector3 position);
        void UpdateDiskLight(DiskLightHandle& handle, uint32_t index, AZ::Vector3 position);
        void UpdateCapsuleLight(CapsuleLightHandle& handle, uint32_t index, AZ::Vector3 position);
        void UpdateQuadLight(QuadLightHandle& handle, uint32_t index, AZ::Vector3 position);
        void UpdatePolygonLight(PolygonLightHandle& handle, uint32_t index, AZ::Vector3 position);

        //! Gets a 0.0 -> 1.0 value based on index and m_config's m_count.
        float GetPositionPercentage(uint32_t index);

        //! Gets the model's position based on the index.
        AZ::Vector3 GetModelPosition(uint32_t index);

        //! Gets the light's position based on the index.
        AZ::Vector3 GetLightPosition(uint32_t index);

        //! Convience function to either lerp two values or just return the first depending on bool.
        template<typename T>
        T GetLerpValue(T values[2], uint32_t index, bool doLerp);

        //! Releases all the models.
        void ReleaseModels();

        //! Releases all the lights.
        void ReleaseLights();

        //! Draws all the ImGui controls.
        void DrawUI();

        //! Draws the lights themselves using AuxGeom
        void DrawAuxGeom();

        // Transforms the points based on the rotation and translation settings.
        static void TransformVertices(AZStd::vector<AZ::Vector3>& vertices, const AZ::Quaternion& orientation, const AZ::Vector3& translation);

        // Utility function to get the nth point out of 'count' points on a unit circle on the z plane. Runs counter-clockwise starting from (1.0, 0.0, 0.0).
        static AZ::Vector3 GetCirclePoint(float n, float count);

        // Calculates the area of a polygon star.
        static float CalculatePolygonArea(const AZStd::vector<AZ::Vector3>& vertices);

        // Gets the edge vertices for a polygon star on the z plane.
        static AZStd::vector<AZ::Vector3> GetPolygonVertices(uint32_t pointCount, float minMaxRadius[2]);

        // Gets triangles for a polygon star on the z plane.
        static AZStd::vector<AZ::Vector3> GetPolygonTriangles(uint32_t pointCount, float minMaxRadius[2]);

        Configuration m_config;

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZ::RPI::AuxGeomDrawPtr m_auxGeom;

        // Feature processors
        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        AZ::Render::PointLightFeatureProcessorInterface* m_pointLightFeatureProcessor = nullptr;
        AZ::Render::DiskLightFeatureProcessorInterface* m_diskLightFeatureProcessor = nullptr;
        AZ::Render::CapsuleLightFeatureProcessorInterface* m_capsuleLightFeatureProcessor = nullptr;
        AZ::Render::QuadLightFeatureProcessorInterface* m_quadLightFeatureProcessor = nullptr;
        AZ::Render::PolygonLightFeatureProcessorInterface* m_polygonLightFeatureProcessor = nullptr;
        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyBoxFeatureProcessor = nullptr;

        AZ::RPI::MaterialPropertyIndex m_roughnessPropertyIndex;
        AZ::RPI::MaterialPropertyIndex m_metallicPropertyIndex;
        AZ::RPI::MaterialPropertyIndex m_multiScatteringEnabledIndex;

        AZStd::vector<MaterialInstance> m_materialInstances;
        AZStd::vector<MeshHandle> m_meshHandles;
        AZStd::vector<LightHandle> m_lightHandles;

        AZ::Render::PhotometricValue m_photometricValue;

        ImGuiSidebar m_imguiSidebar;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiAssetBrowser m_modelBrowser;
        ImGuiAssetBrowser::WidgetSettings m_materialBrowserSettings;
        ImGuiAssetBrowser::WidgetSettings m_modelBrowserSettings;

        bool m_materialsNeedUpdate = true;
    };
}
