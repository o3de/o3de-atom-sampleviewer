/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <CullingAndLodExampleComponent.h>
#include <SampleComponentConfig.h>

#include <Automation/ScriptableImGui.h>
#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerBus.h>
#include <imgui/imgui.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Components/TransformComponent.h>

namespace AtomSampleViewer
{
    const AZ::Color CullingAndLodExampleComponent::DirectionalLightColor = AZ::Color::CreateOne();
    const AZ::Render::ShadowmapSize CullingAndLodExampleComponent::s_shadowmapSizes[] =
    {
        AZ::Render::ShadowmapSize::Size256,
        AZ::Render::ShadowmapSize::Size512,
        AZ::Render::ShadowmapSize::Size1024,
        AZ::Render::ShadowmapSize::Size2048
    };
    const char* CullingAndLodExampleComponent::s_directionalLightShadowmapSizeLabels[] =
    {
        "256",
        "512",
        "1024",
        "2048"
    };
    const AZ::Render::ShadowFilterMethod CullingAndLodExampleComponent::s_shadowFilterMethods[] =
    {
        AZ::Render::ShadowFilterMethod::None,
        AZ::Render::ShadowFilterMethod::Pcf,
        AZ::Render::ShadowFilterMethod::Esm,
        AZ::Render::ShadowFilterMethod::EsmPcf
    };
    const char* CullingAndLodExampleComponent::s_shadowFilterMethodLabels[] =
    {
        "None",
        "PCF",
        "ESM",
        "ESM+PCF"
    };

    void CullingAndLodExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CullingAndLodExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void CullingAndLodExampleComponent::Activate()
    {
        using namespace AZ;

        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<Debug::NoClipControllerComponent>());

        SaveCameraConfiguration();
        ResetNoClipController();        

        SetupScene();

        m_imguiSidebar.Activate();

