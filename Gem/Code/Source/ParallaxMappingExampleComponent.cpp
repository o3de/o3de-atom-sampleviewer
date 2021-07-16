/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ParallaxMappingExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Automation/ScriptableImGui.h>
#include <AzCore/Component/TransformBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    static const char* ParallaxEnableName = "parallax.useTexture";
    static const char* PdoEnableName = "parallax.pdo";
    static const char* ParallaxFactorName = "parallax.factor";
    static const char* ParallaxHeightOffsetName = "parallax.offset";
    static const char* ParallaxShowClippingName = "parallax.showClipping";
    static const char* ParallaxAlgorithmName = "parallax.algorithm";
    static const char* ParallaxQualityName = "parallax.quality";
    static const char* ParallaxUvIndexName = "parallax.textureMapUv";

    static const char* AmbientOcclusionUvIndexName = "occlusion.diffuseTextureMapUv";
    static const char* BaseColorUvIndexName = "baseColor.textureMapUv";
    static const char* NormalUvIndexName = "normal.textureMapUv";
    static const char* RoughnessUvIndexName = "roughness.textureMapUv";

    static const char* CenterUVName = "uv.center";
    static const char* TileUName = "uv.tileU";
    static const char* TileVName = "uv.tileV";
    static const char* OffsetUName = "uv.offsetU";
    static const char* OffsetVName = "uv.offsetV";
    static const char* RotationUVName = "uv.rotateDegrees";
    static const char* ScaleUVName = "uv.scale";

    // Must align with enum value in StandardPbr.materialtype
    static const char* ParallaxAlgorithmList[] =
    {
        "Basic", "Steep", "POM", "Relief", "ContactRefinement"
    };
    static const char* ParallaxQualityList[] =
    {
        "Low", "Medium", "High", "Ultra"
    };
    static const char* ParallaxUvSetList[] =
    {
        "UV0", "UV1"
    };

    void ParallaxMappingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < ParallaxMappingExampleComponent, AZ::Component>()->Version(0);
        }
    }

    ParallaxMappingExampleComponent::ParallaxMappingExampleComponent()
        : m_imguiSidebar("@user@/ParallaxMappingExampleComponent/sidebar.xml")
    {
    }

    void ParallaxMappingExampleComponent::Activate()
    {
        // Asset
        m_planeAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
        m_boxAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cube.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
        m_parallaxMaterialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("testdata/materials/parallaxrock.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
        m_defaultMaterialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("materials/defaultpbr.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
        m_parallaxMaterial = AZ::RPI::Material::Create(m_parallaxMaterialAsset);
        m_defaultMaterial = AZ::RPI::Material::Create(m_defaultMaterialAsset);

        m_planeTransform = AZ::Transform::CreateUniformScale(5);
        m_planeHandle = LoadMesh(m_planeAsset, m_parallaxMaterial, m_planeTransform);
        m_boxHandle = LoadMesh(m_boxAsset, m_defaultMaterial, AZ::Transform::CreateIdentity());

        // Material index
        m_parallaxEnableIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxEnableName));
        m_parallaxFactorIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxFactorName));
        m_parallaxOffsetIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxHeightOffsetName));
        m_parallaxShowClippingIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxShowClippingName));
        m_parallaxAlgorithmIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxAlgorithmName));
        m_parallaxQualityIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxQualityName));
        m_parallaxUvIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ParallaxUvIndexName));
        m_pdoEnableIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(PdoEnableName));

        m_ambientOcclusionUvIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(AmbientOcclusionUvIndexName));
        m_baseColorUvIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(BaseColorUvIndexName));
        m_normalUvIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(NormalUvIndexName));
        m_roughnessUvIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(RoughnessUvIndexName));

        m_centerUVIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(CenterUVName));
        m_tileUIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(TileUName));
        m_tileVIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(TileVName));
        m_offsetUIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(OffsetUName));
        m_offsetVIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(OffsetVName));
        m_rotationUVIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(RotationUVName));
        m_scaleUVIndex = m_parallaxMaterial->FindPropertyIndex(AZ::Name(ScaleUVName));

        SaveCameraConfiguration();
        // Camera
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());

        ConfigureCameraToLookDown();
        SetCameraConfiguration();

        // Light
        AZ::RPI::Scene* scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DirectionalLightFeatureProcessorInterface>();
        CreateDirectionalLight();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiskLightFeatureProcessorInterface>();
        CreateDiskLight();

        m_imguiSidebar.Activate();
        AZ::TickBus::Handler::BusConnect();
    }

    void ParallaxMappingExampleComponent::Deactivate()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_planeHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_boxHandle);

        RestoreCameraConfiguration();
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        m_diskLightFeatureProcessor->ReleaseLight(m_diskLightHandle);

        m_imguiSidebar.Deactivate();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void ParallaxMappingExampleComponent::ConfigureCameraToLookDown()
    {
        const float CameraDistance = 5.0f;
        const float CameraPitch = -0.8f;
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, CameraPitch);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, CameraDistance);
    }

    AZ::Render::MeshFeatureProcessorInterface::MeshHandle ParallaxMappingExampleComponent::LoadMesh(
        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset,
        AZ::Data::Instance<AZ::RPI::Material> material,
        AZ::Transform transform)
    {
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, material);
        GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);
        return meshHandle;
    }

    void ParallaxMappingExampleComponent::CreateDirectionalLight()
    {
        const AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle handle = m_directionalLightFeatureProcessor->AcquireLight();

        m_directionalLightFeatureProcessor->SetShadowmapSize(handle, AZ::Render::ShadowmapSize::Size2048);
        m_directionalLightFeatureProcessor->SetCascadeCount(handle, 4);
        m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(handle, 0.5f);
        m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(handle, true);
        m_directionalLightFeatureProcessor->SetShadowFilterMethod(handle, AZ::Render::ShadowFilterMethod::Esm);
        m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(handle, 0.03f);
        m_directionalLightFeatureProcessor->SetPredictionSampleCount(handle, 8);
        m_directionalLightFeatureProcessor->SetFilteringSampleCount(handle, 32);
        m_directionalLightFeatureProcessor->SetGroundHeight(handle, 0.f);

        m_directionalLightHandle = handle;
    }

    void ParallaxMappingExampleComponent::CreateDiskLight()
    {
        AZ::Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;
        const AZ::Render::DiskLightFeatureProcessorInterface::LightHandle handle = featureProcessor->AcquireLight();

        featureProcessor->SetAttenuationRadius(handle, sqrtf(500.f / CutoffIntensity));
        featureProcessor->SetConeAngles(handle, AZ::DegToRad(22.5f) * ConeAngleInnerRatio, AZ::DegToRad(22.5f));
        featureProcessor->SetShadowsEnabled(handle, true);
        featureProcessor->SetShadowmapMaxResolution(handle, AZ::Render::ShadowmapSize::Size2048);

        m_diskLightHandle = handle;
        
    }

    void ParallaxMappingExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_lightAutoRotate)
        {
            m_lightRotationAngle = fmodf(m_lightRotationAngle + deltaTime, AZ::Constants::TwoPi);
        }

        const auto location = AZ::Vector3(
            5 * sinf(m_lightRotationAngle),
            5 * cosf(m_lightRotationAngle),
            5);
        auto transform = AZ::Transform::CreateLookAt(
            location,
            AZ::Vector3::CreateZero());

        if (m_lightType)
        {
            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> directionalLightColor(AZ::Color::CreateZero());
            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> diskLightColor(AZ::Color::CreateOne() * 500.f);
            m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, directionalLightColor);
            m_diskLightFeatureProcessor->SetRgbIntensity(m_diskLightHandle, diskLightColor);
        }
        else
        {
            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> directionalLightColor(AZ::Color::CreateOne() * 5.f);
            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> diskLightColor(AZ::Color::CreateZero());
            m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, directionalLightColor);
            m_diskLightFeatureProcessor->SetRgbIntensity(m_diskLightHandle, diskLightColor);
        }

        m_diskLightFeatureProcessor->SetPosition(m_diskLightHandle, location);
        m_diskLightFeatureProcessor->SetDirection(m_diskLightHandle, transform.GetBasis(1));
        m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, transform.GetBasis(1));

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
            transform = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(
                transform,
                GetCameraEntityId(),
                &AZ::TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(
                m_directionalLightHandle, transform);
        }

        // Plane Transform
        {
            m_planeTransform.SetRotation(AZ::Quaternion::CreateRotationZ(m_planeRotationAngle));
            GetMeshFeatureProcessor()->SetTransform(m_planeHandle, m_planeTransform);
        }

        DrawSidebar();
    }

    void ParallaxMappingExampleComponent::SetCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            20.f);
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFovRadians,
            AZ::Constants::QuarterPi);
    }

    void ParallaxMappingExampleComponent::SaveCameraConfiguration()
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
    void ParallaxMappingExampleComponent::RestoreCameraConfiguration()
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

    void ParallaxMappingExampleComponent::DrawSidebar()
    {
        if (m_imguiSidebar.Begin())
        {
            bool parallaxSettingChanged = false;
            bool planeUVChanged = false;

            ImGui::Spacing();
            {
                ScriptableImGui::ScopedNameContext context{ "Lighting" };
                ImGui::Text("Lighting");
                ImGui::Indent();
                {
                    ScriptableImGui::RadioButton("Directional Light", &m_lightType, 0);
                    ScriptableImGui::RadioButton("Spot Light", &m_lightType, 1);
                    ScriptableImGui::Checkbox("Auto Rotation", &m_lightAutoRotate);
                    ScriptableImGui::SliderAngle("Direction", &m_lightRotationAngle, 0, 360);
                }
                ImGui::Unindent();
            }
            
            ImGui::Separator();

            {
                ScriptableImGui::ScopedNameContext context{ "Parallax Setting" };
                ImGui::Text("Parallax Setting");
                ImGui::Indent();
                {
                    if (ScriptableImGui::Checkbox("Enable Parallax", &m_parallaxEnable))
                    {
                        parallaxSettingChanged = true;
                        m_parallaxMaterial->SetPropertyValue(m_parallaxEnableIndex, m_parallaxEnable);
                    }

                    if (m_parallaxEnable)
                    {
                        if (ScriptableImGui::Checkbox("Enable Pdo", &m_pdoEnable))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_pdoEnableIndex, m_pdoEnable);
                        }

                        if (ScriptableImGui::SliderFloat("Heightmap Scale", &m_parallaxFactor, 0.0f, 0.1f))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxFactorIndex, m_parallaxFactor);
                        }
                        
                        if (ScriptableImGui::SliderFloat("Offset", &m_parallaxOffset, -0.1f, 0.1f))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxOffsetIndex, m_parallaxOffset);
                        }
                        
                        if (ScriptableImGui::Checkbox("Show Clipping", &m_parallaxShowClipping))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxShowClippingIndex, m_parallaxShowClipping);
                        }

                        if (ScriptableImGui::Combo("Algorithm", &m_parallaxAlgorithm, ParallaxAlgorithmList, AZ_ARRAY_SIZE(ParallaxAlgorithmList)))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxAlgorithmIndex, static_cast<uint32_t>(m_parallaxAlgorithm));
                        }

                        if (ScriptableImGui::Combo("Quality", &m_parallaxQuality, ParallaxQualityList, AZ_ARRAY_SIZE(ParallaxQualityList)))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxQualityIndex, static_cast<uint32_t>(m_parallaxQuality));
                        }

                        if (ScriptableImGui::Combo("UV", &m_parallaxUv, ParallaxUvSetList, AZ_ARRAY_SIZE(ParallaxUvSetList)))
                        {
                            parallaxSettingChanged = true;
                            m_parallaxMaterial->SetPropertyValue(m_parallaxUvIndex, static_cast<uint32_t>(m_parallaxUv));
                            m_parallaxMaterial->SetPropertyValue(m_ambientOcclusionUvIndex, static_cast<uint32_t>(m_parallaxUv));
                            m_parallaxMaterial->SetPropertyValue(m_baseColorUvIndex, static_cast<uint32_t>(m_parallaxUv));
                            m_parallaxMaterial->SetPropertyValue(m_normalUvIndex, static_cast<uint32_t>(m_parallaxUv));
                            m_parallaxMaterial->SetPropertyValue(m_roughnessUvIndex, static_cast<uint32_t>(m_parallaxUv));
                        }
                    }
                }
                ImGui::Unindent();
            }

            ImGui::Separator();

            {
                ScriptableImGui::ScopedNameContext context{ "Plane Setting" };
                ImGui::Text("Plane Setting");
                ImGui::Indent();
                {
                    ScriptableImGui::SliderAngle("Rotation", &m_planeRotationAngle, 0, 360);

                    bool centerUChanged = false;
                    bool centerVChanged = false;
                    bool tileUChanged = false;
                    bool tileVChanged = false;
                    bool offsetUChanged = false;
                    bool offsetVChanged = false;
                    bool rotationUVChanged = false;
                    bool scaleChanged = false;

                    centerUChanged = ScriptableImGui::SliderFloat("Center U", &m_planeCenterU, -1.f, 1.f);
                    centerVChanged = ScriptableImGui::SliderFloat("Center V", &m_planeCenterV, -1.f, 1.f);
                    if (centerUChanged || centerVChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_centerUVIndex, AZ::Vector2(m_planeCenterU, m_planeCenterV));
                    }
                    
                    tileUChanged = ScriptableImGui::SliderFloat("Tile U", &m_planeTileU, 0.f, 2.f);
                    if (tileUChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_tileUIndex, m_planeTileU);
                    }

                    tileVChanged = ScriptableImGui::SliderFloat("Tile V", &m_planeTileV, 0.f, 2.f);
                    if (tileVChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_tileVIndex, m_planeTileV);
                    }

                    offsetUChanged = ScriptableImGui::SliderFloat("Offset U", &m_planeOffsetU, -1.f, 1.f);
                    if (offsetUChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_offsetUIndex, m_planeOffsetU);
                    }

                    offsetVChanged = ScriptableImGui::SliderFloat("Offset V", &m_planeOffsetV, -1.f, 1.f);
                    if (offsetVChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_offsetVIndex, m_planeOffsetV);
                    }

                    rotationUVChanged = ScriptableImGui::SliderFloat("Rotation UV", &m_planeRotateUV, -180.f, 180.f);
                    if (rotationUVChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_rotationUVIndex, m_planeRotateUV);
                    }

                    scaleChanged = ScriptableImGui::SliderFloat("Scale UV", &m_planeScaleUV, 0.f, 2.f);
                    if (scaleChanged)
                    {
                        m_parallaxMaterial->SetPropertyValue(m_scaleUVIndex, m_planeScaleUV);
                    }

                    planeUVChanged = centerUChanged || centerVChanged || tileUChanged || tileVChanged || offsetUChanged || offsetVChanged || rotationUVChanged || scaleChanged;
                }
                ImGui::Unindent();
            }

            m_imguiSidebar.End();

            if (parallaxSettingChanged || planeUVChanged)
            {
                m_parallaxMaterial->Compile();
            }
        }
    }

}
