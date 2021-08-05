/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AreaLightExampleComponent.h>
#include <SampleComponentConfig.h>
#include <Utils/Utils.h>
#include <Automation/ScriptableImGui.h>

#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Shader/ShaderSystem.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <imgui/imgui.h>
#include <Atom/Feature/Material/MaterialAssignment.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void AreaLightExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class <AreaLightExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    AZ::Quaternion AreaLightExampleComponent::Configuration::GetRotationQuaternion()
    {
        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
        rotation.SetFromEulerRadians(AZ::Vector3(m_rotations[0] + AZ::Constants::Pi, m_rotations[1], m_rotations[2]));
        return rotation;
    }

    AZ::Matrix3x3 AreaLightExampleComponent::Configuration::GetRotationMatrix()
    {
        return  AZ::Matrix3x3::CreateFromQuaternion(GetRotationQuaternion());
    }

    AreaLightExampleComponent::AreaLightExampleComponent()
        : m_materialBrowser("@user@/AreaLightExampleComponent/material_browser.xml")
        , m_modelBrowser("@user@/AreaLightExampleComponent/model_browser.xml")
    {
    }

    void AreaLightExampleComponent::Activate()
    {
        // Get Feature processors
        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_meshFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::MeshFeatureProcessorInterface>();
        m_pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiskLightFeatureProcessorInterface>();
        m_capsuleLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::CapsuleLightFeatureProcessorInterface>();
        m_polygonLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PolygonLightFeatureProcessorInterface>();
        m_quadLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::QuadLightFeatureProcessorInterface>();
        m_skyBoxFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::SkyBoxFeatureProcessorInterface>();

        m_auxGeom = AZ::RPI::AuxGeomFeatureProcessorInterface::GetDrawQueueForScene(scene);

        // Create background
        m_skyBoxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::Cubemap);
        m_skyBoxFeatureProcessor->SetCubemap(Utils::GetSolidColorCubemap(0xFF202020));
        m_skyBoxFeatureProcessor->Enable(true);

        // Get material and set up material instances
        AZ::RPI::AssetUtils::TraceLevel traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        static const char* defaultMaterialPath = "materials/presets/macbeth/00_illuminant.azmaterial";
        auto materialAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(defaultMaterialPath, traceLevel);

        InitializeMaterials(materialAsset);

        // Prepare meshes and lights
        auto modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(m_config.m_modelAssetPath.c_str(), traceLevel);
        m_meshHandles.resize(MaxVariants);
        UpdateModels(modelAsset);

        m_photometricValue.ConvertToPhotometricUnit(AZ::Render::PhotometricUnit::Lumen);
        m_lightHandles.resize(MaxVariants);
        UpdateLights();

        // Enable camera
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        // Sidebar
        m_materialBrowser.SetFilter([this](const AZ::Data::AssetInfo& assetInfo)
            {
                return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::MaterialAsset>() &&
                    assetInfo.m_assetId.m_subId == 0; // no materials generated from models.

            });
        m_materialBrowser.Activate();
        m_materialBrowserSettings.m_labels.m_root = "Materials";

        m_modelBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
            {
                return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
            });
        m_modelBrowser.Activate();
        m_modelBrowserSettings.m_labels.m_root = "Models";

        m_imguiSidebar.Activate();
        m_imguiSidebar.SetHideSidebar(true);

        // Connect to busses
        AZ::TickBus::Handler::BusConnect();
    }

    void AreaLightExampleComponent::Deactivate()
    {
        // Force validation off since it's a global flag.
        AZ::RPI::ShaderSystemInterface::Get()->SetGlobalShaderOption(AZ::Name{ "o_area_light_validation" }, AZ::RPI::ShaderOptionValue{ false });

        AZ::TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();
        m_modelBrowser.Deactivate();
        m_materialBrowser.Deactivate();

        ReleaseModels();
        ReleaseLights();
        m_materialInstances.clear();

        m_skyBoxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::None);
        m_skyBoxFeatureProcessor->Enable(false);
    }

    void AreaLightExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawUI();
        DrawAuxGeom();
    }

    float AreaLightExampleComponent::GetPositionPercentage(uint32_t index)
    {
        return aznumeric_cast<float>(index) / aznumeric_cast<float>(m_config.m_count - 1);
    }

    AZ::Vector3 AreaLightExampleComponent::GetModelPosition(uint32_t index)
    {
        static float Spacing = 5.0f;

        // Total width of n models is Spacing * n, so the start x position is half of that.
        float startXPos = aznumeric_cast<float>(m_config.m_count - 1) * 0.5f * -Spacing;
        float xPos = startXPos + aznumeric_cast<float>(index) * Spacing;

        // y position pushes further away from the camera the more models there are to show.
        float yPos = -2.0f + Spacing * m_config.m_count * 0.4f;

        // z is slightly negative so the model's center is slightly below the camera's center
        float zPos = -1.0f;
        
        return AZ::Vector3(xPos, yPos, zPos);
    }

    template<typename T>
    T AreaLightExampleComponent::GetLerpValue(T values[2], uint32_t index, bool doLerp)
    {
        if (doLerp)
        {
            return AZ::Lerp(values[0], values[1], GetPositionPercentage(index));
        }
        return values[0];
    }

    AZ::Vector3 AreaLightExampleComponent::GetLightPosition(uint32_t index)
    {
        AZ::Vector3 position = GetModelPosition(index);
        position.SetZ(position.GetZ() + m_config.m_lightDistance);
        position += AZ::Vector3::CreateFromFloat3(m_config.m_positionOffset);
        return position;
    }

    void AreaLightExampleComponent::InitializeMaterials(AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset)
    {
        m_roughnessPropertyIndex = materialAsset->GetMaterialPropertiesLayout()->FindPropertyIndex(AZ::Name("roughness.factor"));
        m_metallicPropertyIndex = materialAsset->GetMaterialPropertiesLayout()->FindPropertyIndex(AZ::Name("metallic.factor"));
        m_multiScatteringEnabledIndex = materialAsset->GetMaterialPropertiesLayout()->FindPropertyIndex(AZ::Name("specularF0.enableMultiScatterCompensation"));

        m_materialInstances.resize(MaxVariants);
        for (AZ::Data::Instance<AZ::RPI::Material>& material : m_materialInstances)
        {
            material = AZ::RPI::Material::Create(materialAsset);
        }
    }

    void AreaLightExampleComponent::UpdateModels(AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset)
    {
        for (uint32_t i = 0; i < MaxVariants; ++i)
        {
            MeshHandle& meshHandle = m_meshHandles.at(i);

            if (i < m_config.m_count)
            {
                if (!meshHandle.IsValid())
                {
                    meshHandle = m_meshFeatureProcessor->AcquireMesh(MeshHandleDescriptor{ modelAsset }, m_materialInstances.at(i));
                }
                else if (m_modelAsset.GetId() != modelAsset.GetId())
                {
                    // Valid mesh handle, but wrong asset. Release and reacquire.
                    m_meshFeatureProcessor->ReleaseMesh(meshHandle);
                    meshHandle = m_meshFeatureProcessor->AcquireMesh(MeshHandleDescriptor{ modelAsset }, m_materialInstances.at(i));
                }

                AZ::Transform transform = AZ::Transform::CreateIdentity();
                transform.SetTranslation(GetModelPosition(i));

                m_meshFeatureProcessor->SetTransform(meshHandle, transform);
            }
            else if(meshHandle.IsValid())
            {
                m_meshFeatureProcessor->ReleaseMesh(meshHandle);
            }
        }
        m_modelAsset = modelAsset;
    }

    void AreaLightExampleComponent::UpdateMaterials()
    {
        bool allMaterialsCompiled = true;
        for (uint32_t i = 0; i < m_config.m_count; ++i)
        {
            MaterialInstance& materialInstance = m_materialInstances.at(i);
            if (m_roughnessPropertyIndex.IsValid())
            {
                float roughness = GetLerpValue(m_config.m_roughness, i, m_config.GetVaryRoughness());
                materialInstance->SetPropertyValue(m_roughnessPropertyIndex, roughness);
            }
            if (m_metallicPropertyIndex.IsValid())
            {
                float metallic = GetLerpValue(m_config.m_metallic, i, m_config.GetVaryMetallic());
                materialInstance->SetPropertyValue(m_metallicPropertyIndex, metallic);
            }
            if (m_multiScatteringEnabledIndex.IsValid())
            {
                materialInstance->SetPropertyValue(m_multiScatteringEnabledIndex, m_config.m_multiScattering);
            }

            allMaterialsCompiled = allMaterialsCompiled && materialInstance->Compile();
        }
        if (allMaterialsCompiled)
        {
            m_materialsNeedUpdate = false;
        }
    }

    template <typename FeatureProcessorType, typename HandleType>
    void AreaLightExampleComponent::UpdateLightForType(FeatureProcessorType featureProcessor, HandleType& handle, uint32_t index)
    {
        if (index < m_config.m_count)
        {
            if (!handle.IsValid())
            {
                handle = featureProcessor->AcquireLight();
                featureProcessor->SetAttenuationRadius(handle, 100.0f);
            }
        }
        else
        {
            featureProcessor->ReleaseLight(handle);
        }
    }

    void AreaLightExampleComponent::UpdatePointLight(PointLightHandle& handle, uint32_t index, AZ::Vector3 position)
    {
        UpdateLightForType(m_pointLightFeatureProcessor, handle, index);

        if (index < m_config.m_count)
        {
            m_photometricValue.SetEffectiveSolidAngle(AZ::Render::PhotometricValue::OmnidirectionalSteradians);
            m_pointLightFeatureProcessor->SetRgbIntensity(handle, m_photometricValue.GetCombinedRgb<AZ::Render::PhotometricUnit::Candela>());

            float radius = GetLerpValue(m_config.m_radius, index, m_config.GetVaryRadius());
            m_pointLightFeatureProcessor->SetPosition(handle, position);
            m_pointLightFeatureProcessor->SetBulbRadius(handle, radius);
        }
    }

    void AreaLightExampleComponent::UpdateDiskLight(DiskLightHandle& handle, uint32_t index, AZ::Vector3 position)
    {
        UpdateLightForType(m_diskLightFeatureProcessor, handle, index);

        if (index < m_config.m_count)
        {
            m_photometricValue.SetEffectiveSolidAngle(AZ::Render::PhotometricValue::DirectionalEffectiveSteradians);
            m_diskLightFeatureProcessor->SetRgbIntensity(handle, m_photometricValue.GetCombinedRgb<AZ::Render::PhotometricUnit::Candela>());

            m_diskLightFeatureProcessor->SetPosition(handle, position);

            AZ::Matrix3x3 rotationMatrix = m_config.GetRotationMatrix();
            m_diskLightFeatureProcessor->SetDirection(handle, rotationMatrix.GetBasisZ());

            float radius = GetLerpValue(m_config.m_radius, index, m_config.GetVaryRadius());
            m_diskLightFeatureProcessor->SetDiskRadius(handle, radius);
        }
    }

    void AreaLightExampleComponent::UpdateCapsuleLight(CapsuleLightHandle& handle, uint32_t index, AZ::Vector3 position)
    {
        UpdateLightForType(m_capsuleLightFeatureProcessor, handle, index);

        if (index < m_config.m_count)
        {
            m_photometricValue.SetEffectiveSolidAngle(AZ::Render::PhotometricValue::OmnidirectionalSteradians);
            m_capsuleLightFeatureProcessor->SetRgbIntensity(handle, m_photometricValue.GetCombinedRgb<AZ::Render::PhotometricUnit::Candela>());

            AZ::Matrix3x3 rotationMatrix = m_config.GetRotationMatrix();

            AZ::Vector3 startPos = position - rotationMatrix.GetBasisZ() * m_config.m_capsuleHeight * 0.5f;
            AZ::Vector3 endPos = position + rotationMatrix.GetBasisZ() * m_config.m_capsuleHeight * 0.5f;
            m_capsuleLightFeatureProcessor->SetCapsuleLineSegment(handle, startPos, endPos);

            float radius = GetLerpValue(m_config.m_radius, index, m_config.GetVaryRadius());
            m_capsuleLightFeatureProcessor->SetCapsuleRadius(handle, radius);
        }
    }

    void AreaLightExampleComponent::UpdateQuadLight(QuadLightHandle& handle, uint32_t index, AZ::Vector3 position)
    {
        UpdateLightForType(m_quadLightFeatureProcessor, handle, index);

        if (index < m_config.m_count)
        {
            m_photometricValue.SetEffectiveSolidAngle(AZ::Render::PhotometricValue::DirectionalEffectiveSteradians);
            m_photometricValue.SetArea(m_config.m_quadSize[0] * m_config.m_quadSize[1]);
            m_quadLightFeatureProcessor->SetRgbIntensity(handle, m_photometricValue.GetCombinedRgb<AZ::Render::PhotometricUnit::Nit>());

            m_quadLightFeatureProcessor->SetPosition(handle, position);
            m_quadLightFeatureProcessor->SetOrientation(handle, m_config.GetRotationQuaternion());
            m_quadLightFeatureProcessor->SetQuadDimensions(handle, m_config.m_quadSize[0], m_config.m_quadSize[1]);
            m_quadLightFeatureProcessor->SetLightEmitsBothDirections(handle, m_config.m_emitsBothDirections);
            m_quadLightFeatureProcessor->SetUseFastApproximation(handle, m_config.m_fastApproximation);
        }
    }

    void AreaLightExampleComponent::UpdatePolygonLight(PolygonLightHandle& handle, uint32_t index, AZ::Vector3 position)
    {
        UpdateLightForType(m_polygonLightFeatureProcessor, handle, index);

        if (index < m_config.m_count)
        {
            AZStd::vector<AZ::Vector3> points = GetPolygonVertices(m_config.m_polyStarCount, m_config.m_polyMinMaxRadius);

            m_photometricValue.SetEffectiveSolidAngle(AZ::Render::PhotometricValue::DirectionalEffectiveSteradians);
            m_photometricValue.SetArea(CalculatePolygonArea(points));
            m_polygonLightFeatureProcessor->SetRgbIntensity(handle, m_photometricValue.GetCombinedRgb<AZ::Render::PhotometricUnit::Nit>());

            m_polygonLightFeatureProcessor->SetPosition(handle, position);
            m_polygonLightFeatureProcessor->SetLightEmitsBothDirections(handle, m_config.m_emitsBothDirections);

            TransformVertices(points, m_config.GetRotationQuaternion(), position);

            AZ::Vector3 direction = m_config.GetRotationQuaternion().TransformVector(AZ::Vector3::CreateAxisZ());
            m_polygonLightFeatureProcessor->SetPolygonPoints(handle, points.data(), static_cast<uint32_t>(points.size()), direction);
        }
    }

    void AreaLightExampleComponent::UpdateLights()
    {

        m_photometricValue.SetIntensity(m_config.m_intensity);
        m_photometricValue.SetChroma(AZ::Color::CreateFromVector3(AZ::Vector3::CreateFromFloat3(m_config.m_color)));

        for (uint32_t i = 0; i < MaxVariants; ++i)
        {
            LightHandle& lightHandle = m_lightHandles.at(i);
            AZ::Vector3 lightPos = GetLightPosition(i);

            switch (m_config.m_lightType)
            {
            case Point:
                UpdatePointLight(lightHandle.m_point, i, lightPos);
                break;
            case Disk:
                UpdateDiskLight(lightHandle.m_disk, i, lightPos);
                break;
            case Capsule:
                UpdateCapsuleLight(lightHandle.m_capsule, i, lightPos);
                break;
            case Quad:
                UpdateQuadLight(lightHandle.m_quad, i, lightPos);
                break;
            case Polygon:
                UpdatePolygonLight(lightHandle.m_polygon, i, lightPos);
                break;
            }
        }
    }

    void AreaLightExampleComponent::TransformVertices(AZStd::vector<AZ::Vector3>& vertices, const AZ::Quaternion& orientation, const AZ::Vector3& translation)
    {
        for (AZ::Vector3& vertex : vertices)
        {
            vertex = orientation.TransformVector(vertex);
            vertex += translation;
        }
    }

    AZ::Vector3 AreaLightExampleComponent::GetCirclePoint(float n, float count)
    {
        // Calculate angle for this point in the star
        float ratio = 1.0f - (n / count);
        float angle = ratio * AZ::Constants::TwoPi;

        // Get normalized x, y coordinates for point
        float sin = 0.0f;
        float cos = 0.0f;
        AZ::SinCos(angle, sin, cos);

        return AZ::Vector3(sin, cos, 0.0f);
    }

    AZStd::vector<AZ::Vector3> AreaLightExampleComponent::GetPolygonVertices(uint32_t pointCount, float minMaxRadius[2])
    {
        uint32_t vertexCount = pointCount * 2; // For each of the stars points, there's a vertex bewteen the points.
        AZStd::vector<AZ::Vector3> points;
        points.reserve(vertexCount);

        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            // Get a point on the circle and scale it by the min or max radius for every other point.
            AZ::Vector3 point = GetCirclePoint(i, vertexCount) * minMaxRadius[i % 2];
            points.push_back(point);
        }
        return points;
    }

    AZStd::vector<AZ::Vector3> AreaLightExampleComponent::GetPolygonTriangles(uint32_t pointCount, float minMaxRadius[2])
    {
        AZStd::vector<AZ::Vector3> tris;
        tris.reserve(pointCount * 6); // 2 triangles with 3 vertices for each star point.
        uint32_t vertexCount = pointCount * 2; // For each of the stars points, there's a vertex bewteen the points.

        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            uint32_t nextI = (i + 1) % vertexCount;

            AZ::Vector3 p0 = GetCirclePoint(i, vertexCount) * minMaxRadius[i % 2];
            AZ::Vector3 p1 = GetCirclePoint(nextI, vertexCount) * minMaxRadius[nextI % 2];

            tris.push_back(p0);
            tris.push_back(p1);
            tris.push_back(AZ::Vector3::CreateZero());
        }
        return tris;
    }

    void AreaLightExampleComponent::ReleaseModels()
    {
        for (MeshHandle& meshHandle : m_meshHandles)
        {
            m_meshFeatureProcessor->ReleaseMesh(meshHandle);
        }
    }

    void AreaLightExampleComponent::ReleaseLights()
    {
        for (LightHandle& lightHandle : m_lightHandles)
        {
            switch (m_config.m_lightType)
            {
            case Point:
                m_pointLightFeatureProcessor->ReleaseLight(lightHandle.m_point);
                break;
            case Disk:
                m_diskLightFeatureProcessor->ReleaseLight(lightHandle.m_disk);
                break;
            case Capsule:
                m_capsuleLightFeatureProcessor->ReleaseLight(lightHandle.m_capsule);
                break;
            case Quad:
                m_quadLightFeatureProcessor->ReleaseLight(lightHandle.m_quad);
                break;
            case Polygon:
                m_polygonLightFeatureProcessor->ReleaseLight(lightHandle.m_polygon);
                break;
            }
        }
    }

    void AreaLightExampleComponent::DrawUI()
    {
        ScriptableImGui::Begin("AreaLightSample", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

        bool modelsNeedUpdate = false;
        bool lightsNeedUpdate = false;

        ImGui::Text("Area Light Example");
        ImGui::Separator();
        ImGui::Text("Mesh Settings");

        int count = m_config.m_count;
        if (ScriptableImGui::SliderInt("Count", &count, 1, MaxVariants))
        {
            m_config.m_count = aznumeric_cast<uint32_t>(count);
            modelsNeedUpdate = true;
            lightsNeedUpdate = true;
        }

        if (m_roughnessPropertyIndex.IsValid())
        {
            if (m_config.m_count > 1)
            {
                if (ScriptableImGui::Checkbox("Vary Roughness Across Models", &m_config.m_varyRoughness))
                {
                    m_materialsNeedUpdate = true;
                }
            }
            if (m_config.GetVaryRoughness())
            {
                if (ScriptableImGui::SliderFloat2("Min Max Roughness", m_config.m_roughness, 0.0f, 1.0f))
                {
                    m_materialsNeedUpdate = true;
                }
            }
            else if (ScriptableImGui::SliderFloat("Roughness", &m_config.m_roughness[0], 0.0f, 1.0f))
            {
                m_materialsNeedUpdate = true;
            }
        }

        if (m_metallicPropertyIndex.IsValid())
        {
            if (m_config.m_count > 1)
            {
                if (ScriptableImGui::Checkbox("Vary Metallic Across Models", &m_config.m_varyMetallic))
                {
                    m_materialsNeedUpdate = true;
                }
            }
            if (m_config.GetVaryMetallic())
            {
                if (ScriptableImGui::SliderFloat2("Min Max Metallic", m_config.m_metallic, 0.0f, 1.0f))
                {
                    m_materialsNeedUpdate = true;
                }
            }
            else if (ScriptableImGui::SliderFloat("Metallic", &m_config.m_metallic[0], 0.0f, 1.0f))
            {
                m_materialsNeedUpdate = true;
            }
        }

        ImGui::Separator();
        ImGui::Text("Light Settings");

        if (ScriptableImGui::Checkbox("Validate", &m_config.m_validation))
        {
            AZ::RPI::ShaderSystemInterface::Get()->SetGlobalShaderOption(AZ::Name{ "o_area_light_validation" }, AZ::RPI::ShaderOptionValue{ m_config.m_validation });
        }

        AZStd::vector<AZStd::string> lightTypeNames
        {
            "Point",
            "Disk",
            "Capsule",
            "Quad",
            "Polygon",
        };

        if (ScriptableImGui::BeginCombo("LightType", lightTypeNames.at(m_config.m_lightType).c_str()))
        {
            for (uint32_t i = 0; i < lightTypeNames.size(); ++i)
            {
                AZStd::string& name = lightTypeNames.at(i);
                if (ScriptableImGui::Selectable(name.c_str(), m_config.m_lightType == LightType(i)))
                {
                    ReleaseLights();
                    m_config.m_lightType = LightType(i);
                    lightsNeedUpdate = true;
                }
            }
            ScriptableImGui::EndCombo();
        }

        if (ScriptableImGui::SliderFloat("Lumens", &m_config.m_intensity, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            lightsNeedUpdate = true;
        }
        if (ScriptableImGui::ColorEdit3("Color", m_config.m_color, ImGuiColorEditFlags_Float))
        {
            lightsNeedUpdate = true;
        }
        if (ScriptableImGui::SliderFloat3("Position Offset", m_config.m_positionOffset, -3.0f, 3.0f))
        {
            lightsNeedUpdate = true;
        }

        if (m_config.m_lightType == Disk || m_config.m_lightType == Capsule || m_config.m_lightType == Quad || m_config.m_lightType == Polygon)
        {
            if (ScriptableImGui::SliderAngle("X rotation", &m_config.m_rotations[0]))
            {
                lightsNeedUpdate = true;
            }
            if (ScriptableImGui::SliderAngle("Y rotation", &m_config.m_rotations[1]))
            {
                lightsNeedUpdate = true;
            }
            if (m_config.m_lightType == Quad || m_config.m_lightType == Polygon) // Disk and Capsule are circular around z axis, so this only affects Quad and Polygon.
            {
                if (ScriptableImGui::SliderAngle("Z rotation", &m_config.m_rotations[2]))
                {
                    lightsNeedUpdate = true;
                }
            }
        }

        // Radius
        if (m_config.m_lightType == Point || m_config.m_lightType == Disk || m_config.m_lightType == Capsule)
        {
            if (m_config.m_count > 1)
            {
                if (ScriptableImGui::Checkbox("Vary Radius Across Lights", &m_config.m_varyRadius))
                {
                    lightsNeedUpdate = true;
                }
            }
            if (m_config.GetVaryRadius())
            {
                if (ScriptableImGui::SliderFloat2("Min/Max Radius", m_config.m_radius, 0.0f, 1.0f))
                {
                    lightsNeedUpdate = true;
                }
            }
            else if (ScriptableImGui::SliderFloat("Radius", &m_config.m_radius[0], 0.0f, 2.0f))
            {
                lightsNeedUpdate = true;
            }
        }

        // Capsule Height
        if (m_config.m_lightType == Capsule)
        {
            if (ScriptableImGui::SliderFloat("Capsule Height", &m_config.m_capsuleHeight, 0.0f, 10.0f))
            {
                lightsNeedUpdate = true;
            }
        }

        if (m_config.m_lightType == Quad)
        {
            if (ScriptableImGui::SliderFloat2("Quad Dimensions", m_config.m_quadSize, 0.0f, 10.0f))
            {
                lightsNeedUpdate = true;
            }
            if (ScriptableImGui::Checkbox("Use Fast Approximation", &m_config.m_fastApproximation))
            {
                lightsNeedUpdate = true;
            }
        }

        if (m_config.m_lightType == Polygon)
        {
            if (ScriptableImGui::SliderInt("Star Points", &m_config.m_polyStarCount, 2, 32))
            {
                lightsNeedUpdate = true;
            }
            if (ScriptableImGui::SliderFloat2("Star Min-Max Radius", m_config.m_polyMinMaxRadius, 0.0f, 4.0))
            {
                lightsNeedUpdate = true;
            }
        }

        if (m_config.m_lightType == Quad || m_config.m_lightType == Polygon)
        {
            if (ScriptableImGui::Checkbox("Emit Both Directions", &m_config.m_emitsBothDirections))
            {
                lightsNeedUpdate = true;
            }
        }

        if (ScriptableImGui::Checkbox("Multiscattering", &m_config.m_multiScattering))
        {
            m_materialsNeedUpdate = true;
        }

        ScriptableImGui::End();

        if (m_imguiSidebar.Begin())
        {
            if (m_modelBrowser.Tick(m_modelBrowserSettings))
            {
                AZ::Data::AssetId selectedModelAssetId = m_modelBrowser.GetSelectedAssetId();
                if (selectedModelAssetId.IsValid())
                {
                    AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset;
                    modelAsset.Create(selectedModelAssetId);
                    UpdateModels(modelAsset);
                    modelsNeedUpdate = false;
                }
            }

            ImGui::Spacing();

            if (m_materialBrowser.Tick(m_materialBrowserSettings))
            {
                AZ::Data::AssetId selectedMaterialAssetId = m_materialBrowser.GetSelectedAssetId();
                if (selectedMaterialAssetId.IsValid())
                {
                    AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::MaterialAsset>(
                        selectedMaterialAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
                    materialAsset.BlockUntilLoadComplete();

                    if (materialAsset.IsReady())
                    {
                        InitializeMaterials(materialAsset);
                        ReleaseModels();
                        modelsNeedUpdate = true;
                    }
                }
            }

            m_imguiSidebar.End();
        }

        if (modelsNeedUpdate)
        {
            UpdateModels(m_modelAsset);
        }
        if (m_materialsNeedUpdate || modelsNeedUpdate)
        {
            UpdateMaterials();
        }
        if (lightsNeedUpdate)
        {
            UpdateLights();
        }

    }

    void AreaLightExampleComponent::DrawAuxGeom()
    {
        // Lights need to add support for rendering their emissive surfaces to the regular forward pass which will replace the below.

        // Draw AuxGeom for the lights themselves

        AZ::Matrix3x3 rotationMatrix = m_config.GetRotationMatrix();

        for (uint32_t i = 0; i < m_config.m_count; ++i)
        {
            float radius = GetLerpValue(m_config.m_radius, i, m_config.GetVaryRadius());
            AZ::Vector3 lightPos = GetLightPosition(i);

            float area = 0.0f;
            switch (m_config.m_lightType)
            {
            case Point:
                area = 4.0f * AZ::Constants::Pi * radius * radius;
                break;
            case Disk:
                area = AZ::Constants::Pi * radius * radius;
                break;
            case Capsule:
            {
                float cylinderArea = 2.0f * AZ::Constants::Pi * m_config.m_capsuleHeight * radius;
                float capArea = 4.0f * AZ::Constants::Pi * radius * radius;
                area = cylinderArea + capArea;
                break;
            }
            case Quad:
                area = m_config.m_quadSize[0] * m_config.m_quadSize[1];
                break;
            case Polygon:
                area = CalculatePolygonArea(GetPolygonVertices(m_config.m_polyStarCount, m_config.m_polyMinMaxRadius));
                break;
            }


            float luxIntensity = m_config.m_intensity / area;
            float nitsIntensity = luxIntensity / AZ::Constants::Pi;

            // The aux geom pass happens after display mapper pass, so do basic gamma correction so the surface of the light's brightness is closer to everything else.
            nitsIntensity = pow(nitsIntensity, 1.0f / 2.2f);

            AZ::Color nitsColor = AZ::Color(nitsIntensity * m_config.m_color[0], nitsIntensity * m_config.m_color[1], nitsIntensity * m_config.m_color[2], 1.0f);

            AZ::RPI::AuxGeomDraw::DrawStyle drawStyle = AZ::RPI::AuxGeomDraw::DrawStyle::Solid;

            switch (m_config.m_lightType)
            {
            case Point:
                m_auxGeom->DrawSphere(lightPos, radius, nitsColor, drawStyle);
                break;
            case Disk:
                m_auxGeom->DrawDisk(lightPos, rotationMatrix.GetBasisZ(), radius, nitsColor, drawStyle);
                break;
            case Capsule:
            {
                m_auxGeom->DrawCylinder(lightPos, rotationMatrix.GetBasisZ(), radius, m_config.m_capsuleHeight, nitsColor, drawStyle);

                // Draw cylinder caps as spheres
                AZ::Vector3 startPos = lightPos - rotationMatrix.GetBasisZ() * m_config.m_capsuleHeight * 0.5f;
                AZ::Vector3 endPos = lightPos + rotationMatrix.GetBasisZ() * m_config.m_capsuleHeight * 0.5f;
                m_auxGeom->DrawSphere(startPos, radius, nitsColor, drawStyle);
                m_auxGeom->DrawSphere(endPos, radius, nitsColor, drawStyle);
                break;
            }
            case Quad:
            {
                AZ::Transform transform = AZ::Transform::CreateIdentity();
                transform.SetRotation(AZ::ConvertEulerRadiansToQuaternion(AZ::Vector3(m_config.m_rotations[0], -m_config.m_rotations[1], -m_config.m_rotations[2])));
                transform.SetTranslation(lightPos);
                transform *= AZ::Transform::CreateFromQuaternion(AZ::ConvertEulerRadiansToQuaternion(AZ::Vector3(AZ::Constants::Pi * 0.5f, 0.0f, 0.0f)));
                m_auxGeom->DrawQuad(m_config.m_quadSize[0], m_config.m_quadSize[1], AZ::Matrix3x4::CreateFromTransform(transform), nitsColor, drawStyle);
                break;
            }
            case Polygon:
            {
                // Sadly DrawTriangles() only supports 8 bit color, so nitsColor must be capped at 1.0f.
                nitsIntensity = AZ::GetMin(1.0f, nitsIntensity);
                nitsColor = AZ::Color(nitsIntensity * m_config.m_color[0], nitsIntensity * m_config.m_color[1], nitsIntensity * m_config.m_color[2], 1.0f);

                AZStd::vector<AZ::Vector3> tris = GetPolygonTriangles(m_config.m_polyStarCount, m_config.m_polyMinMaxRadius);
                TransformVertices(tris, m_config.GetRotationQuaternion(), lightPos);

                AZ::RPI::AuxGeomDraw::AuxGeomDynamicDrawArguments args;
                args.m_colorCount = 1;
                args.m_colors = &nitsColor;
                args.m_vertCount = static_cast<uint32_t>(tris.size());
                args.m_verts = tris.data();
                m_auxGeom->DrawTriangles(args);
                break;
            }
            }
        }
   }

    float AreaLightExampleComponent::CalculatePolygonArea(const AZStd::vector<AZ::Vector3>& vertices)
    {
        // See https://en.wikipedia.org/wiki/Shoelace_formula
        float twiceArea = 0.0f;
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            size_t j = (i + 1) % vertices.size();
            twiceArea += vertices.at(i).GetX() * vertices.at(j).GetY();
            twiceArea -= vertices.at(i).GetY() * vertices.at(j).GetX();
        }
        return AZ::GetAbs(twiceArea * 0.5f);
    }

}
