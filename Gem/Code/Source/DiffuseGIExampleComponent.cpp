/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <DiffuseGIExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Feature/CoreLights/PhotometricValue.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>

#include <Atom/RPI.Public/View.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/IO/IOUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/sort.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <Automation/ScriptableImGui.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>

namespace AtomSampleViewer
{
    const char* DiffuseGIExampleComponent::CornellBoxColorNames[] =
    {
        "Red",
        "Green",
        "Blue",
        "Yellow",
        "White"
    };

    const AZ::Render::ShadowmapSize DiffuseGIExampleComponent::s_shadowmapSizes[] =
    {
        AZ::Render::ShadowmapSize::Size256,
        AZ::Render::ShadowmapSize::Size512,
        AZ::Render::ShadowmapSize::Size1024,
        AZ::Render::ShadowmapSize::Size2048
    };

    void DiffuseGIExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DiffuseGIExampleComponent, Component>()
                ->Version(0)
                ;
        }
    }

    void DiffuseGIExampleComponent::Activate()
    {
        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();

        // setup the camera
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 180.f);

        // disable global Ibl in the example scene since we're controlling it separately
        DisableGlobalIbl();

        LoadSampleSceneAssets();
    }

    void DiffuseGIExampleComponent::UnloadSampleScene(bool geometryOnly)
    {
        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();

        // release meshes
        AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::MeshFeatureProcessorInterface>();
        for (auto& meshHandle : m_meshHandles)
        {
            meshFeatureProcessor->ReleaseMesh(meshHandle);
        }

        // unload everything else if necessary
        if (!geometryOnly)
        {
            if (m_diffuseProbeGrid)
            {
                AZ::Render::DiffuseProbeGridFeatureProcessorInterface* diffuseProbeGridFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseProbeGridFeatureProcessorInterface>();
                diffuseProbeGridFeatureProcessor->RemoveProbeGrid(m_diffuseProbeGrid);
            }

            if (m_directionalLightHandle.IsValid())
            {
                AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DirectionalLightFeatureProcessorInterface>();
                directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
            }

            if (m_pointLightHandle.IsValid())
            {
                AZ::Render::PointLightFeatureProcessorInterface* pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
                pointLightFeatureProcessor->ReleaseLight(m_pointLightHandle);
            }
        }
    }

    void DiffuseGIExampleComponent::SetSampleScene(bool geometryOnly /*= false*/)
    {
        UnloadSampleScene(geometryOnly);

        // set new scene
        switch (m_sampleScene)
        {
        case SampleScene::CornellBox:
            CreateCornellBoxScene(geometryOnly);
            break;
        case SampleScene::Sponza:
            CreateSponzaScene();
            break;
        };
    }

    void DiffuseGIExampleComponent::Deactivate()
    {
        // disable camera
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        m_defaultIbl.Reset();

        UnloadSampleScene(false);
    }

    float DiffuseGIExampleComponent::ComputePointLightAttenuationRadius(AZ::Color color, float intensity)
    {
        static const float CutoffIntensity = 0.1f;

        float luminance = AZ::Render::PhotometricValue::GetPerceptualLuminance(color * intensity);
        return sqrt(luminance / CutoffIntensity);
    }

    void DiffuseGIExampleComponent::LoadSampleSceneAssets()
    {
        // load plane and cube models
        // all geometry in the CornellBox is created from planes and boxes
        static constexpr const char PlaneModelPath[] = "objects/plane.azmodel";
        AZ::Data::AssetId planeAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(PlaneModelPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_planeModelAsset.Create(planeAssetId);

        static constexpr const char CubeModelPath[] = "objects/cube.azmodel";
        AZ::Data::AssetId cubeAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(CubeModelPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_cubeModelAsset.Create(cubeAssetId);

        // materials for the CornellBox
        static constexpr const char RedMaterialPath[] = "materials/diffusegiexample/red.azmaterial";
        AZ::Data::AssetId redMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(RedMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_redMaterialAsset.Create(redMaterialAssetId);

        static constexpr const char GreenMaterialPath[] = "materials/diffusegiexample/green.azmaterial";
        AZ::Data::AssetId greenMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(GreenMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_greenMaterialAsset.Create(greenMaterialAssetId);

        static constexpr const char BlueMaterialPath[] = "materials/diffusegiexample/blue.azmaterial";
        AZ::Data::AssetId blueMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(BlueMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_blueMaterialAsset.Create(blueMaterialAssetId);

        static constexpr const char YellowMaterialPath[] = "materials/diffusegiexample/yellow.azmaterial";
        AZ::Data::AssetId yellowMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(YellowMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_yellowMaterialAsset.Create(yellowMaterialAssetId);

        static constexpr const char WhiteMaterialPath[] = "materials/diffusegiexample/white.azmaterial";
        AZ::Data::AssetId whiteMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(WhiteMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_whiteMaterialAsset.Create(whiteMaterialAssetId);

        // Sponza models
        static constexpr const char InteriorModelPath[] = "objects/sponza.azmodel";
        AZ::Data::AssetId interiorAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(InteriorModelPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        m_sponzaModelAsset.Create(interiorAssetId);

        // diffuse IBL cubemap
        const constexpr char* DiffuseAssetPath = "lightingpresets/lowcontrast/palermo_sidewalk_4k_iblskyboxcm_ibldiffuse.exr.streamingimage";
        if (!m_diffuseImageAsset.IsReady())
        {
            m_diffuseImageAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>(DiffuseAssetPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
            m_diffuseImageAsset.QueueLoad();
        }
    }

    AZ::Data::Asset<AZ::RPI::MaterialAsset>& DiffuseGIExampleComponent::GetCornellBoxMaterialAsset(CornellBoxColors color)
    {
        switch (color)
        {
        case CornellBoxColors::Red:
            return m_redMaterialAsset;
        case CornellBoxColors::Green:
            return m_greenMaterialAsset;
        case CornellBoxColors::Blue:
            return m_blueMaterialAsset;
        case CornellBoxColors::Yellow:
            return m_yellowMaterialAsset;
        case CornellBoxColors::White:
            return m_whiteMaterialAsset;
        };

        AZ_Assert(false, "Unknown material color");
        return m_whiteMaterialAsset;
    }

    void DiffuseGIExampleComponent::CreateCornellBoxScene(bool geometryOnly)
    {
        m_meshHandles.resize(aznumeric_cast<uint32_t>(CornellBoxMeshes::Count));
        
        // left wall
        if (m_leftWallVisible)
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = GetCornellBoxMaterialAsset(m_leftWallColor);
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(GetCornellBoxMaterialAsset(m_leftWallColor));
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(-5.0f, 0.0f, 0.0f);
            transform *= AZ::Transform::CreateRotationY(AZ::Constants::HalfPi);
            AZ::Vector3 nonUniformScale(10.05f, 10.05f, 1.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::LeftWall)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_planeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::LeftWall)], transform, nonUniformScale);
        }
        
        // right wall
        if (m_rightWallVisible)
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = GetCornellBoxMaterialAsset(m_rightWallColor);
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(GetCornellBoxMaterialAsset(m_rightWallColor));
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(5.0f, 0.0f, 0.0f);
            transform *= AZ::Transform::CreateRotationY(-AZ::Constants::HalfPi);
            AZ::Vector3 nonUniformScale(10.05f, 10.05f, 1.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::RightWall)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_planeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::RightWall)], transform, nonUniformScale);
        }
        
        // back wall
        if (m_backWallVisible)
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = GetCornellBoxMaterialAsset(m_backWallColor);
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(GetCornellBoxMaterialAsset(m_backWallColor));
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(0.0f, 5.0f, 0.0f);
            transform *= AZ::Transform::CreateRotationX(AZ::Constants::HalfPi);
            AZ::Vector3 nonUniformScale(10.05f, 10.05f, 1.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::BackWall)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_planeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::BackWall)], transform, nonUniformScale);
        }
        
        // ceiling
        if (m_ceilingVisible)
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = GetCornellBoxMaterialAsset(m_ceilingColor);
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(GetCornellBoxMaterialAsset(m_ceilingColor));
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(0.0f, 0.0f, 5.0f);
            transform *= AZ::Transform::CreateRotationX(AZ::Constants::Pi);
            AZ::Vector3 nonUniformScale(10.05f, 10.05f, 1.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::Ceiling)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_planeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::Ceiling)], transform, nonUniformScale);
        }
        
        // floor
        if (m_floorVisible)
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = GetCornellBoxMaterialAsset(m_floorColor);
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(GetCornellBoxMaterialAsset(m_floorColor));
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(0.0f, 0.0f, -5.0f);
            AZ::Vector3 nonUniformScale(10.05f, 10.05f, 1.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::Floor)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_planeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::Floor)], transform, nonUniformScale);
        }
        
        // large box
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = m_whiteMaterialAsset;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(m_whiteMaterialAsset);
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(-2.0f, 0.0f, -2.0f);
            transform *= AZ::Transform::CreateRotationZ(AZ::Constants::HalfPi * 0.2f);
            AZ::Vector3 nonUniformScale(3.0f, 3.0f, 6.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::LargeBox)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_cubeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::LargeBox)], transform, nonUniformScale);
        }
        
        // small box
        {
            AZ::Render::MaterialAssignmentMap materialMap;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialAsset = m_whiteMaterialAsset;
            materialMap[AZ::Render::DefaultMaterialAssignmentId].m_materialInstance = AZ::RPI::Material::FindOrCreate(m_whiteMaterialAsset);
        
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(2.0f, -1.5f, -3.5f);
            transform *= AZ::Transform::CreateRotationZ(-AZ::Constants::HalfPi * 0.2f);
            AZ::Vector3 nonUniformScale(3.0f, 3.0f, 3.0f);
            m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::SmallBox)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_cubeModelAsset }, materialMap);
            GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(CornellBoxMeshes::SmallBox)], transform, nonUniformScale);
        }

        // stop now if we were only loading the geometry
        if (geometryOnly)
        {
            return;
        }

        m_pointLightPos = AZ::Vector3(0.0f, -1.0f, 4.0f);
        m_pointLightColor = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        m_pointLightIntensity = 20.0f;

        // point light
        {
            AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
            AZ::Render::PointLightFeatureProcessorInterface* pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
            m_pointLightHandle = pointLightFeatureProcessor->AcquireLight();
            pointLightFeatureProcessor->SetPosition(m_pointLightHandle, m_pointLightPos);
            pointLightFeatureProcessor->SetRgbIntensity(m_pointLightHandle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela>(m_pointLightIntensity * m_pointLightColor));
            pointLightFeatureProcessor->SetBulbRadius(m_pointLightHandle, 1.0f);
            pointLightFeatureProcessor->SetAttenuationRadius(m_pointLightHandle, ComputePointLightAttenuationRadius(m_pointLightColor, m_pointLightIntensity));
        }

        // diffuse probe grid
        {
            AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
            AZ::Render::DiffuseProbeGridFeatureProcessorInterface* diffuseProbeGridFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseProbeGridFeatureProcessorInterface>();
            AZ::Transform transform = AZ::Transform::CreateIdentity();

            m_origin.Set(0.3f, -0.25f, 0.5f);
            transform.SetTranslation(m_origin);
            m_diffuseProbeGrid = diffuseProbeGridFeatureProcessor->AddProbeGrid(transform, AZ::Vector3(12.0f, 12.0f, 12.0f), AZ::Vector3(1.5f, 1.5f, 2.0f));
            diffuseProbeGridFeatureProcessor->SetAmbientMultiplier(m_diffuseProbeGrid, m_ambientMultiplier);

            m_viewBias = 0.7f;
            diffuseProbeGridFeatureProcessor->SetViewBias(m_diffuseProbeGrid, m_viewBias);

            m_normalBias = 0.1f;
            diffuseProbeGridFeatureProcessor->SetNormalBias(m_diffuseProbeGrid, m_normalBias);

            AZ::Render::DiffuseGlobalIlluminationFeatureProcessorInterface* diffuseGlobalIlluminationFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseGlobalIlluminationFeatureProcessorInterface>();
            diffuseGlobalIlluminationFeatureProcessor->SetQualityLevel(AZ::Render::DiffuseGlobalIlluminationQualityLevel::Medium);
        }

        // camera
        m_cameraTranslation = AZ::Vector3(0.0f, -17.5f, 0.0f);
        m_cameraHeading = 0.0f;

        // disable diffuse Ibl
        DisableGlobalIbl();
    }

    void DiffuseGIExampleComponent::CreateSponzaScene()
    {
        m_meshHandles.resize(aznumeric_cast<uint32_t>(SponzaMeshes::Count));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        m_meshHandles[aznumeric_cast<uint32_t>(SponzaMeshes::Inside)] = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_sponzaModelAsset });
        GetMeshFeatureProcessor()->SetTransform(m_meshHandles[aznumeric_cast<uint32_t>(SponzaMeshes::Inside)], transform);
        
        m_directionalLightPitch = AZ::DegToRad(-45.0f);
        m_directionalLightYaw = AZ::DegToRad(62.0f);
        m_directionalLightColor = AZ::Color(0.92f, 0.78f, 0.35f, 1.0f);
        m_directionalLightIntensity = 30.0f;

        m_pointLightPos = AZ::Vector3(10.0f, -4.25f, 1.5f);
        m_pointLightColor = AZ::Color(1.0f, 0.0f, 0.0f, 1.0f);
        m_pointLightIntensity = 10.0f;

        m_ambientMultiplier = 1.0f;

        // directional light
        {
            AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
            AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DirectionalLightFeatureProcessorInterface>();
            m_directionalLightHandle = directionalLightFeatureProcessor->AcquireLight();
            const auto lightTransform = AZ::Transform::CreateRotationZ(m_directionalLightYaw) * AZ::Transform::CreateRotationX(m_directionalLightPitch);
            directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightTransform.GetBasis(1));
            directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(m_directionalLightIntensity * m_directionalLightColor));
            directionalLightFeatureProcessor->SetCascadeCount(m_directionalLightHandle, 4);
            directionalLightFeatureProcessor->SetShadowmapSize(m_directionalLightHandle, AZ::Render::ShadowmapSize::Size2048);
            directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(m_directionalLightHandle, false);
            directionalLightFeatureProcessor->SetShadowFilterMethod(m_directionalLightHandle, AZ::Render::ShadowFilterMethod::EsmPcf);
            directionalLightFeatureProcessor->SetShadowBoundaryWidth(m_directionalLightHandle, 0.03f);
            directionalLightFeatureProcessor->SetPredictionSampleCount(m_directionalLightHandle, 4);
            directionalLightFeatureProcessor->SetFilteringSampleCount(m_directionalLightHandle, 16);
            directionalLightFeatureProcessor->SetGroundHeight(m_directionalLightHandle, 0.0f);
        }

        // point light
        {
            AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
            AZ::Render::PointLightFeatureProcessorInterface* pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
            m_pointLightHandle = pointLightFeatureProcessor->AcquireLight();
            pointLightFeatureProcessor->SetPosition(m_pointLightHandle, m_pointLightPos);
            pointLightFeatureProcessor->SetRgbIntensity(m_pointLightHandle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela>(m_pointLightIntensity * m_pointLightColor));
            pointLightFeatureProcessor->SetBulbRadius(m_pointLightHandle, 1.0f);
            pointLightFeatureProcessor->SetAttenuationRadius(m_pointLightHandle, ComputePointLightAttenuationRadius(m_pointLightColor, m_pointLightIntensity));
        }

        // diffuse probe grid
        {
            AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
            AZ::Render::DiffuseProbeGridFeatureProcessorInterface* diffuseProbeGridFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseProbeGridFeatureProcessorInterface>();
            transform = AZ::Transform::CreateIdentity();
        
            m_origin.Set(1.4f, -1.25f, 5.0f);
            transform.SetTranslation(m_origin);
            m_diffuseProbeGrid = diffuseProbeGridFeatureProcessor->AddProbeGrid(transform, AZ::Vector3(35.0f, 45.0f, 25.0f), AZ::Vector3(3.0f, 3.0f, 4.0f));
            diffuseProbeGridFeatureProcessor->SetAmbientMultiplier(m_diffuseProbeGrid, m_ambientMultiplier);

            m_viewBias = 0.55f;
            diffuseProbeGridFeatureProcessor->SetViewBias(m_diffuseProbeGrid, m_viewBias);

            m_normalBias = 0.4f;
            diffuseProbeGridFeatureProcessor->SetNormalBias(m_diffuseProbeGrid, m_normalBias);

            AZ::Render::DiffuseGlobalIlluminationFeatureProcessorInterface* diffuseGlobalIlluminationFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseGlobalIlluminationFeatureProcessorInterface>();
            diffuseGlobalIlluminationFeatureProcessor->SetQualityLevel(AZ::Render::DiffuseGlobalIlluminationQualityLevel::Medium);
        }

        // camera
        m_cameraTranslation = AZ::Vector3(5.0f, 0.0f, 5.0f);
        m_cameraHeading = 90.0f;

        // diffuse Ibl
        EnableDiffuseIbl();
    }

    void DiffuseGIExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_resetCamera)
        {
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Reset);
            AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, m_cameraTranslation);
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(m_cameraHeading));
            m_resetCamera = false;
        }

        // ImGui sidebar
        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        AZ::Render::DiffuseProbeGridFeatureProcessorInterface* diffuseProbeGridFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DiffuseProbeGridFeatureProcessorInterface>();
        AZ::Render::PointLightFeatureProcessorInterface* pointLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::PointLightFeatureProcessorInterface>();
        AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DirectionalLightFeatureProcessorInterface>();

        bool sceneChanged = false;

        // initialize the scene in OnTick() in order to delay a frame between samples
        if (m_sampleScene == SampleScene::None)
        {
            m_sampleScene = CornellBox;
            sceneChanged = true;
        }

        if (m_imguiSidebar.Begin())
        {
            // sample scene
            {
                ImGui::Text("Scene");
                sceneChanged |= ImGui::RadioButton("Cornell Box", (int*)&m_sampleScene, aznumeric_cast<uint32_t>(CornellBox));
                sceneChanged |= ImGui::RadioButton("Sponza", (int*)&m_sampleScene, aznumeric_cast<uint32_t>(Sponza));
                ImGui::NewLine();
            }

            // diffuse probe grid settings
            {
                bool diffuseProbeGridChanged = false;
                diffuseProbeGridChanged |= ImGui::Checkbox("Enable Diffuse GI", &m_enableDiffuseGI);
                diffuseProbeGridChanged |= ImGui::SliderFloat("##AmbientMultiplier", &m_ambientMultiplier, 0.0f, 10.0f);
                ImGui::NewLine();

                ImGui::Text("View Bias");
                diffuseProbeGridChanged |= ImGui::SliderFloat("##ViewBias", &m_viewBias, 0.01f, 2.0f);

                ImGui::Text("Normal Bias");
                diffuseProbeGridChanged |= ImGui::SliderFloat("##NormalBias", &m_normalBias, 0.01f, 2.0f);
                diffuseProbeGridChanged |= ImGui::Checkbox("GI Shadows", &m_giShadows);

                ImGui::NewLine();

                ImGui::Text("Grid Origin");
                float originX = m_origin.GetX();
                diffuseProbeGridChanged |= ImGui::SliderFloat("##OriginX", &originX, -10.0f, 10.0f);
                float originY = m_origin.GetY();
                diffuseProbeGridChanged |= ImGui::SliderFloat("##OriginY", &originY, -10.0f, 10.0f);
                float originZ = m_origin.GetZ();
                diffuseProbeGridChanged |= ImGui::SliderFloat("##OriginZ", &originZ, -10.0f, 10.0f);

                m_origin.SetX(originX);
                m_origin.SetY(originY);
                m_origin.SetZ(originZ);

                if (diffuseProbeGridChanged && diffuseProbeGridFeatureProcessor->IsValidProbeGridHandle(m_diffuseProbeGrid))
                {
                    diffuseProbeGridFeatureProcessor->Enable(m_diffuseProbeGrid, m_enableDiffuseGI);
                    diffuseProbeGridFeatureProcessor->SetAmbientMultiplier(m_diffuseProbeGrid, m_ambientMultiplier);
                    diffuseProbeGridFeatureProcessor->SetViewBias(m_diffuseProbeGrid, m_viewBias);
                    diffuseProbeGridFeatureProcessor->SetNormalBias(m_diffuseProbeGrid, m_normalBias);
                    diffuseProbeGridFeatureProcessor->SetGIShadows(m_diffuseProbeGrid, m_giShadows);

                    AZ::Transform transform = AZ::Transform::CreateIdentity();
                    transform.SetTranslation(m_origin);
                    diffuseProbeGridFeatureProcessor->SetTransform(m_diffuseProbeGrid, transform);
                }

                ImGui::NewLine();
            }

            // diffuse IBL (Sponza only)
            if (m_sampleScene == SampleScene::Sponza)
            {
                bool diffuseIblChanged = false;
                diffuseIblChanged |= ImGui::Checkbox("Diffuse IBL (Sky Light)", &m_useDiffuseIbl);
                diffuseIblChanged |= ImGui::SliderFloat("Exposure", &m_diffuseIblExposure, -10.0f, 10.0f);

                if (diffuseIblChanged && diffuseProbeGridFeatureProcessor->IsValidProbeGridHandle(m_diffuseProbeGrid))
                {
                    diffuseProbeGridFeatureProcessor->SetUseDiffuseIbl(m_diffuseProbeGrid, m_useDiffuseIbl);

                    if (m_useDiffuseIbl)
                    {
                        EnableDiffuseIbl();
                    }
                    else
                    {
                        DisableGlobalIbl();
                    }
                }

                ImGui::NewLine();
            }

            // directional light
            if (m_directionalLightHandle.IsValid())
            {
                bool directionalLightChanged = false;
                ImGui::Text("Directional Light");
                directionalLightChanged |= ImGui::SliderAngle("Pitch", &m_directionalLightPitch, -90.0f, 0.f);
                directionalLightChanged |= ImGui::SliderAngle("Yaw", &m_directionalLightYaw, 0.f, 360.f);

                ImGui::Text("Color");
                float directionalLightColor[4];
                m_directionalLightColor.StoreToFloat4(&directionalLightColor[0]);
                directionalLightChanged |= ImGui::ColorEdit3("", &directionalLightColor[0]);

                ImGui::Text("Intensity");
                directionalLightChanged |= ImGui::SliderFloat("##DirectionalLightIntensity", &m_directionalLightIntensity, 0, 100);

                if (directionalLightChanged)
                {
                    const auto lightTransform = AZ::Transform::CreateRotationZ(m_directionalLightYaw) * AZ::Transform::CreateRotationX(m_directionalLightPitch);
                    directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, lightTransform.GetBasis(1));

                    m_directionalLightColor = AZ::Color(directionalLightColor[0], directionalLightColor[1], directionalLightColor[2], 1.0f);
                    directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(m_directionalLightColor * m_directionalLightIntensity));
                }

                // Camera Configuration
                {
                    Camera::Configuration config;
                    Camera::CameraRequestBus::EventResult(
                        config,
                        GetCameraEntityId(),
                        &Camera::CameraRequestBus::Events::GetCameraConfiguration);
                    directionalLightFeatureProcessor->SetCameraConfiguration(
                        m_directionalLightHandle,
                        config);
                }

                // Camera Transform
                {
                    AZ::Transform transform = AZ::Transform::CreateIdentity();
                    AZ::TransformBus::EventResult(
                        transform,
                        GetCameraEntityId(),
                        &AZ::TransformBus::Events::GetWorldTM);
                    directionalLightFeatureProcessor->SetCameraTransform(
                        m_directionalLightHandle, transform);
                }

                ImGui::NewLine();
            }

            // point light
            if (m_pointLightHandle.IsValid())
            {
                bool pointLightChanged = false;
                ImGui::Text("Point Light");
                float lightX = m_pointLightPos.GetX();
                pointLightChanged |= ImGui::SliderFloat("##PointLightX", &lightX, -10.0f, 10.0f);
                float lightY = m_pointLightPos.GetY();
                pointLightChanged |= ImGui::SliderFloat("##PointLightY", &lightY, -10.0f, 10.0f);
                float lightZ = m_pointLightPos.GetZ();
                pointLightChanged |= ImGui::SliderFloat("##PointLightZ", &lightZ, -10.0f, 10.0f);

                m_pointLightPos.SetX(lightX);
                m_pointLightPos.SetY(lightY);
                m_pointLightPos.SetZ(lightZ);

                float pointLightColor[4];
                m_pointLightColor.StoreToFloat4(&pointLightColor[0]);
                pointLightChanged |= ImGui::ColorEdit3("Point Light Color", &pointLightColor[0]);

                ImGui::Text("Intensity");
                pointLightChanged |= ImGui::SliderFloat("##PointLightIntensity", &m_pointLightIntensity, 0, 100);

                if (pointLightChanged)
                {
                    m_pointLightColor = AZ::Color(pointLightColor[0], pointLightColor[1], pointLightColor[2], 1.0f);

                    pointLightFeatureProcessor->SetPosition(m_pointLightHandle, m_pointLightPos);
                    pointLightFeatureProcessor->SetRgbIntensity(m_pointLightHandle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela>(m_pointLightColor * m_pointLightIntensity));
                    pointLightFeatureProcessor->SetAttenuationRadius(m_pointLightHandle, ComputePointLightAttenuationRadius(m_pointLightColor, m_pointLightIntensity));
                }

                ImGui::NewLine();
            }

            // Cornell Box specific
            if (m_sampleScene == CornellBox)
            {
                bool geometryChanged = false;
                geometryChanged |= ImGui::Checkbox("Left Wall Visible", &m_leftWallVisible);
                geometryChanged |= ImGui::Combo("Left Wall Color", (int*)&m_leftWallColor, CornellBoxColorNames, CornellBoxColorNamesCount);
                geometryChanged |= ImGui::Checkbox("Right Wall Visible", &m_rightWallVisible);
                geometryChanged |= ImGui::Combo("Right Wall Color", (int*)&m_rightWallColor, CornellBoxColorNames, CornellBoxColorNamesCount);
                geometryChanged |= ImGui::Checkbox("Back Wall Visible", &m_backWallVisible);
                geometryChanged |= ImGui::Combo("Back Wall Color", (int*)&m_backWallColor, CornellBoxColorNames, CornellBoxColorNamesCount);
                geometryChanged |= ImGui::Checkbox("Floor Visible", &m_floorVisible);
                geometryChanged |= ImGui::Combo("Floor Color", (int*)&m_floorColor, CornellBoxColorNames, CornellBoxColorNamesCount);
                geometryChanged |= ImGui::Checkbox("Ceiling Visible", &m_ceilingVisible);
                geometryChanged |= ImGui::Combo("Ceiling Color", (int*)&m_ceilingColor, CornellBoxColorNames, CornellBoxColorNamesCount);

                if (geometryChanged)
                {
                    SetSampleScene(true);
                }
            }

            m_imguiSidebar.End();
        }

        if (sceneChanged)
        {
            m_resetCamera = true;
            SetSampleScene();
        }
    }

    void DiffuseGIExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);
    }

    void DiffuseGIExampleComponent::DisableGlobalIbl()
    {
        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();

        // disable Ibl by setting the empty cubemap
        const constexpr char* DiffuseAssetPath = "textures/default/default_iblglobalcm_ibldiffuse.dds.streamingimage";
        const constexpr char* SpecularAssetPath = "textures/default/default_iblglobalcm_iblspecular.dds.streamingimage";

        auto assertTraceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> diffuseImageAsset =
            AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>
            (DiffuseAssetPath, assertTraceLevel);
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> specularImageAsset =
            AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>
            (SpecularAssetPath, assertTraceLevel);

        auto featureProcessor = scene->GetFeatureProcessor<AZ::Render::ImageBasedLightFeatureProcessorInterface>();
        AZ_Assert(featureProcessor, "Unable to find ImageBasedLightFeatureProcessorInterface on scene.");

        featureProcessor->SetDiffuseImage(diffuseImageAsset);
        featureProcessor->SetSpecularImage(specularImageAsset);
    }

    void DiffuseGIExampleComponent::EnableDiffuseIbl()
    {
        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        AZ::Render::ImageBasedLightFeatureProcessorInterface* imageBaseLightFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::ImageBasedLightFeatureProcessorInterface>();
        AZ_Assert(imageBaseLightFeatureProcessor, "Unable to find ImageBasedLightFeatureProcessorInterface on scene.");

        imageBaseLightFeatureProcessor->SetDiffuseImage(m_diffuseImageAsset);
        imageBaseLightFeatureProcessor->SetExposure(m_diffuseIblExposure);
    }
} // namespace AtomSampleViewer