        TickBus::Handler::BusConnect();
    }

    void CullingAndLodExampleComponent::Deactivate()
    {
        using namespace AZ;

        TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        // disable camera control
        RestoreCameraConfiguration();
        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Disable);

        ClearMeshes();

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        UpdateDiskLightCount(0);
    }

    void CullingAndLodExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(timePoint);

        using namespace AZ;

        DrawSidebar();

        // Pass camera data to the DirectionalLightFeatureProcessor
        if (m_directionalLightHandle.IsValid())
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(config, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(m_directionalLightHandle, config);

            Transform transform = Transform::CreateIdentity();
            TransformBus::EventResult(transform, GetCameraEntityId(), &TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform( m_directionalLightHandle, transform);
        }
    }

    void CullingAndLodExampleComponent::ResetNoClipController()
    {
        using namespace AZ;
        using namespace AZ::Debug;
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 2000.0f);
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetPosition, Vector3(0.0f, -1.2f, 3.4f));
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetHeading, 0.0f);
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetPitch, 0.0f);
    }

    void CullingAndLodExampleComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void CullingAndLodExampleComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
    }

    void CullingAndLodExampleComponent::SetupScene()
    {
        using namespace AZ;

        SpawnModelsIn2DGrid(5, 5);
        SetupLights();
    }

    void CullingAndLodExampleComponent::ClearMeshes()
    {
        using namespace AZ;
        Render::MeshFeatureProcessorInterface* meshFP = GetMeshFeatureProcessor();
        for (auto& meshHandle : m_meshHandles)
        {
            meshFP->ReleaseMesh(meshHandle);
        }
        m_meshHandles.clear();
    }

    void CullingAndLodExampleComponent::SpawnModelsIn2DGrid(uint32_t numAlongXAxis, uint32_t numAlongYAxis)
    {
        using namespace AZ;
        Render::MeshFeatureProcessorInterface* meshFP = GetMeshFeatureProcessor();

        ClearMeshes();

        const char objectModelFilename[] = "Objects/sphere_5lods.azmodel";
        const char planeModelFilename[] = "Objects/plane.azmodel";
        Data::Asset<RPI::ModelAsset> objectModelAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(
            objectModelFilename, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::ModelAsset> planeModelAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ModelAsset>(
            planeModelFilename, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::MaterialAsset>(
            DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        Data::Instance<RPI::Material> material = RPI::Material::FindOrCreate(materialAsset);

        float spacing = 2.0f*objectModelAsset->GetAabb().GetExtents().GetMaxElement();

        for (uint32_t x = 0; x < numAlongXAxis; ++x)
        {
            for (uint32_t y = 0; y < numAlongYAxis; ++y)
            {
                auto meshHandle = meshFP->AcquireMesh(Render::MeshHandleDescriptor{ objectModelAsset }, material);
                Transform modelToWorld = Transform::CreateTranslation(Vector3(x * spacing, y * spacing, 2.0f));
                meshFP->SetTransform(meshHandle, modelToWorld);
                m_meshHandles.push_back(AZStd::move(meshHandle));
            }
        }

        auto planeMeshHandle = meshFP->AcquireMesh(Render::MeshHandleDescriptor{ planeModelAsset }, material);
        Vector3 planeNonUniformScale(numAlongXAxis * spacing, numAlongYAxis * spacing, 1.0f);
        Transform planeModelToWorld = Transform::CreateTranslation(Vector3(0.5f * numAlongXAxis * spacing, 0.5f * numAlongYAxis * spacing, 0.0f));
        meshFP->SetTransform(planeMeshHandle, planeModelToWorld, planeNonUniformScale);
        m_meshHandles.push_back(AZStd::move(planeMeshHandle));
    }

    void CullingAndLodExampleComponent::SetupLights()
    {
        using namespace AZ;

        m_directionalLightShadowmapSizeIndex = s_shadowmapSizeIndexDefault;
        m_diskLightShadowmapSize = Render::ShadowmapSize::None; // random
        m_cascadeCount = s_cascadesCountDefault;
        m_ratioLogarithmUniform = s_ratioLogarithmUniformDefault;
        m_diskLightCount = 0;

        RPI::Scene* scene = RPI::Scene::GetSceneForEntityContextId(GetEntityContextId());
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();

        // directional light
        {
            Render::DirectionalLightFeatureProcessorInterface* dirLightFP = m_directionalLightFeatureProcessor;
            const DirectionalLightHandle handle = dirLightFP->AcquireLight();

            const auto lightTransform = Transform::CreateLookAt(
                Vector3(100, 100, 100),
                Vector3::CreateZero());
            dirLightFP->SetDirection(handle, lightTransform.GetBasis(1));

            dirLightFP->SetRgbIntensity(handle, Render::PhotometricColor<Render::PhotometricUnit::Lux>(m_directionalLightIntensity * DirectionalLightColor));
            dirLightFP->SetCascadeCount(handle, s_cascadesCountDefault);
            dirLightFP->SetShadowmapSize(handle, s_shadowmapSizes[s_shadowmapSizeIndexDefault]);
            dirLightFP->SetDebugFlags(handle,
                m_isDebugColoringEnabled ?
                AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawAll :
                AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawNone);
            dirLightFP->SetViewFrustumCorrectionEnabled(handle, m_isCascadeCorrectionEnabled);
            dirLightFP->SetShadowFilterMethod(handle, s_shadowFilterMethods[m_shadowFilterMethodIndex]);
            dirLightFP->SetShadowBoundaryWidth(handle, m_boundaryWidth);
            dirLightFP->SetPredictionSampleCount(handle, m_predictionSampleCount);
            dirLightFP->SetFilteringSampleCount(handle, m_filteringSampleCount);
            dirLightFP->SetGroundHeight(handle, 0.f);

            m_directionalLightHandle = handle;
        }

        // disk lights
        {
            m_diskLights.clear();
            m_diskLights.reserve(DiskLightCountMax);
            const float disklightSpacing = 10.0f;
            const int diskLightsPerRow = 10;
            const Color colors[5] = {Colors::Red, Colors::Green, Colors::Blue, Colors::Orange, Colors::Pink};
            for (int index = 0; index < DiskLightCountMax; ++index)
            {
                float xPos = (index % diskLightsPerRow) * disklightSpacing;
                float yPos = (index / diskLightsPerRow) * disklightSpacing;
                m_diskLights.emplace_back(
                    colors[index % 5],
                    Vector3(xPos, yPos, 10.0f),
                    Vector3(1.0f, 1.0f, -1.0f).GetNormalized(),
                    Render::ShadowmapSize::Size256);
            }
            UpdateDiskLightCount(20);
        }
    }

    void CullingAndLodExampleComponent::UpdateDiskLightCount(uint16_t count)
    {
        using namespace AZ;

        for (int index = count; index < m_diskLightCount; ++index)
        {
            DiskLightHandle& handle = m_diskLights[index].m_handle;
            m_diskLightFeatureProcessor->ReleaseLight(handle);
        }

        const int previousDiskLightCount = m_diskLightCount;

        for (int index = previousDiskLightCount; index < count; ++index)
        {
            Render::DiskLightFeatureProcessorInterface* const diskLightFP = m_diskLightFeatureProcessor;
            const DiskLightHandle handle = diskLightFP->AcquireLight();
            const DiskLight &diskLight = m_diskLights[index];

            diskLightFP->SetPosition(handle, diskLight.m_position);
            diskLightFP->SetDirection(handle, diskLight.m_direction);
            diskLightFP->SetRgbIntensity(handle, Render::PhotometricColor<Render::PhotometricUnit::Candela>(diskLight.m_color * m_diskLightIntensity));
            const float radius = sqrtf(m_diskLightIntensity / CutoffIntensity);
            diskLightFP->SetAttenuationRadius(handle, radius);
            diskLightFP->SetShadowsEnabled(handle, m_diskLightShadowEnabled);
            if (m_diskLightShadowEnabled)
            {
                diskLightFP->SetShadowmapMaxResolution(handle, m_diskLights[index].m_shadowmapSize);
            }
            diskLightFP->SetConeAngles(handle, DegToRad(45.f), DegToRad(55.f));

            m_diskLights[index].m_handle = handle;
        }

        m_diskLightCount = count;
    }

    void CullingAndLodExampleComponent::DrawSidebar()
    {
        using namespace AZ;
        using namespace AZ::Render;

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Spacing();

        if (ImGui::Button("Spawn 20x20 Grid of objects"))
        {
            SpawnModelsIn2DGrid(20, 20);
        }
        if (ImGui::Button("Spawn 50x50 Grid of objects"))
        {
            SpawnModelsIn2DGrid(50, 50);
        }
        if (ImGui::Button("Spawn 100x100 Grid of objects"))
        {
            SpawnModelsIn2DGrid(100, 100);
        }

        ImGui::Separator();        

        ImGui::Text("Directional Light");
        ImGui::Indent();
        {
            //SliderAngle() displays angles in degrees, but returns an angle in radians
            ImGui::SliderAngle("Pitch", &m_directionalLightPitch, -90.0f, 0.f);
            ImGui::SliderAngle("Yaw", &m_directionalLightYaw, 0.f, 360.f);
            const auto lightTrans = Transform::CreateRotationZ(m_directionalLightYaw) * Transform::CreateRotationX(m_directionalLightPitch);
            m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightTrans.GetBasis(1));

            if (ImGui::SliderFloat("Intensity##directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                m_directionalLightFeatureProcessor->SetRgbIntensity(
                    m_directionalLightHandle,
                    Render::PhotometricColor<Render::PhotometricUnit::Lux>(DirectionalLightColor * m_directionalLightIntensity));
            }

            ImGui::Separator();

            ImGui::Text("Shadowmap Size");
            if (ImGui::Combo( "Size", &m_directionalLightShadowmapSizeIndex, s_directionalLightShadowmapSizeLabels, AZ_ARRAY_SIZE(s_directionalLightShadowmapSizeLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowmapSize(m_directionalLightHandle, s_shadowmapSizes[m_directionalLightShadowmapSizeIndex]);
            }

            ImGui::Text("Number of cascades");
            bool cascadesChanged = false;
            cascadesChanged = cascadesChanged ||
                ImGui::RadioButton("1", &m_cascadeCount, 1);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ImGui::RadioButton("2", &m_cascadeCount, 2);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ImGui::RadioButton("3", &m_cascadeCount, 3);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ImGui::RadioButton("4", &m_cascadeCount, 4);
            if (cascadesChanged)
            {
                m_directionalLightFeatureProcessor->SetCascadeCount(m_directionalLightHandle, m_cascadeCount);
            }

            ImGui::Spacing();

            ImGui::Text("Cascade partition scheme");
            ImGui::Text("  (uniform <--> logarithm)");
            if (ImGui::SliderFloat("Ratio", &m_ratioLogarithmUniform, 0.f, 1.f, "%0.3f"))
            {
                m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(m_directionalLightHandle, m_ratioLogarithmUniform);
            }

            ImGui::Spacing();

            if (ImGui::Checkbox("Cascade Position Correction", &m_isCascadeCorrectionEnabled))
            {
                m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(m_directionalLightHandle, m_isCascadeCorrectionEnabled);
            }

            if (ImGui::Checkbox("Debug Coloring", &m_isDebugColoringEnabled))
            {
                m_directionalLightFeatureProcessor->SetDebugFlags(m_directionalLightHandle,
                    m_isDebugColoringEnabled ?
                    AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawAll :
                    AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawNone);
            }

            ImGui::Spacing();

            ImGui::Text("Filtering");
            if (ImGui::Combo("Filter Method", &m_shadowFilterMethodIndex, s_shadowFilterMethodLabels, AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowFilterMethod(m_directionalLightHandle, s_shadowFilterMethods[m_shadowFilterMethodIndex]);
            }
            ImGui::Text("Boundary Width in meter");
            if (ImGui::SliderFloat("Width", &m_boundaryWidth, 0.f, 0.1f, "%.3f"))
            {
                m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(m_directionalLightHandle, m_boundaryWidth);
            }

            ImGui::Spacing();
            ImGui::Text("Filtering (PCF specific)");
            if (ImGui::SliderInt("Prediction #", &m_predictionSampleCount, 4, 16))
            {
                m_directionalLightFeatureProcessor->SetPredictionSampleCount(m_directionalLightHandle, m_predictionSampleCount);
            }
            if (ImGui::SliderInt("Filtering #", &m_filteringSampleCount, 4, 64))
            {
                m_directionalLightFeatureProcessor->SetFilteringSampleCount(m_directionalLightHandle, m_filteringSampleCount);
            }
        }
        ImGui::Unindent();

        ImGui::Separator();

        ImGui::Text("Disk Lights");
        ImGui::Indent();
        {
            int diskLightCount = m_diskLightCount;
            if (ImGui::SliderInt("Number", &diskLightCount, 0, DiskLightCountMax))
            {
                UpdateDiskLightCount(diskLightCount);
            }

            if (ImGui::SliderFloat("Intensity##disk", &m_diskLightIntensity, 0.f, 100000.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                for (const DiskLight& light : m_diskLights)
                {
                    if (light.m_handle.IsValid())
                    {
                        m_diskLightFeatureProcessor->SetRgbIntensity(light.m_handle, Render::PhotometricColor<Render::PhotometricUnit::Candela>(light.m_color * m_diskLightIntensity));
                        const float radius = sqrtf(m_diskLightIntensity / CutoffIntensity);
                        m_diskLightFeatureProcessor->SetAttenuationRadius(light.m_handle, radius);
                    }
                }
            }

            bool diskLightShadowmapChanged = ImGui::Checkbox("Enable Shadow", &m_diskLightShadowEnabled);
            
            ImGui::Text("Shadowmap Size");
            int newSize = static_cast<int>(m_diskLightShadowmapSize);
            // To avoid GPU memory consumption, we avoid bigger shadowmap sizes here.
            diskLightShadowmapChanged = diskLightShadowmapChanged ||
                ImGui::RadioButton("256", &newSize, static_cast<int>(Render::ShadowmapSize::Size256)) ||
                ImGui::RadioButton("512", &newSize, static_cast<int>(Render::ShadowmapSize::Size512)) ||
                ImGui::RadioButton("Random", &newSize, static_cast<int>(Render::ShadowmapSize::None));

            if (diskLightShadowmapChanged)
            {
                m_diskLightShadowmapSize = static_cast<Render::ShadowmapSize>(newSize);
                UpdateDiskLightShadowmapSize();
            }
        }
        ImGui::Unindent();

        // For automated screenshot verification testing: force the camera transform and turn on the debug window so the cull stats show up in the screenshot(s)
        if (ScriptableImGui::Button("Begin Verification"))
        {
            Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Disable);
            Transform tm = Transform::CreateTranslation(Vector3(3.0f, -16.0f, 6.0f));
            TransformBus::Event(GetCameraEntityId(), &TransformBus::Events::SetWorldTM, tm);
        }
        if (ScriptableImGui::Button("End Verification"))
        {
            Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<Debug::NoClipControllerComponent>());
        }

        m_imguiSidebar.End();
    }

    void CullingAndLodExampleComponent::UpdateDiskLightShadowmapSize()
    {
        using namespace AZ::Render;
        DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;

        if (!m_diskLightShadowEnabled)
        {
            // disabled shadows
            for (const DiskLight& light : m_diskLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowsEnabled(light.m_handle, false);
                }
            }
        }
        else if (m_diskLightShadowmapSize != ShadowmapSize::None)
        {
            // uniform size
            for (const DiskLight& light : m_diskLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowsEnabled(light.m_handle, true);
                    featureProcessor->SetShadowmapMaxResolution(light.m_handle, m_diskLightShadowmapSize);
                }
            }
        }
        else
        {
            // random sizes
            for (const DiskLight& light : m_diskLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowsEnabled(light.m_handle, true);
                    featureProcessor->SetShadowmapMaxResolution(light.m_handle, light.m_shadowmapSize);
                }
            }
        }
    }

} // namespace AtomSampleViewer
