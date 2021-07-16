/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ShadowedSponzaExampleComponent.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <imgui/imgui.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzFramework/Components/CameraBus.h>

namespace AtomSampleViewer
{
    const AZ::Color ShadowedSponzaExampleComponent::DirectionalLightColor = AZ::Color::CreateOne();
    const AZ::Render::ShadowmapSize ShadowedSponzaExampleComponent::s_shadowmapSizes[] =
    {
        AZ::Render::ShadowmapSize::Size256,
        AZ::Render::ShadowmapSize::Size512,
        AZ::Render::ShadowmapSize::Size1024,
        AZ::Render::ShadowmapSize::Size2048
    };
    const char* ShadowedSponzaExampleComponent::s_directionalLightShadowmapSizeLabels[] =
    {
        "256",
        "512",
        "1024",
        "2048"
    };
    const AZ::Render::ShadowFilterMethod ShadowedSponzaExampleComponent::s_shadowFilterMethods[] =
    {
        AZ::Render::ShadowFilterMethod::None,
        AZ::Render::ShadowFilterMethod::Pcf,
        AZ::Render::ShadowFilterMethod::Esm,
        AZ::Render::ShadowFilterMethod::EsmPcf
    };
    const char* ShadowedSponzaExampleComponent::s_shadowFilterMethodLabels[] =
    {
        "None",
        "PCF",
        "ESM",
        "ESM+PCF"
    };

