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

namespace AtomSampleViewer
{
    namespace
    {
        const char* BunnyModelFilePath = "objects/bunny.azmodel";
        const char* CubeModelFilePath = "testdata/objects/cube/cube.azmodel";
    };

    const AZ::Color ShadowExampleComponent::DirectionalLightColor = AZ::Color::CreateOne();
    AZ::Color ShadowExampleComponent::s_spotLightColors[] = {
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
        s_spotLightColors[0] = AZ::Colors::Red;
        s_spotLightColors[1] = AZ::Colors::Green;
        s_spotLightColors[2] = AZ::Colors::Blue;
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
        for (uint32_t index = 0; index < SpotLightCount; ++index)
        {
            m_spotLightImageSizeIndices[index] = 2; // image size is 1024.
        }
        m_cascadeCount = 2;
        m_ratioLogarithmUniform = 0.5f;

        UseArcBallCameraController();

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_spotLightFeatureProcessor = scene->GetFeatureProcessor<Render::SpotLightFeatureProcessorInterface>();

        CreateMeshes();
        CreateDirectionalLight();
        CreateSpotLights();

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

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        for (SpotLightHandle& handle : m_spotLightHandles)
        {
            m_spotLightFeatureProcessor->ReleaseLight(handle);
        }

        m_imguiSidebar.Deactivate();
    }

