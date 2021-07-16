/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ShadowExampleComponent.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <AzCore/Component/Entity.h>
#include <AzFramework/Components/CameraBus.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <RHI/BasicRHIComponent.h>
#include <Atom/RPI.Public/ColorManagement/TransformColor.h>


namespace AtomSampleViewer
{
    const AZ::Color ShadowExampleComponent::DirectionalLightColor = AZ::Color::CreateOne();
    AZ::Color ShadowExampleComponent::s_positionalLightColors[] = {
        // they will be initialized in the constructor.
        AZ::Color::CreateZero(),
        AZ::Color::CreateZero(),
        AZ::Color::CreateZero()
    };

    const AZ::Render::ShadowmapSize ShadowExampleComponent::s_shadowmapImageSizes[] = 
    {
        AZ::Render::ShadowmapSize::Size256,
        AZ::Render::ShadowmapSize::Size512,
        AZ::Render::ShadowmapSize::Size1024,
        AZ::Render::ShadowmapSize::Size2048
    };
    const char* ShadowExampleComponent::s_shadowmapImageSizeLabels[] = 
    {
        "256",
        "512",
        "1024",
        "2048"
    };
    const AZ::Render::ShadowFilterMethod ShadowExampleComponent::s_shadowFilterMethods[] =
    {
        AZ::Render::ShadowFilterMethod::None,
        AZ::Render::ShadowFilterMethod::Pcf,
        AZ::Render::ShadowFilterMethod::Esm,
        AZ::Render::ShadowFilterMethod::EsmPcf
    };
    const char* ShadowExampleComponent::s_shadowFilterMethodLabels[] =
    {
        "None",
        "PCF",
        "ESM",
        "ESM+PCF"
    };

    ShadowExampleComponent::ShadowExampleComponent()
        : m_imguiSidebar("@user@/ShadowExampleComponent/sidebar.xml")
    {
        s_positionalLightColors[0] = AZ::Colors::Red;
        s_positionalLightColors[1] = AZ::Colors::Green;
        s_positionalLightColors[2] = AZ::Colors::Blue;
    }

    void ShadowExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ShadowExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void ShadowExampleComponent::Activate()
    {
        using namespace AZ;

        m_sampleName = "ShadowExampleComponent";

        // Don't continue the script until assets are ready and scene is setup
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {DefaultPbrMaterialPath, azrtti_typeid<RPI::MaterialAsset>()},
            {BunnyModelFilePath, azrtti_typeid<RPI::ModelAsset>()},
            {CubeModelFilePath, azrtti_typeid<RPI::ModelAsset>()}
        };

