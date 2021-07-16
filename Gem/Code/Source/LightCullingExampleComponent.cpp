/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <LightCullingExampleComponent.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <imgui/imgui.h>

#include <Atom/Feature/CoreLights/PhotometricValue.h>

#include <Atom/Feature/CoreLights/CoreLightsConstants.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Obb.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzFramework/Components/CameraBus.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Pass/FullscreenTrianglePass.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace AZ::Render;
    using namespace AZ::RPI;

    static const char* WorldModelName = "Objects/Sponza.azmodel";

    static const char* TransparentModelName = "Objects/ShaderBall_simple.azmodel";
    static const char* TransparentMaterialName = "materials/DefaultPBRTransparent.azmaterial";

    static const char* DecalMaterialPath = "materials/Decal/airship_tail_01_decal.azmaterial";

    static const float TimingSmoothingFactor = 0.9f;
    static const size_t MaxNumLights = 1024;
    static const float AuxGeomDebugAlpha = 0.5f;
    static const AZ::Vector3 CameraStartPosition = AZ::Vector3(-12.f, -35.5f, 0.7438f);

    AZ::Color LightCullingExampleComponent::GetRandomColor()
    {
        static const AZStd::vector<AZ::Color> colors = {
            AZ::Colors::Red,
            AZ::Colors::Blue,
            AZ::Colors::Green,
            AZ::Colors::White,
            AZ::Colors::Purple,
            AZ::Colors::MediumAquamarine,
            AZ::Colors::Fuchsia,
            AZ::Colors::Thistle,
            AZ::Colors::LightGoldenrodYellow,
            AZ::Colors::BlanchedAlmond,
            AZ::Colors::PapayaWhip,
            AZ::Colors::Bisque,
            AZ::Colors::Chocolate,
            AZ::Colors::MintCream,
            AZ::Colors::LemonChiffon,
            AZ::Colors::Plum
        };

        int randomNumber = m_random.GetRandom() % colors.size();
        AZ::Color color = colors[randomNumber];
        color.SetA(AuxGeomDebugAlpha);
        return color;
    }
    static AZ::Render::MaterialAssignmentMap CreateMaterialAssignmentMap(const char* materialPath)
    {
        Render::MaterialAssignmentMap materials;
        Render::MaterialAssignment& defaultMaterialAssignment = materials[AZ::Render::DefaultMaterialAssignmentId];
        defaultMaterialAssignment.m_materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(materialPath, RPI::AssetUtils::TraceLevel::Assert);
        defaultMaterialAssignment.m_materialInstance = RPI::Material::FindOrCreate(defaultMaterialAssignment.m_materialAsset);
        return materials;
    }

    LightCullingExampleComponent::LightCullingExampleComponent()
    {
        m_sampleName = "LightCullingExampleComponent";

        // Add some initial lights to illuminate the scene
        m_settings[(int)LightType::Point].m_numActive = 150;
        m_settings[(int)LightType::Disk].m_intensity = 40.0f;
        m_settings[(int)LightType::Capsule].m_intensity = 10.0f;
    }

    void LightCullingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<LightCullingExampleComponent, AZ::Component>()
                ->Version(0)
            ;
        }
    }

    void LightCullingExampleComponent::Activate()
    {
        GetFeatureProcessors();

        // Don't continue the script until after the models have loaded and lights have been created. 
        // Use a large timeout because of how slow this level loads in debug mode.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);

        // preload assets
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {WorldModelName, azrtti_typeid<RPI::ModelAsset>()},
            {TransparentModelName, azrtti_typeid<RPI::ModelAsset>()},
            {TransparentMaterialName, azrtti_typeid<RPI::MaterialAsset>()},
            {DecalMaterialPath, azrtti_typeid<RPI::MaterialAsset>()}
        };

        PreloadAssets(assetList);
    }

    void LightCullingExampleComponent::OnAllAssetsReadyActivate()
    {
        LoadDecalMaterial();
        SetupScene();
        SetupCamera();

        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();

        // Now that the model and all the lights are initialized, we can allow the script to continue.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void LightCullingExampleComponent::SetupCamera()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
        SaveCameraConfiguration();

        const float FarClipDistance = 16384.0f;

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            FarClipDistance);

        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 200.0f);
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, Vector3(5.0f, 0.0f, 5.0f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(90.0f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, AZ::DegToRad(-11.936623));
    }

    void LightCullingExampleComponent::Deactivate()
    {
        m_decalMaterial = {};
        DisableHeatmap();

        AZ::TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        RestoreCameraConfiguration();
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        DestroyOpaqueModels();
        DestroyTransparentModels();

        DestroyLightsAndDecals();
    }

    void LightCullingExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (m_worldModelAssetLoaded)
        {
            CalculateSmoothedFPS(deltaTime);
            DrawSidebar();
            DrawDebuggingHelpers();
            UpdateLights();
        }
    }

    void LightCullingExampleComponent::UpdateLights()
    {
        if (m_refreshLights)
        {
            DestroyLightsAndDecals();

            CreateLightsAndDecals();


            m_refreshLights = false;
        }
    }


    void LightCullingExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        m_meshChangedHandler.Disconnect();
        m_worldModelAssetLoaded = true;
        m_worldModelAABB = model->GetModelAsset()->GetAabb();

        InitLightArrays();
        CreateLightsAndDecals();
        MoveCameraToStartPosition();
    }

    void LightCullingExampleComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(
            m_originalFarClipDistance,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void LightCullingExampleComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_originalFarClipDistance);
    }

    void LightCullingExampleComponent::SetupScene()
    {
        using namespace AZ;
        CreateTransparentModels();
        CreateOpaqueModels();
    }

    AZ::Vector3 LightCullingExampleComponent::GetRandomPositionInsideWorldModel()
    {
        AZ::Vector3 randomPosition;

        float x = m_random.GetRandomFloat() * m_worldModelAABB.GetXExtent();
        float y = m_random.GetRandomFloat() * m_worldModelAABB.GetYExtent();
        float z = m_random.GetRandomFloat() * m_worldModelAABB.GetZExtent();

        randomPosition.SetX(x);
        randomPosition.SetY(y);
        randomPosition.SetZ(z);

        randomPosition += m_worldModelAABB.GetMin();
        return randomPosition;
    }

    AZ::Vector3 LightCullingExampleComponent::GetRandomDirection()
    {
        float x = m_random.GetRandomFloat() - 0.5f;
        float y = m_random.GetRandomFloat() - 0.5f;
        float z = m_random.GetRandomFloat() - 0.5f;
        AZ::Vector3 direction(x, y, z);
        direction.NormalizeSafe();
        return direction;
    }

    float LightCullingExampleComponent::GetRandomNumber(float low, float high)
    {
        float r = m_random.GetRandomFloat();
        return r * (high - low) + low;
    }

    void LightCullingExampleComponent::CreatePointLights()
    {
        for (int i = 0; i < m_settings[(int)LightType::Point].m_numActive; ++i)
        {
            CreatePointLight(i);
        }
    }

    void LightCullingExampleComponent::CreateDiskLights()
    {
        for (int i = 0; i < m_settings[(int)LightType::Disk].m_numActive; ++i)
        {
            CreateDiskLight(i);
        }
    }

    void LightCullingExampleComponent::CreateCapsuleLights()
    {
        for (int i = 0; i < m_settings[(int)LightType::Capsule].m_numActive; ++i)
        {
            CreateCapsuleLight(i);
        }
    }

    void LightCullingExampleComponent::CreateQuadLights()
    {
        for (int i = 0; i < m_settings[(int)LightType::Quad].m_numActive; ++i)
        {
            CreateQuadLight(i);
        }
    }

    void LightCullingExampleComponent::CreateDecals()
    {
        for (int i = 0; i < m_settings[(int)LightType::Decal].m_numActive; ++i)
        {
            CreateDecal(i);
        }
    }


    void LightCullingExampleComponent::DestroyDecals()
    {
        for (size_t i = 0; i < m_decals.size(); ++i)
        {
            m_decalFeatureProcessor->ReleaseDecal(m_decals[i].m_decalHandle);
            m_decals[i].m_decalHandle = DecalHandle::Null;
        }
    }

    void LightCullingExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        if (!m_worldModelAssetLoaded)
        {
            const ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Asset", nullptr, windowFlags))
            {
                ImGui::Text("World Model: %s", m_worldModelAssetLoaded ? "Loaded" : "Loading...");

                ImGui::End();
            }
        }

        DrawSidebarTimingSection();

        ImGui::Spacing();
        ImGui::Separator();

        DrawSidebarPointLightsSection(&m_settings[(int)LightType::Point]);
        DrawSidebarDiskLightsSection(&m_settings[(int)LightType::Disk]);
        DrawSidebarCapsuleLightSection(&m_settings[(int)LightType::Capsule]);
        DrawSidebarQuadLightsSections(&m_settings[(int)LightType::Quad]);
        DrawSidebarDecalSection(&m_settings[(int)LightType::Decal]);
        DrawSidebarHeatmapOpacity();

        m_imguiSidebar.End();
    }

    void LightCullingExampleComponent::DrawSidebarPointLightsSection(LightSettings* lightSettings)
    {
        ScriptableImGui::ScopedNameContext context{ "Point Lights" };
        if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            m_refreshLights |= ScriptableImGui::SliderInt("Point light count", &lightSettings->m_numActive, 0, MaxNumLights);
            m_refreshLights |= ScriptableImGui::SliderFloat("Bulb Radius", &m_bulbRadius, 0.0f, 20.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Point Intensity", &lightSettings->m_intensity, 0.0f, 200.0f);
            m_refreshLights |= ScriptableImGui::Checkbox("Enable automatic light falloff (Point)", &lightSettings->m_enableAutomaticFalloff);
            m_refreshLights |= ScriptableImGui::SliderFloat("Point Attenuation Radius", &lightSettings->m_attenuationRadius, 0.0f, 20.0f);
            ScriptableImGui::Checkbox("Draw Debug Spheres", &lightSettings->m_enableDebugDraws);
        }
    }

    void LightCullingExampleComponent::DrawSidebarDiskLightsSection(LightSettings* lightSettings)
    {
        ScriptableImGui::ScopedNameContext context{"Disk Lights"};
        if (ImGui::CollapsingHeader("Disk Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            m_refreshLights |= ScriptableImGui::SliderInt("Disk light count", &lightSettings->m_numActive, 0, MaxNumLights);
            m_refreshLights |= ScriptableImGui::SliderFloat("Disk Radius", &m_diskRadius, 0.0f, 20.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Disk Attenuation Radius", &lightSettings->m_attenuationRadius, 0.0f, 20.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Disk Intensity", &lightSettings->m_intensity, 0.0f, 200.0f);
            m_refreshLights |= ScriptableImGui::Checkbox("Enable Disk Cone", &m_diskConesEnabled);

            if (m_diskConesEnabled)
            {
                m_refreshLights |= ScriptableImGui::SliderFloat("Inner Cone (degrees)", &m_diskInnerConeDegrees, 0.0f, 180.0f);
                m_refreshLights |= ScriptableImGui::SliderFloat("Outer Cone (degrees)", &m_diskOuterConeDegrees, 0.0f, 180.0f);
                ScriptableImGui::Checkbox("Draw Debug Cones", &lightSettings->m_enableDebugDraws);
            }
            else
            {
                ScriptableImGui::Checkbox("Draw disk lights", &lightSettings->m_enableDebugDraws);
            }
        }
    }

    void LightCullingExampleComponent::DrawSidebarCapsuleLightSection(LightSettings* lightSettings)
    {
        ScriptableImGui::ScopedNameContext context{"Capsule Lights"};
        if (ImGui::CollapsingHeader("Capsule Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            m_refreshLights |= ScriptableImGui::SliderInt("Capsule light count", &lightSettings->m_numActive, 0, MaxNumLights);
            m_refreshLights |= ScriptableImGui::SliderFloat("Capsule Intensity", &lightSettings->m_intensity, 0.0f, 200.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Capsule Radius", &m_capsuleRadius, 0.0f, 5.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Capsule Length", &m_capsuleLength, 0.0f, 20.0f);
            ScriptableImGui::Checkbox("Draw capsule lights", &lightSettings->m_enableDebugDraws);
        }
    }

    void LightCullingExampleComponent::DrawSidebarQuadLightsSections(LightSettings* lightSettings)
    {
        ScriptableImGui::ScopedNameContext context{ "Quad Lights" };
        if (ImGui::CollapsingHeader("Quad Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            m_refreshLights |= ScriptableImGui::SliderInt("Quad light count", &lightSettings->m_numActive, 0, MaxNumLights);
            m_refreshLights |= ScriptableImGui::SliderFloat("Quad Attenuation Radius", &lightSettings->m_attenuationRadius, 0.0f, 20.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Quad light width", &m_quadLightSize[0], 0.0f, 10.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Quad light height", &m_quadLightSize[1], 0.0f, 10.0f);
            m_refreshLights |= ScriptableImGui::Checkbox("Double sided quad", &m_isQuadLightDoubleSided);
            m_refreshLights |= ScriptableImGui::Checkbox("Use fast approximation", &m_quadLightsUseFastApproximation);
            ScriptableImGui::Checkbox("Draw quad lights", &lightSettings->m_enableDebugDraws);
        }
    }

    void LightCullingExampleComponent::DrawSidebarHeatmapOpacity()
    {
        ScriptableImGui::ScopedNameContext context{"Heatmap"};
        ImGui::Text("Heatmap");
        ImGui::Indent();
        bool opacityChanged = ScriptableImGui::SliderFloat("Opacity", &m_heatmapOpacity, 0, 1);
        ImGui::Unindent();

        if (opacityChanged)
        {
            UpdateHeatmapOpacity();
        }
    }

    void LightCullingExampleComponent::DrawSidebarDecalSection(LightSettings* lightSettings)
    {
        ScriptableImGui::ScopedNameContext context{"Decals"};
        if (ImGui::CollapsingHeader("Decals", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            m_refreshLights |= ScriptableImGui::SliderInt("Decal count", &lightSettings->m_numActive, 0, MaxNumLights);
            ScriptableImGui::Checkbox("Draw decals", &lightSettings->m_enableDebugDraws);
            m_refreshLights |= ScriptableImGui::SliderFloat3("Decal Size", m_decalSize.data(), 0.0f, 10.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Decal Opacity", &m_decalOpacity, 0.0f, 1.0f);
            m_refreshLights |= ScriptableImGui::SliderFloat("Decal Angle Attenuation", &m_decalAngleAttenuation, 0.0f, 1.0f);
        }
    }

    void LightCullingExampleComponent::CreatePointLight(int index)
    {
        auto& light = m_pointLights[index];
        AZ_Assert(light.m_lightHandle.IsNull(), "CreatePointLight called on a light that was already created previously");

        light.m_lightHandle = m_pointLightFeatureProcessor->AcquireLight();

        const LightSettings& settings = m_settings[(int)LightType::Point];

        m_pointLightFeatureProcessor->SetPosition(light.m_lightHandle, light.m_position);
        m_pointLightFeatureProcessor->SetRgbIntensity(light.m_lightHandle, PhotometricColor<PhotometricUnit::Candela>(settings.m_intensity * light.m_color));
        m_pointLightFeatureProcessor->SetBulbRadius(light.m_lightHandle, m_bulbRadius);

        float attenuationRadius = settings.m_enableAutomaticFalloff ? AutoCalculateAttenuationRadius(light.m_color, settings.m_intensity) : settings.m_attenuationRadius;
        m_pointLightFeatureProcessor->SetAttenuationRadius(light.m_lightHandle, attenuationRadius);
    }

    void LightCullingExampleComponent::CreateDiskLight(int index)
    {
        auto& light = m_diskLights[index];
        light.m_lightHandle = m_diskLightFeatureProcessor->AcquireLight();

        const LightSettings& settings = m_settings[(int)LightType::Disk];

        m_diskLightFeatureProcessor->SetDiskRadius(light.m_lightHandle, m_diskRadius);
        m_diskLightFeatureProcessor->SetPosition(light.m_lightHandle, light.m_position);
        m_diskLightFeatureProcessor->SetRgbIntensity(light.m_lightHandle, PhotometricColor<PhotometricUnit::Candela>(settings.m_intensity * light.m_color));
        m_diskLightFeatureProcessor->SetDirection(light.m_lightHandle, light.m_direction);
        
        m_diskLightFeatureProcessor->SetConstrainToConeLight(light.m_lightHandle, m_diskConesEnabled);
        if (m_diskConesEnabled)
        {
            m_diskLightFeatureProcessor->SetConeAngles(light.m_lightHandle, DegToRad(m_diskInnerConeDegrees), DegToRad(m_diskOuterConeDegrees));
        }

        m_diskLightFeatureProcessor->SetAttenuationRadius(light.m_lightHandle, m_settings[(int)LightType::Disk].m_attenuationRadius);
    }

    void LightCullingExampleComponent::CreateCapsuleLight(int index)
    {
        auto& light = m_capsuleLights[index];
        AZ_Assert(light.m_lightHandle.IsNull(), "CreateCapsuleLight called on a light that was already created previously");
        light.m_lightHandle = m_capsuleLightFeatureProcessor->AcquireLight();

        const LightSettings& settings = m_settings[(int)LightType::Capsule];

        m_capsuleLightFeatureProcessor->SetAttenuationRadius(light.m_lightHandle, m_settings[(int)LightType::Capsule].m_attenuationRadius);
        m_capsuleLightFeatureProcessor->SetRgbIntensity(light.m_lightHandle, PhotometricColor<PhotometricUnit::Candela>(settings.m_intensity * light.m_color));
        m_capsuleLightFeatureProcessor->SetCapsuleRadius(light.m_lightHandle, m_capsuleRadius);

        AZ::Vector3 startPoint = light.m_position - light.m_direction * m_capsuleLength * 0.5f;
        AZ::Vector3 endPoint = light.m_position + light.m_direction * m_capsuleLength * 0.5f;
        m_capsuleLightFeatureProcessor->SetCapsuleLineSegment(light.m_lightHandle, startPoint, endPoint);
    }

    void LightCullingExampleComponent::CreateQuadLight(int index)
    {
        auto& light = m_quadLights[index];
        AZ_Assert(light.m_lightHandle.IsNull(), "CreateQuadLight called on a light that was already created previously");
        light.m_lightHandle = m_quadLightFeatureProcessor->AcquireLight();

        const LightSettings& settings = m_settings[(int)LightType::Quad];

        m_quadLightFeatureProcessor->SetRgbIntensity(light.m_lightHandle, PhotometricColor<PhotometricUnit::Nit>(settings.m_intensity * light.m_color));

        const auto orientation = AZ::Quaternion::CreateFromVector3(light.m_direction);
        m_quadLightFeatureProcessor->SetOrientation(light.m_lightHandle, orientation);
        m_quadLightFeatureProcessor->SetQuadDimensions(light.m_lightHandle, m_quadLightSize[0], m_quadLightSize[1]);
        m_quadLightFeatureProcessor->SetLightEmitsBothDirections(light.m_lightHandle, m_isQuadLightDoubleSided);
        m_quadLightFeatureProcessor->SetUseFastApproximation(light.m_lightHandle, m_quadLightsUseFastApproximation);
        m_quadLightFeatureProcessor->SetAttenuationRadius(light.m_lightHandle, m_settings[(int)LightType::Quad].m_attenuationRadius);
        m_quadLightFeatureProcessor->SetPosition(light.m_lightHandle, light.m_position);
    }

    void LightCullingExampleComponent::CreateDecal(int index)
    {
        Decal& decal = m_decals[index];
        decal.m_decalHandle = m_decalFeatureProcessor->AcquireDecal();

        AZ::Render::DecalData decalData;
        decalData.m_position = {
            { decal.m_position.GetX(), decal.m_position.GetY(), decal.m_position.GetZ() }
        };
        decalData.m_halfSize = {
            {m_decalSize[0] * 0.5f, m_decalSize[1] * 0.5f, m_decalSize[2] * 0.5f}
        };
        decalData.m_quaternion = {
            { decal.m_quaternion.GetX(), decal.m_quaternion.GetY(), decal.m_quaternion.GetZ(), decal.m_quaternion.GetW() }
        };
        decalData.m_angleAttenuation = m_decalAngleAttenuation;
        decalData.m_opacity = m_decalOpacity;

        m_decalFeatureProcessor->SetDecalData(decal.m_decalHandle, decalData);

        m_decalFeatureProcessor->SetDecalMaterial(decal.m_decalHandle, m_decalMaterial.GetId());

    }

    void LightCullingExampleComponent::DrawPointLightDebugSpheres(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        const LightSettings& settings = m_settings[(int)LightType::Point];
        int numToDraw = AZStd::min(m_settings[(int)LightType::Point].m_numActive, aznumeric_cast<int>(m_pointLights.size()));
        for (int i = 0; i < numToDraw; ++i)
        {
            const auto& light = m_pointLights[i];
            if (light.m_lightHandle.IsNull())
            {
                continue;
            }

            float radius = settings.m_enableAutomaticFalloff ? AutoCalculateAttenuationRadius(light.m_color, settings.m_intensity) : settings.m_attenuationRadius;
            auxGeom->DrawSphere(light.m_position, radius, light.m_color, AZ::RPI::AuxGeomDraw::DrawStyle::Shaded);
        }
    }

    void LightCullingExampleComponent::DrawDecalDebugBoxes(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        int numToDraw = AZStd::min(m_settings[(int)LightType::Decal].m_numActive, aznumeric_cast<int>(m_decals.size()));
        for (int i = 0; i < numToDraw; ++i)
        {
            const Decal& decal = m_decals[i];
            if (decal.m_decalHandle.IsValid())
            {
                AZ::Aabb aabb = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3::CreateZero(), AZ::Vector3::CreateFromFloat3(m_decalSize.data()) * 0.5f);
                AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromQuaternionAndTranslation(decal.m_quaternion, decal.m_position);
                auxGeom->DrawObb(AZ::Obb::CreateFromAabb(aabb), transform, AZ::Colors::White, AZ::RPI::AuxGeomDraw::DrawStyle::Line);
            }
        }
    }

    void LightCullingExampleComponent::DrawDiskLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        int numToDraw = AZStd::min(m_settings[(int)LightType::Disk].m_numActive, aznumeric_cast<int>(m_diskLights.size()));
        for (int i = 0; i < numToDraw; ++i)
        {
            const auto& light = m_diskLights[i];
            if (light.m_lightHandle.IsValid())
            {
                auxGeom->DrawDisk(light.m_position, light.m_direction, m_diskRadius, light.m_color, AZ::RPI::AuxGeomDraw::DrawStyle::Shaded);
            }
        }
    }

    void LightCullingExampleComponent::DrawCapsuleLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        int numToDraw = AZStd::min(m_settings[(int)LightType::Capsule].m_numActive, aznumeric_cast<int>(m_capsuleLights.size()));
        for (int i = 0; i < numToDraw; ++i)
        {
            const auto& light = m_capsuleLights[i];
            if (light.m_lightHandle.IsValid())
            {
                auxGeom->DrawCylinder(light.m_position, light.m_direction, m_capsuleRadius, m_capsuleLength, light.m_color);
            }
        }
    }

    void LightCullingExampleComponent::DrawQuadLightDebugObjects(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        int numToDraw = AZStd::min(m_settings[(int)LightType::Quad].m_numActive, aznumeric_cast<int>(m_quadLights.size()));
        for (int i = 0; i < numToDraw; ++i)
        {
            const auto& light = m_quadLights[i];
            if (light.m_lightHandle.IsValid())
            {
                auto transform = AZ::Transform::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateFromVector3(light.m_direction), light.m_position);

                // Rotate 90 degrees so that the debug draw is aligned properly with the quad light
                transform *= AZ::Transform::CreateFromQuaternion(AZ::ConvertEulerRadiansToQuaternion(AZ::Vector3(AZ::Constants::HalfPi, 0.0f, 0.0f)));
                auxGeom->DrawQuad(m_quadLightSize[0], m_quadLightSize[1], AZ::Matrix3x4::CreateFromTransform(transform), light.m_color);
            }
        }
    }

    void LightCullingExampleComponent::DrawDebuggingHelpers()
    {
        auto defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        if (auto auxGeom = AZ::RPI::AuxGeomFeatureProcessorInterface::GetDrawQueueForScene(defaultScene))
        {
            if (m_settings[(int)LightType::Point].m_enableDebugDraws)
            {
                DrawPointLightDebugSpheres(auxGeom);
            }
            if (m_settings[(int)LightType::Disk].m_enableDebugDraws)
            {
                DrawDiskLightDebugObjects(auxGeom);
            }
            if (m_settings[(int)LightType::Capsule].m_enableDebugDraws)
            {
                DrawCapsuleLightDebugObjects(auxGeom);
            }
            if (m_settings[(int)LightType::Quad].m_enableDebugDraws)
            {
                DrawQuadLightDebugObjects(auxGeom);
            }
            if (m_settings[(int)LightType::Decal].m_enableDebugDraws)
            {
                DrawDecalDebugBoxes(auxGeom);
            }
        }
    }

    void LightCullingExampleComponent::DrawSidebarTimingSection()
    {
        ImGui::Text("Timing");
        DrawSidebarTimingSectionCPU();
    }

    void LightCullingExampleComponent::CalculateSmoothedFPS(float deltaTimeSeconds)
    {
        float currFPS = 1.0f / deltaTimeSeconds;
        m_smoothedFPS = TimingSmoothingFactor * m_smoothedFPS + (1.0f - TimingSmoothingFactor) * currFPS;
    }

    void LightCullingExampleComponent::DrawSidebarTimingSectionCPU()
    {
        ImGui::Text("CPU (ms)");
        ImGui::Indent();
        ImGui::Text("Total: %5.1f", 1000.0f / m_smoothedFPS);
        ImGui::Unindent();
    }

    void LightCullingExampleComponent::InitLightArrays()
    {
        const auto InitLight = [this](auto& light)
            {
                light.m_color = GetRandomColor();
                light.m_position = GetRandomPositionInsideWorldModel();
                light.m_direction = GetRandomDirection();
            };
        
        // Set seed to a specific value for each light type so values are consistent between multiple app runs
        // And changes to one type don't polute the random numbers for another type.
        // Intended for use with the screenshot comparison tool
        m_random.SetSeed(0);
        m_pointLights.resize(MaxNumLights);
        AZStd::for_each(m_pointLights.begin(), m_pointLights.end(), InitLight);
        
        m_random.SetSeed(1);
        m_diskLights.resize(MaxNumLights);
        AZStd::for_each(m_diskLights.begin(), m_diskLights.end(), InitLight);
        
        m_random.SetSeed(2);
        m_capsuleLights.resize(MaxNumLights);
        AZStd::for_each(m_capsuleLights.begin(), m_capsuleLights.end(), InitLight);

        m_random.SetSeed(3);
        m_decals.resize(MaxNumLights);
        AZStd::for_each(m_decals.begin(), m_decals.end(), [&](Decal& decal)
            {
                decal.m_position = GetRandomPositionInsideWorldModel();
                decal.m_quaternion = AZ::Quaternion::CreateFromAxisAngle(GetRandomDirection(), GetRandomNumber(0.0f, AZ::Constants::TwoPi));
            });
        
        m_random.SetSeed(4);
        m_quadLights.resize(MaxNumLights);
        AZStd::for_each(m_quadLights.begin(), m_quadLights.end(), InitLight);
    }

    void LightCullingExampleComponent::GetFeatureProcessors()
    {
        AZ::RPI::Scene* scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiskLightFeatureProcessorInterface>();
        m_capsuleLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::CapsuleLightFeatureProcessorInterface>();
        m_quadLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::QuadLightFeatureProcessorInterface>();
        m_decalFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DecalFeatureProcessorInterface>();
    }

    float LightCullingExampleComponent::AutoCalculateAttenuationRadius(const AZ::Color& color, float intensity)
    {
        // Get combined intensity luma from m_photometricValue, then calculate the radius at which the irradiance will be equal to cutoffIntensity.
        static const float CutoffIntensity = 0.1f; // Make this configurable later.

        float luminance = AZ::Render::PhotometricValue::GetPerceptualLuminance(color * intensity);
        return sqrt(luminance / CutoffIntensity);
    }

    void LightCullingExampleComponent::MoveCameraToStartPosition()
    {
        const AZ::Vector3 target = AZ::Vector3::CreateAxisZ();
        const AZ::Transform transform = AZ::Transform::CreateLookAt(CameraStartPosition, target, AZ::Transform::Axis::YPositive);
        AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTM, transform);
    }

    void LightCullingExampleComponent::UpdateHeatmapOpacity()
    {
        if (const ScenePtr scene = RPISystemInterface::Get()->GetDefaultScene())
        {
            if (const RenderPipelinePtr pipeline = scene->GetDefaultRenderPipeline())
            {
                if (const Ptr<Pass> pass = pipeline->GetRootPass()->FindPassByNameRecursive(AZ::Name("LightCullingHeatmapPass")))
                {
                    if (const Ptr<RenderPass> trianglePass = azrtti_cast<RenderPass*>(pass))
                    {
                        trianglePass->SetEnabled(m_heatmapOpacity > 0.0f);
                        Data::Instance<ShaderResourceGroup> srg = trianglePass->GetShaderResourceGroup();
                        RHI::ShaderInputConstantIndex opacityIndex = srg->FindShaderInputConstantIndex(AZ::Name("m_heatmapOpacity"));
                        [[maybe_unused]] bool setOk = srg->SetConstant<float>(opacityIndex, m_heatmapOpacity);
                        AZ_Warning("LightCullingExampleComponent", setOk, "Unable to set heatmap opacity");
                    }
                }
            }
        }
    }

    void LightCullingExampleComponent::DisableHeatmap()
    {
        m_heatmapOpacity = 0.0f;
        UpdateHeatmapOpacity();
    }

    void LightCullingExampleComponent::CreateOpaqueModels()
    {
        Data::Asset<RPI::ModelAsset> modelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(WorldModelName, RPI::AssetUtils::TraceLevel::Assert);

        auto meshFeatureProcessor = GetMeshFeatureProcessor();

        m_meshHandle = meshFeatureProcessor->AcquireMesh(MeshHandleDescriptor{ modelAsset });
        meshFeatureProcessor->SetTransform(m_meshHandle, Transform::CreateIdentity());
        Data::Instance<RPI::Model> model = meshFeatureProcessor->GetModel(m_meshHandle);
        // Loading in the world will probably take a while and I want to grab the AABB afterwards, so hook it up to a a ModelChangeEventHandler
        if (model)
        {
            OnModelReady(model);
        }
        else
        {
            meshFeatureProcessor->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
        }
    }

    void LightCullingExampleComponent::CreateTransparentModels()
    {
        const AZStd::vector<AZ::Vector3 > TransparentModelPositions = { AZ::Vector3(-6.f, -20, 1),
                                                                        AZ::Vector3(7.5f, 0, 1)
        };

        Data::Asset<RPI::ModelAsset> transparentModelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(TransparentModelName, RPI::AssetUtils::TraceLevel::Assert);

        // Override the shader ball material with a transparent material
        Render::MaterialAssignmentMap materialAssignmentMap = CreateMaterialAssignmentMap(TransparentMaterialName);
        for (const AZ::Vector3& position : TransparentModelPositions)
        {
            AZ::Render::MeshFeatureProcessorInterface::MeshHandle meshHandle = GetMeshFeatureProcessor()->AcquireMesh(MeshHandleDescriptor{ transparentModelAsset }, materialAssignmentMap);
            GetMeshFeatureProcessor()->SetTransform(meshHandle, Transform::CreateTranslation(position));
            m_transparentMeshHandles.push_back(std::move(meshHandle));
        }
    }

    void LightCullingExampleComponent::DestroyOpaqueModels()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
    }

    void LightCullingExampleComponent::DestroyTransparentModels()
    {
        for (auto& elem : m_transparentMeshHandles)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(elem);
        }
        m_transparentMeshHandles.clear();
    }

    void LightCullingExampleComponent::DestroyLightsAndDecals()
    {
        DestroyLights(m_pointLightFeatureProcessor, m_pointLights);
        DestroyLights(m_diskLightFeatureProcessor, m_diskLights);
        DestroyLights(m_capsuleLightFeatureProcessor, m_capsuleLights);
        DestroyLights(m_quadLightFeatureProcessor, m_quadLights);
        DestroyDecals();
    }

    void LightCullingExampleComponent::CreateLightsAndDecals()
    {
        CreatePointLights();
        CreateDiskLights();
        CreateCapsuleLights();
        CreateQuadLights();
        CreateDecals();
    }

    void LightCullingExampleComponent::LoadDecalMaterial()
    {
        const AZ::Data::AssetId id = AZ::RPI::AssetUtils::GetAssetIdForProductPath(DecalMaterialPath);

        m_decalMaterial = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::MaterialAsset>(
            id, AZ::Data::AssetLoadBehavior::PreLoad);
        m_decalMaterial.BlockUntilLoadComplete();
    }

} // namespace AtomSampleViewer