    void ShadowExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint)
    {
        using namespace AZ;
        constexpr float directionalLightPeriodTime = 5.f; // 5 seconds for a rotation of directional light position.
        constexpr float spotLightPeriodTime = 7.f; // 7 seconds for a rotation of spot light positions.
        constexpr float directionalLightDist = 10.f;
        constexpr float spotLightDist = 5.f;

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
        if (m_isSpotLightAutoRotate)
        {
            m_spotLightRotationAngle = fmodf(m_spotLightRotationAngle - deltaTime * Constants::TwoPi / spotLightPeriodTime + Constants::TwoPi, Constants::TwoPi);
        }

        // Directional Light Transform
        {
            const auto lightLocation = Vector3(
                directionalLightDist * sinf(m_directionalLightRotationAngle),
                directionalLightDist * cosf(m_directionalLightRotationAngle),
                m_directionalLightHeight);
            const auto lightTransform = Transform::CreateLookAt(
                lightLocation,
                Vector3::CreateZero());
            m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightTransform.GetBasis(1));
        }

        // Spot Lights Transform
        for (uint32_t index = 0; index < SpotLightCount; ++index)
        { 
            const float angle = m_spotLightRotationAngle + index * Constants::TwoPi / 3;
            const auto location = Vector3(
                spotLightDist * sinf(angle),
                spotLightDist * cosf(angle),
                m_spotLightHeights[index]);
            const auto transform = Transform::CreateLookAt(
                location,
                Vector3::CreateZero());
            m_spotLightFeatureProcessor->SetPosition(m_spotLightHandles[index], location);
            m_spotLightFeatureProcessor->SetDirection(m_spotLightHandles[index], transform.GetBasis(1));
        }

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovRadians,
            m_cameraFovY);

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

        DrawSidebar();
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
        m_floorMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(m_floorModelAsset, m_materialInstance);
        m_bunnyMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(m_bunnyModelAsset, m_materialInstance);

        auto updateFloorTransform = [&](Data::Instance<RPI::Model> model)
        {
            const AZ::Aabb& aabb = model->GetAabb();
            const float maxZ = aabb.GetMax().GetZ();
            const AZ::Vector3 scale{ 12.f, 12.f, 0.1f };
            const AZ::Vector3 translation{ 0.f, 0.f, -maxZ * scale.GetZ() };
            const auto transform = AZ::Transform::CreateTranslation(translation) *
                AZ::Transform::CreateScale(scale);
            GetMeshFeatureProcessor()->SetTransform(m_floorMeshHandle, transform);
            m_floorMeshIsReady = true;
            if (m_bunnyMeshIsReady)
            {
                // Now that the models are initialized, we can allow the script to continue.
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
            }
        };

        auto updateBunnyTransform = [&](Data::Instance<RPI::Model> model)
        {
            const AZ::Aabb& aabb = model->GetAabb();
            const float minZ = aabb.GetMin().GetZ();
            const AZ::Vector3 translation{ 0.f, 0.f, -minZ };
            const auto transform = AZ::Transform::CreateTranslation(translation);
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

    void ShadowExampleComponent::CreateSpotLights()
    {
        using namespace AZ;
        Render::SpotLightFeatureProcessorInterface* const featureProcessor = m_spotLightFeatureProcessor;

        for (uint32_t index = 0; index < SpotLightCount; ++index)
        {
            const SpotLightHandle handle = featureProcessor->AcquireLight();

            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(s_spotLightColors[index] * m_spotLightIntensities[index]);
            featureProcessor->SetRgbIntensity(handle, lightColor);
            featureProcessor->SetAttenuationRadius(
                handle,
                sqrtf(m_spotLightIntensities[index] / CutoffIntensity));
            featureProcessor->SetConeAngles(
                handle,
                m_outerConeAngles[index] * ConeAngleInnerRatio,
                m_outerConeAngles[index]);
            featureProcessor->SetShadowmapSize(
                handle,
                m_spotLightShadowEnabled[index] ?
                s_shadowmapImageSizes[m_spotLightImageSizeIndices[index]] :
                Render::ShadowmapSize::None);
            featureProcessor->SetShadowFilterMethod(handle, s_shadowFilterMethods[m_shadowFilterMethodIndicesSpot[index]]);
            featureProcessor->SetShadowBoundaryWidthAngle(handle, m_boundaryWidthsSpot[index]);
            featureProcessor->SetPredictionSampleCount(handle, m_predictionSampleCountsSpot[index]);
            featureProcessor->SetFilteringSampleCount(handle, m_filteringSampleCountsSpot[index]);
            featureProcessor->SetPcfMethod(handle, m_pcfMethod[index]);
            m_spotLightHandles[index] = handle;
        }
    }

    void ShadowExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();

            ImGui::Text("Directional Light");
            ImGui::Indent();
            {
                ImGui::SliderFloat("Height##Directional", &m_directionalLightHeight, 1.f, 30.f, "%.1f", 2.f);

                ScriptableImGui::Checkbox("Auto Rotation##Directional", &m_isDirectionalLightAutoRotate);
                ScriptableImGui::SliderAngle("Direction##Directional", &m_directionalLightRotationAngle, 0, 360);

                if (ScriptableImGui::SliderFloat("Intensity##Directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", 2.f))
                {
                    AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(DirectionalLightColor * m_directionalLightIntensity);
                    m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, lightColor);
                }

                ImGui::Separator();

                ImGui::Text("Shadowmap Size");
                if (ScriptableImGui::Combo(
                    "Size##Directional",
                    &m_directionalLightImageSizeIndex,
                    s_shadowmapImageSizeLabels,
                    AZ_ARRAY_SIZE(s_shadowmapImageSizeLabels)))
                {
                    m_directionalLightFeatureProcessor->SetShadowmapSize(
                        m_directionalLightHandle,
                        s_shadowmapImageSizes[m_directionalLightImageSizeIndex]);
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
                bool cascadeDepthIsChanged = 
                    ScriptableImGui::Checkbox("Automatic Cascade Split", &m_shadowmapFrustumSplitIsAutomatic);
                if (m_shadowmapFrustumSplitIsAutomatic)
                {
                    ImGui::Text("Cascade partition scheme");
                    ImGui::Text("  (uniform <--> logarithm)");
                    cascadeDepthIsChanged = cascadeDepthIsChanged ||
                        ScriptableImGui::SliderFloat("Ratio", &m_ratioLogarithmUniform, 0.f, 1.f, "%0.3f");
                    if (cascadeDepthIsChanged)
                    {
                        m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(
                            m_directionalLightHandle,
                            m_ratioLogarithmUniform);
                    }
                }
                else
                {
                    for (int cascadeIndex = 0; cascadeIndex < m_cascadeCount; ++cascadeIndex)
                    {
                        const AZStd::string label = AZStd::string::format("FarDepth %d", cascadeIndex);
                        cascadeDepthIsChanged = cascadeDepthIsChanged ||
                            ScriptableImGui::SliderFloat(label.c_str(), &m_cascadeFarDepth[cascadeIndex], 0.01f, 20.f);
                    }
                    if (cascadeDepthIsChanged)
                    {
                        for (int cascadeIndex = 0; cascadeIndex < m_cascadeCount; ++cascadeIndex)
                        {
                            m_directionalLightFeatureProcessor->SetCascadeFarDepth(
                                m_directionalLightHandle,
                                aznumeric_cast<uint16_t>(cascadeIndex),
                                m_cascadeFarDepth[cascadeIndex]);
                        }
                    }
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
                    if (ScriptableImGui::SliderInt("Prediction # ##Directional", &m_predictionSampleCountDirectional, 4, 16))
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
                    m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(
                        m_directionalLightHandle,
                        m_isCascadeCorrectionEnabled);
                }
            }
            ImGui::Unindent();


            ImGui::Separator();

            ImGui::Text("Spot Lights");
            ImGui::Indent();
            {
                ScriptableImGui::Checkbox("Auto Rotation##Spot", &m_isSpotLightAutoRotate);
                ScriptableImGui::SliderAngle("Base Direction##Spot", &m_spotLightRotationAngle, 0, 360);

                ImGui::Spacing();

                ImGui::Text("Control Target");
                ScriptableImGui::RadioButton("Red", &m_controlTargetSpotLightIndex, 0);
                ImGui::SameLine();
                ScriptableImGui::RadioButton("Green", &m_controlTargetSpotLightIndex, 1);
                ImGui::SameLine();
                ScriptableImGui::RadioButton("Blue", &m_controlTargetSpotLightIndex, 2);

                const int index = m_controlTargetSpotLightIndex;
                const SpotLightHandle lightId = m_spotLightHandles[index];

                ScriptableImGui::SliderFloat("Height##Spot",
                    &m_spotLightHeights[index], 1.f, 30.f, "%.1f", 2.f);

                if (ScriptableImGui::SliderFloat("Cone Angle", &m_outerConeAngles[index], 0.f, 120.f))
                {
                    m_spotLightFeatureProcessor->SetConeAngles(
                        lightId,
                        m_outerConeAngles[index] * ConeAngleInnerRatio,
                        m_outerConeAngles[index]);
                }

                if (ScriptableImGui::SliderFloat("Intensity##Spot", &m_spotLightIntensities[index], 0.f, 20000.f, "%.1f", 2.f))
                {
                    AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(s_spotLightColors[index] * m_spotLightIntensities[index]);
                    m_spotLightFeatureProcessor->SetRgbIntensity(lightId, lightColor);
                    m_spotLightFeatureProcessor->SetAttenuationRadius(
                        lightId,
                        sqrtf(m_spotLightIntensities[index] / CutoffIntensity));
                }

                bool shadowmapSizeChanged =
                    ScriptableImGui::Checkbox("Enable Shadow", &m_spotLightShadowEnabled[index]);

                ImGui::Text("Shadowmap Size");
                shadowmapSizeChanged = shadowmapSizeChanged ||
                    ScriptableImGui::Combo(
                        "Size##Spot",
                        &m_spotLightImageSizeIndices[index],
                        s_shadowmapImageSizeLabels,
                        AZ_ARRAY_SIZE(s_shadowmapImageSizeLabels));
                if (shadowmapSizeChanged)
                {
                    m_spotLightFeatureProcessor->SetShadowmapSize(
                        lightId,
                        m_spotLightShadowEnabled[index] ?
                        s_shadowmapImageSizes[m_spotLightImageSizeIndices[index]] :
                        ShadowmapSize::None);

                    // Reset shadow parameters when shadow is disabled.
                    if (!m_spotLightShadowEnabled[index])
                    {
                        m_shadowFilterMethodIndicesSpot[index] = 0;
                        m_boundaryWidthsSpot[index] = 0.f;
                        m_predictionSampleCountsSpot[index] = 0;
                        m_filteringSampleCountsSpot[index] = 0;
                    }
                }

                ImGui::Spacing();

                ImGui::Text("Filtering");
                if (ScriptableImGui::Combo(
                    "Filter Method##Spot",
                    &m_shadowFilterMethodIndicesSpot[index],
                    s_shadowFilterMethodLabels,
                    AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
                {
                    m_spotLightFeatureProcessor->SetShadowFilterMethod(
                        lightId,
                        s_shadowFilterMethods[m_shadowFilterMethodIndicesSpot[index]]);
                }

                if (m_shadowFilterMethodIndicesSpot[index] != aznumeric_cast<int>(ShadowFilterMethod::None))
                {
                    ImGui::Text("Boundary Width in degrees");
                    if (ScriptableImGui::SliderFloat("Width##Spot", &m_boundaryWidthsSpot[index], 0.f, 1.0f, "%.3f"))
                    {
                        m_spotLightFeatureProcessor->SetShadowBoundaryWidthAngle(
                            lightId,
                            m_boundaryWidthsSpot[index]);
                    }
                }

                if (m_shadowFilterMethodIndicesSpot[index] == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                    m_shadowFilterMethodIndicesSpot[index] == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
                {
                    ImGui::Spacing();
                    ImGui::Text("Filtering (PCF specific)");
                    
                    if (m_pcfMethod[index] == PcfMethod::BoundarySearch && ScriptableImGui::SliderInt("Prediction # ##Spot", &m_predictionSampleCountsSpot[index], 4, 16))
                    {
                        m_spotLightFeatureProcessor->SetPredictionSampleCount(lightId, m_predictionSampleCountsSpot[index]);
                    }
                    if (ScriptableImGui::SliderInt("Filtering # ##Spot", &m_filteringSampleCountsSpot[index], 4, 64))
                    {
                        m_spotLightFeatureProcessor->SetFilteringSampleCount(lightId, m_filteringSampleCountsSpot[index]);
                    }

                    int pcfMethodAsInteger = aznumeric_cast<int>(m_pcfMethod[index]);
                    if (ScriptableImGui::RadioButton(
                            "Boundary Search filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::BoundarySearch)))
                    {
                        m_pcfMethod[index] = PcfMethod::BoundarySearch;
                        m_spotLightFeatureProcessor->SetPcfMethod(lightId, m_pcfMethod[index]);
                    }
                    if (ScriptableImGui::RadioButton(
                            "Bicubic filtering", &pcfMethodAsInteger, static_cast<int>(PcfMethod::Bicubic)))
                    {
                        m_pcfMethod[index] = PcfMethod::Bicubic;
                        m_spotLightFeatureProcessor->SetPcfMethod(lightId, m_pcfMethod[index]);
                    }

                }
            }
            ImGui::Unindent();

            ImGui::Separator();

            ImGui::Text("Camera");
            ImGui::Indent();
            {
                using namespace AZ;

                ScriptableImGui::SliderAngle("FoVY", &m_cameraFovY, 1.f, 120.f);
                ImGui::Spacing();

                Transform cameraTransform;
                TransformBus::EventResult(
                    cameraTransform,
                    GetCameraEntityId(),
                    &TransformBus::Events::GetWorldTM);
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

} // namespace AtomSampleViewer
