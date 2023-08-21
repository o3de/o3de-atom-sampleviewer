/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SSRExampleComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/Feature/SpecularReflections/SpecularReflectionsFeatureProcessorInterface.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>
#include <Utils/Utils.h>

#include <SSRExampleComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    void SSRExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < SSRExampleComponent, AZ::Component>()
                ->Version(0)
            ;
        }
    }

    void SSRExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        m_imguiSidebar.Activate();

        // setup the camera
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 180.f);

        // create scene
        CreateModels();
        CreateGroundPlane();

        InitLightingPresets(true);

        // enable the SSR pass in the pipeline
        m_enableSSR = true;
        UpdateSSROptions();
    }

    void SSRExampleComponent::Deactivate()
    {
        // disable the SSR pass in the pipeline
        m_enableSSR = false;
        UpdateSSROptions();

        ShutdownLightingPresets();

        GetMeshFeatureProcessor()->ReleaseMesh(m_statueMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_boxMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_shaderBallMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundMeshHandle);

        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
    }

    void SSRExampleComponent::CreateModels()
    {
        GetMeshFeatureProcessor();

        // statue
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("objects/hermanubis/hermanubis_stone.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(ATOMSAMPLEVIEWER_TRAIT_SSR_SAMPLE_HERMANUBIS_MODEL_NAME, AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(0.0f, 0.0f, -0.05f);

            m_statueMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_statueMeshHandle, transform);
        }

        // cube
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("materials/ssrexample/cube.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cube.fbx.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(-4.5f, 0.0f, 0.49f);

            m_boxMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_boxMeshHandle, transform);
        }

        // shader ball
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("Materials/Presets/PBR/default_grid.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/ShaderBall_simple.fbx.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform *= AZ::Transform::CreateRotationZ(AZ::Constants::Pi);
            transform.SetTranslation(4.5f, 0.0f, 0.89f);

            m_shaderBallMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_shaderBallMeshHandle, transform);
        }
    }

    void SSRExampleComponent::CreateGroundPlane()
    {
        AZ::Render::MeshFeatureProcessorInterface* meshFeatureProcessor = GetMeshFeatureProcessor();
        if (m_groundMeshHandle.IsValid())
        {
            meshFeatureProcessor->ReleaseMesh(m_groundMeshHandle);
        }

        // load material
        AZStd::string materialName;
        switch (m_groundPlaneMaterial)
        {
        case 0:
            materialName = AZStd::string::format("materials/ssrexample/groundplanechrome.azmaterial");
            break;
        case 1:
            materialName = AZStd::string::format("materials/ssrexample/groundplanealuminum.azmaterial");
            break;
        case 2:
            materialName = AZStd::string::format("materials/presets/pbr/default_grid.azmaterial");
            break;
        default:
            materialName = AZStd::string::format("materials/ssrexample/groundplanemirror.azmaterial");
            break;
        }

        AZ::Data::AssetId groundMaterialAssetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(materialName.c_str(), AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundMaterialAsset.Create(groundMaterialAssetId);

        // load mesh
        AZ::Data::Asset<AZ::RPI::ModelAsset> planeModel = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.fbx.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ planeModel }, AZ::RPI::Material::FindOrCreate(m_groundMaterialAsset));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        const AZ::Vector3 nonUniformScale(15.0f, 15.0f, 1.0f);
        GetMeshFeatureProcessor()->SetTransform(m_groundMeshHandle, transform, nonUniformScale);
    }

    void SSRExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        if (m_resetCamera)
        {
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Reset);
            AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTranslation, AZ::Vector3(7.5f, -10.5f, 3.0f));
            AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetHeading, AZ::DegToRad(22.5f));
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetPitch, AZ::DegToRad(-10.0f));
            m_resetCamera = false;
        }

        DrawSidebar();
    }

    void SSRExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        AZ::RHI::Ptr<AZ::RHI::Device> device = AZ::RHI::RHISystemInterface::Get()->GetDevice();
        bool rayTracingSupported = device->GetFeatures().m_rayTracing;

        bool optionsChanged = false;

        ImGui::NewLine();
        optionsChanged |= ImGui::Checkbox("Enable SSR", &m_enableSSR);

        if (rayTracingSupported)
        {
            optionsChanged |= ImGui::Checkbox("Hardware Ray Tracing", &m_rayTracing);
            if (m_rayTracing)
            {
                optionsChanged |= ImGui::Checkbox("Ray Trace Fallback Data", &m_rayTraceFallbackData);
            }
        }

        ImGui::NewLine();
        ImGui::Text("Temporal Filtering Strength");
        optionsChanged |= ImGui::SliderFloat("##Temporal Filtering Strength", &m_temporalFilteringStrength, 0.0f, 3.0f);
        if (optionsChanged)
        {
            UpdateSSROptions();
        }

        ImGui::NewLine();
        ImGui::Text("Ground Plane Material");
        bool materialChanged = false;
        materialChanged |= ScriptableImGui::RadioButton("Chrome", &m_groundPlaneMaterial, 0);
        materialChanged |= ScriptableImGui::RadioButton("Aluminum", &m_groundPlaneMaterial, 1);
        materialChanged |= ScriptableImGui::RadioButton("Default Grid", &m_groundPlaneMaterial, 2);
        materialChanged |= ScriptableImGui::RadioButton("Mirror", &m_groundPlaneMaterial, 3);
        if (materialChanged)
        {
            CreateGroundPlane();
        }

        ImGui::NewLine();
        ImGuiLightingPreset();

        m_imguiSidebar.End();
    }

    void SSRExampleComponent::UpdateSSROptions()
    {
        AZ::Render::SpecularReflectionsFeatureProcessorInterface* specularReflectionsFeatureProcessor = m_scene->GetFeatureProcessorForEntityContextId<AZ::Render::SpecularReflectionsFeatureProcessorInterface>(GetEntityContextId());
        AZ_Assert(specularReflectionsFeatureProcessor, "SpecularReflectionsFeatureProcessor not available.");

        AZ::Render::SSROptions ssrOptions = specularReflectionsFeatureProcessor->GetSSROptions();
        ssrOptions.m_enable = m_enableSSR;
        ssrOptions.m_rayTracing = m_rayTracing;
        ssrOptions.m_rayTraceFallbackData = m_rayTraceFallbackData;
        ssrOptions.m_coneTracing = false;
        ssrOptions.m_maxRoughness = 0.5f;
        ssrOptions.m_maxDepthThreshold = m_maxDepthThreshold;
        ssrOptions.m_temporalFilteringStrength = m_temporalFilteringStrength;

        specularReflectionsFeatureProcessor->SetSSROptions(ssrOptions);
    }
}