        PreloadAssets(assetList);
    }

    void ShadowExampleComponent::OnAllAssetsReadyActivate()
    {
        using namespace AZ;

        // Load the assets
        m_bunnyModelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(BunnyModelFilePath, RPI::AssetUtils::TraceLevel::Assert);
        m_floorModelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(CubeModelFilePath, RPI::AssetUtils::TraceLevel::Assert);
        m_materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        m_bunnyMeshIsReady = false;
        m_floorMeshIsReady = false;

        m_directionalLightImageSizeIndex = 2; // image size is 1024.
        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            m_positionalLightImageSizeIndices[index] = 2; // image size is 1024.
        }
        m_cascadeCount = 2;
        m_ratioLogarithmUniform = 0.5f;

        UseArcBallCameraController();

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();
        m_pointLightFeatureProcessor = scene->GetFeatureProcessor<Render::PointLightFeatureProcessorInterface>();

        CreateMeshes();
        CreateDirectionalLight();
        CreateDiskLights();

        m_elapsedTime = 0.f;
        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();
    }

    void ShadowExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        RestoreCameraConfiguration();
        RemoveController();

        GetMeshFeatureProcessor()->ReleaseMesh(m_floorMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_bunnyMeshHandle);
        DestroyDiskLights();
        DestroyPointLights();

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);

        m_imguiSidebar.Deactivate();
    }

    void ShadowExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint)
    {
        using namespace AZ;
        constexpr float directionalLightPeriodTime = 5.f; // 5 seconds for a rotation of directional light position.
        constexpr float lightPeriodTime = 7.f; // 7 seconds for a rotation of light positions.

        if (m_elapsedTime == 0.f)
        {
            SaveCameraConfiguration();
            SetInitialArcBallControllerParams();
            SetInitialShadowParams();
        }

        m_elapsedTime += deltaTime;

        if (m_isDirectionalLightAutoRotate)
        {
            m_directionalLightRotationAngle = fmodf(m_directionalLightRotationAngle + deltaTime * Constants::TwoPi / directionalLightPeriodTime, Constants::TwoPi);
        }
        if (m_isPositionalLightAutoRotate)
        {
            m_positionalLightRotationAngle = fmodf(m_positionalLightRotationAngle - deltaTime * Constants::TwoPi / lightPeriodTime + Constants::TwoPi, Constants::TwoPi);
        }


        UpdateDirectionalLight();

        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            const auto transform = GetTransformForLight(index);

            if (m_diskLightHandles[index].IsValid())
            {
                m_diskLightFeatureProcessor->SetPosition(m_diskLightHandles[index], transform.GetTranslation());
                m_diskLightFeatureProcessor->SetDirection(m_diskLightHandles[index], transform.GetBasis(1));
            }

            if (m_pointLightHandles[index].IsValid())
            {
                m_pointLightFeatureProcessor->SetPosition(m_pointLightHandles[index], transform.GetTranslation());
            }
        }
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovRadians,
            m_cameraFovY);


        DrawSidebar();
    }

    AZ::Transform ShadowExampleComponent::GetTransformForLight(const uint32_t index) const
    {
        constexpr float diskLightDist = 5.f;
        using namespace AZ;
        const float angle = m_positionalLightRotationAngle + index * Constants::TwoPi / 3;
        const auto location = Vector3(diskLightDist * sinf(angle), diskLightDist * cosf(angle), m_positionalLightHeights[index]);
        const auto transform = Transform::CreateLookAt(location, Vector3::CreateZero());
        return transform;
    }

    float ShadowExampleComponent::GetAttenuationForLight(const uint32_t index) const
    {
        return sqrtf(m_lightIntensities[index] / CutoffIntensity);
    }

    AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> ShadowExampleComponent::GetRgbIntensityForLight(
        const uint32_t index) const
    {
        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightRgbIntensity(
            s_positionalLightColors[index] * m_lightIntensities[index]);
       
        return lightRgbIntensity;
    }

    AZStd::pair<float, float> ShadowExampleComponent::GetConeAnglesForLight(const uint32_t index) const
    {
        const float innerConeAngle = AZ::DegToRad(m_outerConeAngles[index]) * ConeAngleInnerRatio;
        const float outerConeAngle = AZ::DegToRad(m_outerConeAngles[index]);

        return AZStd::make_pair(innerConeAngle, outerConeAngle);
    }


    void ShadowExampleComponent::UpdateDirectionalLight()
    {
        using namespace AZ;
        constexpr float directionalLightDist = 10.f;

        // Directional Light Transform
        {
            const auto lightLocation = Vector3(
                directionalLightDist * sinf(m_directionalLightRotationAngle), directionalLightDist * cosf(m_directionalLightRotationAngle),
                m_directionalLightHeight);
            const auto lightTransform = Transform::CreateLookAt(lightLocation, Vector3::CreateZero());
            m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightTransform.GetBasis(1));
        }

        // Camera Configuration
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(config, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(m_directionalLightHandle, config);
        }

        // Camera Transform
        {
            Transform transform = Transform::CreateIdentity();
            TransformBus::EventResult(transform, GetCameraEntityId(), &TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(m_directionalLightHandle, transform);
        }
    }

    void ShadowExampleComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(
            m_originalFarClipDistance,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::EventResult(
            m_originalCameraFovRadians,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFovRadians);
    }

    void ShadowExampleComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_originalFarClipDistance);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovRadians,
            m_originalCameraFovRadians);
    }

    void ShadowExampleComponent::UseArcBallCameraController()
    {
        using namespace AZ;
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<Debug::ArcBallControllerComponent>());
    }

    void ShadowExampleComponent::SetInitialArcBallControllerParams()
    {
        using namespace AZ;
        const auto cameraLocation = Vector3(0.f, -2.f, 2.f);
        Debug::ArcBallControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::ArcBallControllerRequestBus::Events::SetCenter,
            cameraLocation);
        Debug::ArcBallControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::ArcBallControllerRequestBus::Events::SetPitch,
            -Constants::QuarterPi);
        Debug::ArcBallControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::ArcBallControllerRequestBus::Events::SetDistance,
            50.f);
    }

    void ShadowExampleComponent::SetInitialShadowParams()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovRadians,
            AZ::Constants::QuarterPi);
    }

    void ShadowExampleComponent::RemoveController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void ShadowExampleComponent::CreateMeshes()
    {
        using namespace AZ;

        m_materialInstance = RPI::Material::FindOrCreate(m_materialAsset);
        m_floorMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ m_floorModelAsset }, m_materialInstance);
        m_bunnyMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ m_bunnyModelAsset }, m_materialInstance);

        auto updateFloorTransform = [&](Data::Instance<RPI::Model> model)
        {
            const AZ::Aabb& aabb = model->GetModelAsset()->GetAabb();
            const float maxZ = aabb.GetMax().GetZ();
            const AZ::Vector3 nonUniformScale{ 12.f, 12.f, 0.1f };
            const AZ::Vector3 translation{ 0.f, 0.f, -maxZ * nonUniformScale.GetZ() };
            const auto transform = AZ::Transform::CreateTranslation(translation);
            GetMeshFeatureProcessor()->SetTransform(m_floorMeshHandle, transform, nonUniformScale);
            m_floorMeshIsReady = true;
            if (m_bunnyMeshIsReady)
            {
                // Now that the models are initialized, we can allow the script to continue.
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
            }
        };

        auto updateBunnyTransform = [&](Data::Instance<RPI::Model> model)
        {
            const AZ::Aabb& aabb = model->GetModelAsset()->GetAabb();
            const float minZ = aabb.GetMin().GetZ();
            const AZ::Vector3 translation{ 0.f, 0.f, -minZ };
            auto transform = AZ::Transform::CreateTranslation(translation);
            transform.SetUniformScale(1.5f);
            GetMeshFeatureProcessor()->SetTransform(m_bunnyMeshHandle, transform);
            m_bunnyMeshIsReady = true;
            if (m_floorMeshIsReady)
            {
                // Now that the models are initialized, we can allow the script to continue.
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
            }
        };

        m_floorReadyHandle = ModelChangedHandler(updateFloorTransform);
        m_bunnyReadyHandle = ModelChangedHandler(updateBunnyTransform);

        GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_floorMeshHandle, m_floorReadyHandle);
        GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_bunnyMeshHandle, m_bunnyReadyHandle);

        // Currently there's no way for the mesh feature procesor to announce change on connect if the model is ready already.
        // This can go away when AZ::Event::Handler's callback can be called publicly.
        Data::Instance<RPI::Model> floorModel = GetMeshFeatureProcessor()->GetModel(m_floorMeshHandle);
        if (floorModel)
        {
            updateFloorTransform(floorModel);
        }

        Data::Instance<RPI::Model> bunnyModel = GetMeshFeatureProcessor()->GetModel(m_bunnyMeshHandle);
        if (bunnyModel)
        {
            updateBunnyTransform(bunnyModel);
        }
    }

    void ShadowExampleComponent::CreateDirectionalLight()
    {
        using namespace AZ;

        Render::DirectionalLightFeatureProcessorInterface* const featureProcessor = m_directionalLightFeatureProcessor;
        const DirectionalLightHandle handle = featureProcessor->AcquireLight();

        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(DirectionalLightColor * m_directionalLightIntensity);
        featureProcessor->SetRgbIntensity(handle, lightColor);
        featureProcessor->SetShadowmapSize(handle, s_shadowmapImageSizes[m_directionalLightImageSizeIndex]);
        featureProcessor->SetShadowFarClipDistance(handle, FarClipDistance);
        featureProcessor->SetCascadeCount(handle, m_cascadeCount);
        featureProcessor->SetShadowmapFrustumSplitSchemeRatio(handle, m_ratioLogarithmUniform);
        featureProcessor->SetViewFrustumCorrectionEnabled(handle, m_isCascadeCorrectionEnabled);
        featureProcessor->SetShadowFilterMethod(handle, s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
        featureProcessor->SetShadowBoundaryWidth(handle, m_boundaryWidthDirectional);
        featureProcessor->SetPredictionSampleCount(handle, m_predictionSampleCountDirectional);
        featureProcessor->SetFilteringSampleCount(handle, m_filteringSampleCountDirectional);
        featureProcessor->SetGroundHeight(handle, 0.f);

        m_directionalLightHandle = handle;
        SetupDebugFlags();
    }

    void ShadowExampleComponent::CreateDiskLights()
    {
        DestroyDiskLights();

        using namespace AZ;
        Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;

        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            const DiskLightHandle handle = featureProcessor->AcquireLight();
            AZ_Assert(m_diskLightHandles[index].IsNull(), "Unreleased light");
            m_diskLightHandles[index] = handle;
        }
        ApplyDiskLightSettings();
    }
    void ShadowExampleComponent::CreatePointLights()
    {
        DestroyPointLights();

        using namespace AZ;
        Render::PointLightFeatureProcessorInterface* const featureProcessor = m_pointLightFeatureProcessor;

        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            const PointLightHandle handle = featureProcessor->AcquireLight();        
            AZ_Assert(m_pointLightHandles[index].IsNull(), "Unreleased light");
            m_pointLightHandles[index] = handle;
        }
        ApplyPointLightSettings();
    }

    void ShadowExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();

            DrawSidebarDirectionalLight();

            ImGui::Separator();

            if (DrawSidebarPositionalLights())
            {
                ApplyDiskLightSettings();
                ApplyPointLightSettings();
            }

            ImGui::Separator();

            DrawSidebarCamera();

            ImGui::Separator();

            if (ScriptableImGui::Button("Material Details..."))
            {
                m_imguiMaterialDetails.SetMaterial(m_materialInstance);
                m_imguiMaterialDetails.OpenDialog();
            }

            m_imguiSidebar.End();
        }
        m_imguiMaterialDetails.Tick();
    }

    void ShadowExampleComponent::DrawSidebarDirectionalLight()
    {
        ImGui::Indent();
        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ScriptableImGui::SliderFloat("Height##Directional", &m_directionalLightHeight, 1.f, 30.f, "%.1f", ImGuiSliderFlags_Logarithmic);

            ScriptableImGui::Checkbox("Auto Rotation##Directional", &m_isDirectionalLightAutoRotate);
            ScriptableImGui::SliderAngle("Direction##Directional", &m_directionalLightRotationAngle, 0, 360);

            if (ScriptableImGui::SliderFloat(
                    "Intensity##Directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(
                    DirectionalLightColor * m_directionalLightIntensity);
                m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, lightColor);
            }

            ImGui::Text("Shadowmap Size");
            if (ScriptableImGui::Combo(
                    "Size##Directional", &m_directionalLightImageSizeIndex, s_shadowmapImageSizeLabels,
                    AZ_ARRAY_SIZE(s_shadowmapImageSizeLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowmapSize(
                    m_directionalLightHandle, s_shadowmapImageSizes[m_directionalLightImageSizeIndex]);
            }

            ImGui::Text("Number of cascades");
            bool cascadesChanged = false;
            cascadesChanged = cascadesChanged || ScriptableImGui::RadioButton("1", &m_cascadeCount, 1);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged || ScriptableImGui::RadioButton("2", &m_cascadeCount, 2);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged || ScriptableImGui::RadioButton("3", &m_cascadeCount, 3);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged || ScriptableImGui::RadioButton("4", &m_cascadeCount, 4);
            if (cascadesChanged)
            {
                m_directionalLightFeatureProcessor->SetCascadeCount(m_directionalLightHandle, m_cascadeCount);
            }

            ImGui::Spacing();
            bool cascadeDepthIsChanged = ScriptableImGui::Checkbox("Automatic Cascade Split", &m_shadowmapFrustumSplitIsAutomatic);
            if (m_shadowmapFrustumSplitIsAutomatic)
            {
                ImGui::Text("Cascade partition scheme");
                ImGui::Text("  (uniform <--> logarithm)");
                cascadeDepthIsChanged =
                    cascadeDepthIsChanged || ScriptableImGui::SliderFloat("Ratio", &m_ratioLogarithmUniform, 0.f, 1.f, "%0.3f");
                if (cascadeDepthIsChanged)
                {
                    m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(
                        m_directionalLightHandle, m_ratioLogarithmUniform);
                }
            }
            else
            {
                for (int cascadeIndex = 0; cascadeIndex < m_cascadeCount; ++cascadeIndex)
                {
                    const AZStd::string label = AZStd::string::format("FarDepth %d", cascadeIndex);
                    cascadeDepthIsChanged =
                        cascadeDepthIsChanged || ScriptableImGui::SliderFloat(label.c_str(), &m_cascadeFarDepth[cascadeIndex], 0.01f, 20.f);
                }
                if (cascadeDepthIsChanged)
                {
                    for (int cascadeIndex = 0; cascadeIndex < m_cascadeCount; ++cascadeIndex)
                    {
                        m_directionalLightFeatureProcessor->SetCascadeFarDepth(
                            m_directionalLightHandle, aznumeric_cast<uint16_t>(cascadeIndex), m_cascadeFarDepth[cascadeIndex]);
                    }
                }
            }

            ImGui::Spacing();

            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                    "Filter Method##Directional", &m_shadowFilterMethodIndexDirectional, s_shadowFilterMethodLabels,
                    AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowFilterMethod(
                    m_directionalLightHandle, s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
            }
            if (m_shadowFilterMethodIndexDirectional != aznumeric_cast<int>(AZ::Render::ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in meter");
                if (ScriptableImGui::SliderFloat("Width##Directional", &m_boundaryWidthDirectional, 0.f, 0.1f, "%.3f"))
                {
                    m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(m_directionalLightHandle, m_boundaryWidthDirectional);
                }
            }

            if (m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(AZ::Render::ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(AZ::Render::ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");
                if (ScriptableImGui::SliderInt("Prediction # ##Directional", &m_predictionSampleCountDirectional, 4, 16))
                {
                    m_directionalLightFeatureProcessor->SetPredictionSampleCount(
                        m_directionalLightHandle, m_predictionSampleCountDirectional);
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Directional", &m_filteringSampleCountDirectional, 4, 64))
                {
                    m_directionalLightFeatureProcessor->SetFilteringSampleCount(
                        m_directionalLightHandle, m_filteringSampleCountDirectional);
                }
            }

            ImGui::Spacing();

            bool debugFlagsChanged = false;
            debugFlagsChanged = ScriptableImGui::Checkbox("Debug Coloring", &m_isDebugColoringEnabled) || debugFlagsChanged;
            debugFlagsChanged = ScriptableImGui::Checkbox("Debug Bounding Box", &m_isDebugBoundingBoxEnabled) || debugFlagsChanged;

            if (debugFlagsChanged)
            {
                SetupDebugFlags();
            }

            if (ScriptableImGui::Checkbox("Cascade Position Correction", &m_isCascadeCorrectionEnabled))
            {
                m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(m_directionalLightHandle, m_isCascadeCorrectionEnabled);
            }
        }
        ImGui::Unindent();
    }

    bool ShadowExampleComponent::DrawSidebarPositionalLights()
    {
        using namespace AZ::Render;
        bool settingsChanged = false;
        ImGui::Indent();
        if (ImGui::CollapsingHeader("Positional Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Text("Light Type");
            if (ScriptableImGui::RadioButton("Disk", &m_positionalLightTypeActive, 0))
            {
                DestroyPointLights();
                CreateDiskLights();
                settingsChanged = true;
            }
            if (ScriptableImGui::RadioButton("Point", &m_positionalLightTypeActive, 1))
            {
                DestroyDiskLights();
                CreatePointLights();
                settingsChanged = true;
            }

            ScriptableImGui::Checkbox("Auto Rotation##Positional", &m_isPositionalLightAutoRotate);
            ScriptableImGui::SliderAngle("Base Direction##Positional", &m_positionalLightRotationAngle, 0, 360);

            ImGui::Spacing();

            ImGui::Text("Control Target");
            ScriptableImGui::RadioButton("Red", &m_controlTargetPositionalLightIndex, 0);
            ImGui::SameLine();
            ScriptableImGui::RadioButton("Green", &m_controlTargetPositionalLightIndex, 1);
            ImGui::SameLine();
            ScriptableImGui::RadioButton("Blue", &m_controlTargetPositionalLightIndex, 2);

            const int index = m_controlTargetPositionalLightIndex;

            ScriptableImGui::SliderFloat(
                "Height##Positional", &m_positionalLightHeights[index], 1.f, 30.f, "%.1f", ImGuiSliderFlags_Logarithmic);

            if (ScriptableImGui::SliderFloat("Cone Angle", &m_outerConeAngles[index], 0.f, 120.f))
            {
                settingsChanged = true;
            }

            if (ScriptableImGui::SliderFloat(
                    "Intensity##Positional", &m_lightIntensities[index], 0.f, 20000.f, "%.1f", ImGuiSliderFlags_Logarithmic))
            {
                settingsChanged = true;
            }

            bool shadowmapSizeChanged = ScriptableImGui::Checkbox("Enable Shadow", &m_positionalLightShadowEnabled[index]);

            ImGui::Text("Shadowmap Size");
            shadowmapSizeChanged = shadowmapSizeChanged ||
                ScriptableImGui::Combo("Size##Positional", &m_positionalLightImageSizeIndices[index], s_shadowmapImageSizeLabels,
                                       AZ_ARRAY_SIZE(s_shadowmapImageSizeLabels));
            if (shadowmapSizeChanged)
            {
                // Reset shadow parameters when shadow is disabled.
                if (!m_positionalLightShadowEnabled[index])
                {
                    m_shadowFilterMethodIndicesPositional[index] = 0;
                    m_boundaryWidthsPositional[index] = 0.f;
                    m_predictionSampleCountsPositional[index] = 0;
                    m_filteringSampleCountsPositional[index] = 0;
                }
                settingsChanged = true;
            }

            ImGui::Spacing();

            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                    "Filter Method##Positional", &m_shadowFilterMethodIndicesPositional[index], s_shadowFilterMethodLabels,
                    AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                settingsChanged = true;
            }

            if (m_shadowFilterMethodIndicesPositional[index] != aznumeric_cast<int>(ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in degrees");
                if (ScriptableImGui::SliderFloat("Width##Positional", &m_boundaryWidthsPositional[index], 0.f, 1.0f, "%.3f"))
                {
                    settingsChanged = true;
                }
            }

            if (m_shadowFilterMethodIndicesPositional[index] == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndicesPositional[index] == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");

                if (m_pcfMethod[index] == PcfMethod::BoundarySearch &&
                    ScriptableImGui::SliderInt("Prediction # ##Positional", &m_predictionSampleCountsPositional[index], 4, 16))
                {
                    settingsChanged = true;
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Positional", &m_filteringSampleCountsPositional[index], 4, 64))
                {
                    settingsChanged = true;
                }

                int pcfMethodAsInteger = aznumeric_cast<int>(m_pcfMethod[index]);
                if (ScriptableImGui::RadioButton(
                        "Boundary Search filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::BoundarySearch)))
                {
                    m_pcfMethod[index] = PcfMethod::BoundarySearch;
                    settingsChanged = true;
                }
                if (ScriptableImGui::RadioButton("Bicubic filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::Bicubic)))
                {
                    m_pcfMethod[index] = PcfMethod::Bicubic;
                    settingsChanged = true;
                }
            }
        }
        ImGui::Unindent();
        return settingsChanged;
    }

    void ShadowExampleComponent::DrawSidebarCamera()
    {
        ImGui::Indent();
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            using namespace AZ;

            ScriptableImGui::SliderAngle("FoVY", &m_cameraFovY, 1.f, 120.f);
            ImGui::Spacing();

            Transform cameraTransform;
            TransformBus::EventResult(cameraTransform, GetCameraEntityId(), &TransformBus::Events::GetWorldTM);
            const Vector3 eularDegrees = cameraTransform.GetEulerDegrees();
            float cameraPitch = eularDegrees.GetElement(0);
            const float cameraYaw = eularDegrees.GetElement(1);
            if (cameraPitch > 180.f)
            {
                cameraPitch -= 360.f;
            }
            ImGui::Text("Pitch: %f", cameraPitch);
            ImGui::Text("Yaw:   %f", cameraYaw);
        }
        ImGui::Unindent();
    }

    void ShadowExampleComponent::SetupDebugFlags()
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

    void ShadowExampleComponent::DestroyDiskLights()
    {
        for (DiskLightHandle& handle : m_diskLightHandles)
        {
            m_diskLightFeatureProcessor->ReleaseLight(handle);
        }
    }

    void ShadowExampleComponent::DestroyPointLights()
    {
        for (PointLightHandle& handle : m_pointLightHandles)
        {
            m_pointLightFeatureProcessor->ReleaseLight(handle);
        }
    }

    void ShadowExampleComponent::ApplyDiskLightSettings()
    {
        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            auto lightHandle = m_diskLightHandles[index];
            if (lightHandle.IsValid())
            {
                auto [innerConeAngle, outerConeAngle] = GetConeAnglesForLight(index);
                m_diskLightFeatureProcessor->SetConeAngles(lightHandle, innerConeAngle, outerConeAngle);
                m_diskLightFeatureProcessor->SetPosition(lightHandle, GetTransformForLight(index).GetTranslation());
                m_diskLightFeatureProcessor->SetDirection(lightHandle, GetTransformForLight(index).GetBasis(1));
                m_diskLightFeatureProcessor->SetAttenuationRadius(lightHandle, GetAttenuationForLight(index));
                m_diskLightFeatureProcessor->SetRgbIntensity(lightHandle, GetRgbIntensityForLight(index));

                const bool shadowEnabled = m_positionalLightShadowEnabled[index];
                m_diskLightFeatureProcessor->SetShadowsEnabled(lightHandle, shadowEnabled);
                if (shadowEnabled)
                {
                    m_diskLightFeatureProcessor->SetShadowmapMaxResolution(
                        lightHandle, s_shadowmapImageSizes[m_positionalLightImageSizeIndices[index]]);

                    m_diskLightFeatureProcessor->SetShadowFilterMethod(
                        lightHandle, s_shadowFilterMethods[m_shadowFilterMethodIndicesPositional[index]]);

                    m_diskLightFeatureProcessor->SetSofteningBoundaryWidthAngle(lightHandle, AZ::DegToRad(m_boundaryWidthsPositional[index]));
                    m_diskLightFeatureProcessor->SetFilteringSampleCount(lightHandle, m_filteringSampleCountsPositional[index]);
                    m_diskLightFeatureProcessor->SetPredictionSampleCount(lightHandle, m_predictionSampleCountsPositional[index]);
                    m_diskLightFeatureProcessor->SetPcfMethod(lightHandle, m_pcfMethod[index]);
                }
            }
        }
    }

    void ShadowExampleComponent::ApplyPointLightSettings()
    {
        for (uint32_t index = 0; index < PositionalLightCount; ++index)
        {
            auto lightHandle = m_pointLightHandles[index];
            if (lightHandle.IsValid())
            {

                m_pointLightFeatureProcessor->SetPosition(lightHandle, GetTransformForLight(index).GetTranslation());
                m_pointLightFeatureProcessor->SetBulbRadius(lightHandle, 0.0f);
                m_pointLightFeatureProcessor->SetAttenuationRadius(lightHandle, GetAttenuationForLight(index));
                m_pointLightFeatureProcessor->SetRgbIntensity(lightHandle, GetRgbIntensityForLight(index));

                const bool shadowEnabled = m_positionalLightShadowEnabled[index];
                m_pointLightFeatureProcessor->SetShadowsEnabled(lightHandle, shadowEnabled);
                if (shadowEnabled)
                {
                    m_pointLightFeatureProcessor->SetShadowmapMaxResolution(
                        lightHandle, s_shadowmapImageSizes[m_positionalLightImageSizeIndices[index]]);

                    m_pointLightFeatureProcessor->SetShadowFilterMethod(
                        lightHandle, s_shadowFilterMethods[m_shadowFilterMethodIndicesPositional[index]]);

                    m_pointLightFeatureProcessor->SetPcfMethod(lightHandle, m_pcfMethod[index]);
                    m_pointLightFeatureProcessor->SetFilteringSampleCount(lightHandle, m_filteringSampleCountsPositional[index]);
                    m_pointLightFeatureProcessor->SetPredictionSampleCount(lightHandle, m_predictionSampleCountsPositional[index]);
                    m_pointLightFeatureProcessor->SetSofteningBoundaryWidthAngle(lightHandle, AZ::DegToRad(m_boundaryWidthsPositional[index]));
                }
            }
        }
    }

} // namespace AtomSampleViewer
