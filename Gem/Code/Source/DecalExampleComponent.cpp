/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <DecalExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>


namespace AtomSampleViewer
{
    namespace
    {
        static constexpr const char* TargetMeshName = "objects/plane.azmodel";
        static constexpr const char* TargetMaterialName = "materials/defaultpbr.azmaterial";
    }

    void DecalExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < DecalExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void DecalExampleComponent::Activate()
    {
        m_sampleName = "DecalExampleComponent";

        CreateDecalContainer();
        m_decalContainer->SetNumDecalsActive(m_decalContainer->GetMaxDecals());

        m_imguiSidebar.Activate();

        // List of all assets this example needs.
        AZStd::vector<AZ::AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {"objects/plane.azmodel", azrtti_typeid<AZ::RPI::ModelAsset>()}, // The model
        };

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);

        PreloadAssets(assetList);
    }

    void DecalExampleComponent::OnAllAssetsReadyActivate()
    {
        CreatePlaneObject();
        EnableArcBallCameraController();
        ConfigureCameraToLookDownAtObject();
        AddImageBasedLight();

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
        AZ::TickBus::Handler::BusConnect();
    }

    void DecalExampleComponent::CreatePlaneObject()
    {
        const auto meshAsset = m_assetLoadManager.GetAsset<AZ::RPI::ModelAsset>(TargetMeshName);
        const auto materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(TargetMaterialName, AZ::RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ meshAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
        ScaleObjectToFitDecals();
    }

    void DecalExampleComponent::ScaleObjectToFitDecals()
    {
        const AZ::Vector3 nonUniformScale(4.0f, 1.0f, 1.0f);
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, AZ::Transform::CreateIdentity(), nonUniformScale);
    }

    void DecalExampleComponent::Deactivate()
    {
        m_decalContainer = nullptr;
        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        m_defaultIbl.Reset();
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
    }

    void DecalExampleComponent::AddImageBasedLight()
    {
        m_defaultIbl.Init(AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get());
    }

    void DecalExampleComponent::EnableArcBallCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
    }

    void DecalExampleComponent::ConfigureCameraToLookDownAtObject()
    {
        const AZ::Vector3 CameraPanOffet(0.0f, 0.5f, -0.5f);
        const float CameraDistance = 1.5f;
        const float CameraPitch = -0.8f;

        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPan, CameraPanOffet);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, CameraPitch);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, CameraDistance);
    }

    void DecalExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawSidebar();
    }

    void DecalExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }
        int numDecalsActive = m_decalContainer->GetNumDecalsActive();
        if (ScriptableImGui::SliderInt("Point count", &numDecalsActive, 0, m_decalContainer->GetMaxDecals()))
        {
            m_decalContainer->SetNumDecalsActive(numDecalsActive);
        }

        if (ScriptableImGui::Checkbox("Clone decals", &m_cloneDecalsEnabled))
        {
            if (m_cloneDecalsEnabled)
            {
                m_decalContainerClone->CloneFrom(*m_decalContainer.get());
            }
            else
            {
                m_decalContainerClone->SetNumDecalsActive(0);
            }
        }

        m_imguiSidebar.End();
    }

    void DecalExampleComponent::CreateDecalContainer()
    {
        const AZ::RPI::Scene* scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        const auto decalFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::DecalFeatureProcessorInterface>();
        m_decalContainer = AZStd::make_unique<DecalContainer>(decalFeatureProcessor, AZ::Vector3(1,0,0));
        m_decalContainerClone = AZStd::make_unique<DecalContainer>(decalFeatureProcessor, AZ::Vector3(-1,0,0));
    }

}