    void ShadowedSponzaExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ShadowedSponzaExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void ShadowedSponzaExampleComponent::Activate()
    {
        using namespace AZ;
        m_directionalLightShadowmapSizeIndex = s_shadowmapSizeIndexDefault;
        m_diskLightShadowmapSize = Render::ShadowmapSize::None; // random
        m_cascadeCount = s_cascadesCountDefault;
        m_ratioLogarithmUniform = s_ratioLogarithmUniformDefault;
        m_diskLightCount = 0;

        // heuristic disk light default position configuration
        m_diskLightsBasePosition[0] = 0.04f;
        m_diskLightsBasePosition[1] = 0.04f;;
        m_diskLightsBasePosition[2] = -0.03f;
        m_diskLightsPositionScatteringRatio = 0.27f;

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();

        SetupScene();

        // enable camera control
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<Debug::NoClipControllerComponent>());
        SaveCameraConfiguration();

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            75.f);

        m_cameraTransformInitialized = false;

        m_imguiSidebar.Activate();

        TickBus::Handler::BusConnect();

        // Don't continue the script until after the models have loaded.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);
    }

    void ShadowedSponzaExampleComponent::Deactivate()
    {
        using namespace AZ;

        TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        // disable camera control
        RestoreCameraConfiguration();
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Disable);

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        UpdateDiskLightCount(0);
    }

    void ShadowedSponzaExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(timePoint);

        using namespace AZ;

        SetInitialCameraTransform();

        const auto lightTrans = Transform::CreateRotationZ(m_directionalLightYaw) * Transform::CreateRotationX(m_directionalLightPitch);
        m_directionalLightFeatureProcessor->SetDirection(
            m_directionalLightHandle,
            lightTrans.GetBasis(1));

        DrawSidebar();

        // Camera Configuration
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(
                config,
                GetCameraEntityId(),
                &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(
                m_directionalLightHandle,
                config);
        }

        // Camera Transform
        {
            Transform transform = Transform::CreateIdentity();
            TransformBus::EventResult(
                transform,
                GetCameraEntityId(),
                &TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(
                m_directionalLightHandle, transform);
        }
    }

    void ShadowedSponzaExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        m_sponzaExteriorAssetLoaded = true;
        m_worldAabb = model->GetModelAsset()->GetAabb();
        UpdateDiskLightCount(DiskLightCountDefault);

        // Now that the models are initialized, we can allow the script to continue.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void ShadowedSponzaExampleComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(
            m_originalFarClipDistance,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void ShadowedSponzaExampleComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_originalFarClipDistance);
    }

    void ShadowedSponzaExampleComponent::SetInitialCameraTransform()
    {
        using namespace AZ;

        if (!m_cameraTransformInitialized)
        {
            Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(5.0f, 1.0f, 5.0f));
            Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetHeading, DegToRad(90.0f));
            Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetPitch, DegToRad(-20.0f));
            m_cameraTransformInitialized = true;
        }
    }

    void ShadowedSponzaExampleComponent::SetupScene()
    {
        using namespace AZ;

        const char* modelPath = "objects/sponza.azmodel";
        Data::Asset<RPI::ModelAsset> modelAsset =
            RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(modelPath, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ modelAsset }, RPI::Material::FindOrCreate(materialAsset));

        // rotate the entity 180 degrees about Z (the vertical axis)
        // This makes it consistent with how it was positioned in the world when the world was Y-up.
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, Transform::CreateRotationZ(AZ::Constants::Pi));

        Data::Instance<RPI::Model> model = GetMeshFeatureProcessor()->GetModel(m_meshHandle);
        if (model)
        {
            OnModelReady(model);
        }
        else
        {
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
        }

        // directional light
        {
            Render::DirectionalLightFeatureProcessorInterface* featureProcessor = m_directionalLightFeatureProcessor;
            const DirectionalLightHandle handle = featureProcessor->AcquireLight();

            const auto lightTransform = Transform::CreateLookAt(
                Vector3(100, 100, 100),
                Vector3::CreateZero());
            featureProcessor->SetDirection(
                handle,
                lightTransform.GetBasis(1));

            featureProcessor->SetRgbIntensity(handle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(DirectionalLightColor * m_directionalLightIntensity));
            featureProcessor->SetCascadeCount(handle, s_cascadesCountDefault);
            featureProcessor->SetShadowmapSize(handle, s_shadowmapSizes[s_shadowmapSizeIndexDefault]);
            featureProcessor->SetViewFrustumCorrectionEnabled(handle, m_isCascadeCorrectionEnabled);
            featureProcessor->SetShadowFilterMethod(handle, s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
            featureProcessor->SetShadowBoundaryWidth(handle, m_boundaryWidthDirectional);
            featureProcessor->SetPredictionSampleCount(handle, m_predictionSampleCountDirectional);
            featureProcessor->SetFilteringSampleCount(handle, m_filteringSampleCountDirectional);
            featureProcessor->SetGroundHeight(handle, 0.f);

            m_directionalLightHandle = handle;
            SetupDebugFlags();
        }

        // disk lights are initialized after loading models.
        BuildDiskLightParameters();
    }

    void ShadowedSponzaExampleComponent::BuildDiskLightParameters()
    {
        m_random.SetSeed(0);
        m_diskLights.clear();
        m_diskLights.reserve(DiskLightCountMax);
        for (int index = 0; index < DiskLightCountMax; ++index)
        {
            m_diskLights.emplace_back(
                GetRandomColor(),
                GetRandomPosition(),
                GetRandomShadowmapSize());
        }
    }

    void ShadowedSponzaExampleComponent::UpdateDiskLightCount(uint16_t count)
    {
        // We suppose m_diskLights has been initialized except m_entity.
        using namespace AZ;

        // Don't assert here if count == 0, since the count is set to 0 during Deactivate
        if ((!m_worldAabb.IsValid() || !m_worldAabb.IsFinite()) && count != 0)
        {
            AZ_Assert(false, "World AABB is not initialized correctly.");
            return;
        }

        for (int index = count; index < m_diskLightCount; ++index)
        {
            DiskLightHandle& handle = m_diskLights[index].m_handle;
            m_diskLightFeatureProcessor->ReleaseLight(handle);
        }

        const int previousDiskLightCount = m_diskLightCount;

        for (int index = previousDiskLightCount; index < count; ++index)
        {
            Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;
            const DiskLightHandle handle = featureProcessor->AcquireLight();

            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(m_diskLights[index].m_color * m_diskLightIntensity);
            featureProcessor->SetRgbIntensity(handle, lightColor);
            featureProcessor->SetAttenuationRadius(
                handle,
                sqrtf(m_diskLightIntensity / CutoffIntensity));
            featureProcessor->SetConeAngles(handle, DegToRad(22.5f), DegToRad(27.5));
            featureProcessor->SetShadowsEnabled(handle, m_diskLightShadowEnabled);
            if (m_diskLightShadowEnabled)
            {
                featureProcessor->SetShadowmapMaxResolution(
                    handle,
                    m_diskLightShadowEnabled ?
                    m_diskLights[index].m_shadowmapSize :
                    Render::ShadowmapSize::None);
                featureProcessor->SetShadowFilterMethod(handle, aznumeric_cast<Render::ShadowFilterMethod>(m_shadowFilterMethodIndexDisk));
                featureProcessor->SetPredictionSampleCount(handle, aznumeric_cast<uint16_t>(m_predictionSampleCountDisk));
                featureProcessor->SetFilteringSampleCount(handle, aznumeric_cast<uint16_t>(m_filteringSampleCountDisk));
            }
            m_diskLights[index].m_handle = handle;

            UpdateDiskLightPosition(index);
        }

        m_diskLightCount = count;
    }

    const AZ::Color& ShadowedSponzaExampleComponent::GetRandomColor()
    {
        static const AZStd::vector<AZ::Color> colors =
        {
            AZ::Colors::Red,
            AZ::Colors::Green,
            AZ::Colors::Blue,
            AZ::Colors::Cyan,
            AZ::Colors::Fuchsia,
            AZ::Colors::Yellow,
            AZ::Colors::SpringGreen
        };

        return colors[m_random.GetRandom() % colors.size()];
    }

    AZ::Vector3 ShadowedSponzaExampleComponent::GetRandomPosition()
    {
        // returns a position in the range [-0.5, +0.5]^3.
        return AZ::Vector3(
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f);
    }

    AZ::Render::ShadowmapSize ShadowedSponzaExampleComponent::GetRandomShadowmapSize() 
    {
        static const AZStd::vector<AZ::Render::ShadowmapSize> sizes =
        {
            AZ::Render::ShadowmapSize::Size256,
            AZ::Render::ShadowmapSize::Size512,
            AZ::Render::ShadowmapSize::Size1024,
            AZ::Render::ShadowmapSize::Size2048
        };

        return sizes[m_random.GetRandom() % sizes.size()];
    }

    void ShadowedSponzaExampleComponent::DrawSidebar()
    {
        using namespace AZ;
        using namespace AZ::Render;

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        if (!m_sponzaExteriorAssetLoaded)
        {
            const ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Asset", nullptr, windowFlags))
            {
                ImGui::Text("Sponza Model: %s", m_sponzaExteriorAssetLoaded ? "Loaded" : "Loading...");
                ImGui::End();
            }
            m_imguiSidebar.End();
            return;
        }

        ImGui::Spacing();

        ImGui::Text("Directional Light");
        ImGui::Indent();
        {
            ScriptableImGui::SliderAngle("Pitch", &m_directionalLightPitch, -90.0f, 0.f);
            ScriptableImGui::SliderAngle("Yaw", &m_directionalLightYaw, 0.f, 360.f);

            if (ScriptableImGui::SliderFloat("Intensity##directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(DirectionalLightColor * m_directionalLightIntensity);
                m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, lightColor);
            }

            ImGui::Separator();

            ImGui::Text("Shadowmap Size");
            if (ScriptableImGui::Combo(
                "Size",
                &m_directionalLightShadowmapSizeIndex,
                s_directionalLightShadowmapSizeLabels,
                AZ_ARRAY_SIZE(s_directionalLightShadowmapSizeLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowmapSize(
                    m_directionalLightHandle,
                    s_shadowmapSizes[m_directionalLightShadowmapSizeIndex]);
            }

            ImGui::Text("Number of cascades");
            bool cascadesChanged = false;
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("1", &m_cascadeCount, 1);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("2", &m_cascadeCount, 2);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("3", &m_cascadeCount, 3);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("4", &m_cascadeCount, 4);
            if (cascadesChanged)
            {
                m_directionalLightFeatureProcessor->SetCascadeCount(
                    m_directionalLightHandle,
                    m_cascadeCount);
            }

            ImGui::Spacing();

            ImGui::Text("Cascade partition scheme");
            ImGui::Text("  (uniform <--> logarithm)");
            if (ScriptableImGui::SliderFloat("Ratio", &m_ratioLogarithmUniform, 0.f, 1.f, "%0.3f"))
            {
                m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(
                    m_directionalLightHandle,
                    m_ratioLogarithmUniform);
            }

            ImGui::Spacing();

            if (ScriptableImGui::Checkbox("Cascade Position Correction", &m_isCascadeCorrectionEnabled))
            {
                m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(
                    m_directionalLightHandle,
                    m_isCascadeCorrectionEnabled);
            }

            bool DebugFlagsChanged = false;
            DebugFlagsChanged = DebugFlagsChanged || ScriptableImGui::Checkbox("Debug Coloring", &m_isDebugColoringEnabled);
            DebugFlagsChanged = DebugFlagsChanged || ScriptableImGui::Checkbox("Debug Bounding Box", &m_isDebugBoundingBoxEnabled);

            if (DebugFlagsChanged)
            {
                SetupDebugFlags();
            }

            ImGui::Spacing();
            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                "Filter Method##Directional",
                &m_shadowFilterMethodIndexDirectional,
                s_shadowFilterMethodLabels,
                AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowFilterMethod(
                    m_directionalLightHandle,
                    s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
            }

            if (m_shadowFilterMethodIndexDirectional != aznumeric_cast<int>(ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in meter");
                if (ScriptableImGui::SliderFloat("Width##Directional", &m_boundaryWidthDirectional, 0.f, 0.1f, "%.3f"))
                {
                    m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(
                        m_directionalLightHandle,
                        m_boundaryWidthDirectional);
                }
            }

            if (m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");

                int pcfMethodAsInteger = aznumeric_cast<int>(m_pcfMethodDirectional);
                if (ScriptableImGui::RadioButton(
                        "Boundary Search filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::BoundarySearch)))
                {
                    m_pcfMethodDirectional = PcfMethod::BoundarySearch;
                    m_directionalLightFeatureProcessor->SetPcfMethod(m_directionalLightHandle, m_pcfMethodDirectional);
                }
                if (ScriptableImGui::RadioButton("Bicubic filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::Bicubic)))
                {
                    m_pcfMethodDirectional = PcfMethod::Bicubic;
                    m_directionalLightFeatureProcessor->SetPcfMethod(m_directionalLightHandle, m_pcfMethodDirectional);
                }

                if (m_pcfMethodDirectional ==
                    AZ::Render::PcfMethod::BoundarySearch && ScriptableImGui::SliderInt(
                        "Prediction # ##Directional", &m_predictionSampleCountDirectional, 4, 16))
                {
                    m_directionalLightFeatureProcessor->SetPredictionSampleCount(
                        m_directionalLightHandle,
                        m_predictionSampleCountDirectional);
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Directional", &m_filteringSampleCountDirectional, 4, 64))
                {
                    m_directionalLightFeatureProcessor->SetFilteringSampleCount(
                        m_directionalLightHandle,
                        m_filteringSampleCountDirectional);
                }
            }
        }
        ImGui::Unindent();

        ImGui::Separator();

        ImGui::Text("Spot Lights");
        ImGui::Indent();
        {
            int diskLightCount = m_diskLightCount;
            if (ScriptableImGui::SliderInt("Number", &diskLightCount, 0, DiskLightCountMax))
            {
                UpdateDiskLightCount(diskLightCount);
            }

            if (ScriptableImGui::SliderFloat("Intensity##spot", &m_diskLightIntensity, 0.f, 100000.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                for (const DiskLight& light : m_diskLights)
                {
                    if (light.m_handle.IsValid())
                    {
                        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(light.m_color * m_diskLightIntensity);
                        m_diskLightFeatureProcessor->SetRgbIntensity(light.m_handle, lightColor);
                        m_diskLightFeatureProcessor->SetAttenuationRadius(
                            light.m_handle,
                            sqrtf(m_diskLightIntensity / CutoffIntensity));
                    }
                }
            }

            // avoiding SliderFloat3 since its sliders are too narrow.
            if (ScriptableImGui::SliderFloat("Center X", &m_diskLightsBasePosition[0], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Center Y", &m_diskLightsBasePosition[1], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Center Z", &m_diskLightsBasePosition[2], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Pos. Scatt. Ratio", &m_diskLightsPositionScatteringRatio, 0.f, 1.f))
            {
                UpdateDiskLightPositions();
            }

            bool diskLightShadowmapChanged = ScriptableImGui::Checkbox("Enable Shadow", &m_diskLightShadowEnabled);

            ImGui::Text("Shadowmap Size");
            int newSize = static_cast<int>(m_diskLightShadowmapSize);
            // To avoid GPU memory consumption, we avoid bigger shadowmap sizes here.
            diskLightShadowmapChanged = diskLightShadowmapChanged ||
                ScriptableImGui::RadioButton("256", &newSize, static_cast<int>(Render::ShadowmapSize::Size256)) ||
                ScriptableImGui::RadioButton("512", &newSize, static_cast<int>(Render::ShadowmapSize::Size512)) ||
                ScriptableImGui::RadioButton("1024", &newSize, static_cast<int>(Render::ShadowmapSize::Size1024)) ||
                ScriptableImGui::RadioButton("Random", &newSize, static_cast<int>(Render::ShadowmapSize::None));

            if (diskLightShadowmapChanged)
            {
                m_diskLightShadowmapSize = static_cast<Render::ShadowmapSize>(newSize);
                UpdateDiskLightShadowmapSize();
            }

            ImGui::Spacing();
            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                "Filter Method##Spot",
                &m_shadowFilterMethodIndexDisk,
                s_shadowFilterMethodLabels,
                AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                for (int index = 0; index < m_diskLightCount; ++index)
                {
                    m_diskLightFeatureProcessor->SetShadowFilterMethod(m_diskLights[index].m_handle, aznumeric_cast<ShadowFilterMethod>(m_shadowFilterMethodIndexDisk));
                }
            }

            if (m_shadowFilterMethodIndexDisk != aznumeric_cast<int>(ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in degrees");
                if (ScriptableImGui::SliderFloat("Width##Spot", &m_boundaryWidthDisk, 0.f, 1.f, "%.3f"))
                {
                    for (int index = 0; index < m_diskLightCount; ++index)
                    {
                        m_diskLightFeatureProcessor->SetSofteningBoundaryWidthAngle(m_diskLights[index].m_handle, DegToRad(m_boundaryWidthDisk));
                    }
                }
            }

            if (m_shadowFilterMethodIndexDisk == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndexDisk == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");
                if (ScriptableImGui::SliderInt("Predictiona # ##Spot", &m_predictionSampleCountDisk, 4, 16))
                {
                    for (int index = 0; index < m_diskLightCount; ++index)
                    {
                        m_diskLightFeatureProcessor->SetPredictionSampleCount(m_diskLights[index].m_handle, m_predictionSampleCountDisk);
                    }
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Spot", &m_filteringSampleCountDisk, 4, 64))
                {
                    for (int index = 0; index < m_diskLightCount; ++index)
                    {
                        m_diskLightFeatureProcessor->SetFilteringSampleCount(m_diskLights[index].m_handle, m_filteringSampleCountDisk);
                    }
                }
            }
        }
        ImGui::Unindent();

        m_imguiSidebar.End();
    }

    void ShadowedSponzaExampleComponent::UpdateDiskLightShadowmapSize()
    {
        using namespace AZ::Render;
        DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;

        for (const DiskLight& light : m_diskLights)
        {
            if (!light.m_handle.IsValid())
            {
                continue;
            }

            featureProcessor->SetShadowsEnabled(light.m_handle, m_diskLightShadowEnabled);
            if (m_diskLightShadowEnabled)
            {
                if (m_diskLightShadowmapSize != ShadowmapSize::None)
                {
                    // Uniform size
                    featureProcessor->SetShadowmapMaxResolution(
                        light.m_handle,
                        m_diskLightShadowmapSize);
                }
                else
                {
                    // Random sizes
                    featureProcessor->SetShadowmapMaxResolution(
                        light.m_handle,
                        light.m_shadowmapSize);
                }
            }
        }
    }

    void ShadowedSponzaExampleComponent::UpdateDiskLightPositions()
    {
        for (int index = 0; index < m_diskLightCount; ++index)
        {
            UpdateDiskLightPosition(index);
        }
    }

    void ShadowedSponzaExampleComponent::UpdateDiskLightPosition(int index)
    {
        using namespace AZ;
        Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;


        if (!m_worldAabb.IsValid() || !m_worldAabb.IsFinite())
        {
            AZ_Assert(false, "World AABB is not initialized correctly.");
            return;
        }

        const Vector3 basePosition(
            m_diskLightsBasePosition[0],
            m_diskLightsBasePosition[1],
            m_diskLightsBasePosition[2]);
        const Vector3 relativePosition = basePosition + m_diskLights[index].m_relativePosition * m_diskLightsPositionScatteringRatio;
        const Vector3 position = m_worldAabb.GetCenter() +
            m_worldAabb.GetExtents() * relativePosition;
        const auto transform =
            Transform::CreateTranslation(position) * Transform::CreateRotationX(-Constants::HalfPi);
        featureProcessor->SetPosition(
            m_diskLights[index].m_handle,
            position);
        featureProcessor->SetDirection(
            m_diskLights[index].m_handle,
            transform.GetBasis(1));
    }

    void ShadowedSponzaExampleComponent::SetupDebugFlags()
    {
        int flags = AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawNone;
        if (m_isDebugColoringEnabled)
        {
            flags |= AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawColoring;
        }
        if (m_isDebugBoundingBoxEnabled)
        {
            flags |= AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawBoundingBoxes;
        }
        m_directionalLightFeatureProcessor->SetDebugFlags(m_directionalLightHandle,
            static_cast<AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags>(flags));
    }
} // namespace AtomSampleViewer
