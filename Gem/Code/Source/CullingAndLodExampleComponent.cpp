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
        UpdateSpotLightCount(0);
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
                auto meshHandle = meshFP->AcquireMesh(objectModelAsset, material);
                Transform modelToWorld = Transform::CreateTranslation(Vector3(x * spacing, y * spacing, 2.0f));
                meshFP->SetTransform(meshHandle, modelToWorld);
                m_meshHandles.push_back(AZStd::move(meshHandle));
            }
        }

        auto planeMeshHandle = meshFP->AcquireMesh(planeModelAsset, material);
        Transform planeScale = Transform::CreateScale(Vector3(numAlongXAxis * spacing, numAlongYAxis * spacing, 1.0f));
        Transform planeTranslation = Transform::CreateTranslation(Vector3(0.5f * numAlongXAxis * spacing, 0.5f * numAlongYAxis * spacing, 0.0f));
        Transform planeModelToWorld = planeTranslation * planeScale;
        meshFP->SetTransform(planeMeshHandle, planeModelToWorld);
        m_meshHandles.push_back(AZStd::move(planeMeshHandle));
    }

    void CullingAndLodExampleComponent::SetupLights()
    {
        using namespace AZ;

        m_directionalLightShadowmapSizeIndex = s_shadowmapSizeIndexDefault;
        m_spotLightShadowmapSize = Render::ShadowmapSize::None; // random
        m_cascadeCount = s_cascadesCountDefault;
        m_ratioLogarithmUniform = s_ratioLogarithmUniformDefault;
        m_spotLightCount = 0;

        RPI::Scene* scene = RPI::Scene::GetSceneForEntityContextId(GetEntityContextId());
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_spotLightFeatureProcessor = scene->GetFeatureProcessor<Render::SpotLightFeatureProcessorInterface>();

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

        // spot lights
        {
            m_spotLights.clear();
            m_spotLights.reserve(SpotLightCountMax);
            const float spotlightSpacing = 10.0f;
            const int spotLightsPerRow = 10;
            const Color colors[5] = {Colors::Red, Colors::Green, Colors::Blue, Colors::Orange, Colors::Pink};
            for (int index = 0; index < SpotLightCountMax; ++index)
            {
                float xPos = (index % spotLightsPerRow) * spotlightSpacing;
                float yPos = (index / spotLightsPerRow) * spotlightSpacing;
                m_spotLights.emplace_back(
                    colors[index % 5],
                    Vector3(xPos, yPos, 10.0f),
                    Vector3(1.0f, 1.0f, -1.0f).GetNormalized(),
                    Render::ShadowmapSize::Size256);
            }
            UpdateSpotLightCount(20);
        }
    }

    void CullingAndLodExampleComponent::UpdateSpotLightCount(uint16_t count)
    {
        using namespace AZ;

        for (int index = count; index < m_spotLightCount; ++index)
        {
            SpotLightHandle& handle = m_spotLights[index].m_handle;
            m_spotLightFeatureProcessor->ReleaseLight(handle);
        }

        const int previousSpotLightCount = m_spotLightCount;

        for (int index = previousSpotLightCount; index < count; ++index)
        {
            Render::SpotLightFeatureProcessorInterface* const spotLightFP = m_spotLightFeatureProcessor;
            const SpotLightHandle handle = spotLightFP->AcquireLight();
            const SpotLight &spotLight = m_spotLights[index];

            spotLightFP->SetPosition(handle, spotLight.m_position);
            spotLightFP->SetDirection(handle, spotLight.m_direction);
            spotLightFP->SetRgbIntensity(handle, Render::PhotometricColor<Render::PhotometricUnit::Candela>(spotLight.m_color * m_spotLightIntensity));
            const float radius = sqrtf(m_spotLightIntensity / CutoffIntensity);
            spotLightFP->SetAttenuationRadius(handle, radius);
            spotLightFP->SetShadowmapSize(handle, m_spotLightShadowEnabled ? m_spotLights[index].m_shadowmapSize : Render::ShadowmapSize::None);
            spotLightFP->SetConeAngles(handle, 45.f, 55.f);

            m_spotLights[index].m_handle = handle;
        }

        m_spotLightCount = count;
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

            if (ImGui::SliderFloat("Intensity##directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", 2.f))
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

        ImGui::Text("Spot Lights");
        ImGui::Indent();
        {
            int spotLightCount = m_spotLightCount;
            if (ImGui::SliderInt("Number", &spotLightCount, 0, SpotLightCountMax))
            {
                UpdateSpotLightCount(spotLightCount);
            }

            if (ImGui::SliderFloat("Intensity##spot", &m_spotLightIntensity, 0.f, 100000.f, "%.1f", 4.f))
            {
                for (const SpotLight& light : m_spotLights)
                {
                    if (light.m_handle.IsValid())
                    {
                        m_spotLightFeatureProcessor->SetRgbIntensity(light.m_handle, Render::PhotometricColor<Render::PhotometricUnit::Candela>(light.m_color * m_spotLightIntensity));
                        const float radius = sqrtf(m_spotLightIntensity / CutoffIntensity);
                        m_spotLightFeatureProcessor->SetAttenuationRadius(light.m_handle, radius);
                    }
                }
            }

            bool spotLightShadowmapChanged = ImGui::Checkbox("Enable Shadow", &m_spotLightShadowEnabled);
            
            ImGui::Text("Shadowmap Size");
            int newSize = static_cast<int>(m_spotLightShadowmapSize);
            // To avoid GPU memory consumption, we avoid bigger shadowmap sizes here.
            spotLightShadowmapChanged = spotLightShadowmapChanged ||
                ImGui::RadioButton("256", &newSize, static_cast<int>(Render::ShadowmapSize::Size256)) ||
                ImGui::RadioButton("512", &newSize, static_cast<int>(Render::ShadowmapSize::Size512)) ||
                ImGui::RadioButton("Random", &newSize, static_cast<int>(Render::ShadowmapSize::None));

            if (spotLightShadowmapChanged)
            {
                m_spotLightShadowmapSize = static_cast<Render::ShadowmapSize>(newSize);
                UpdateSpotLightShadowmapSize();
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

    void CullingAndLodExampleComponent::UpdateSpotLightShadowmapSize()
    {
        using namespace AZ::Render;
        SpotLightFeatureProcessorInterface* const featureProcessor = m_spotLightFeatureProcessor;

        if (!m_spotLightShadowEnabled)
        {
            // disabled shadows
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        ShadowmapSize::None);
                }
            }
        }
        else if (m_spotLightShadowmapSize != ShadowmapSize::None)
        {
            // uniform size
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        m_spotLightShadowmapSize);
                }
            }
        }
        else
        {
            // random sizes
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        light.m_shadowmapSize);
                }
            }
        }
    }

} // namespace AtomSampleViewer
