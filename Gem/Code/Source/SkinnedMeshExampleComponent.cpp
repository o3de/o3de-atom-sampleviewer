/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SkinnedMeshExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>

#include <Atom/Feature/SkinnedMesh/SkinnedMeshInputBuffers.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerBus.h>

#include <AzCore/Script/ScriptTimePoint.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    namespace
    {
        static const char* const SkinnedMeshMaterial = "materials/defaultpbrmaterial.azmaterial";
    }

    void SkinnedMeshExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < SkinnedMeshExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void SkinnedMeshExampleComponent::Activate()
    {
        CreateSkinnedMeshContainer();
        m_skinnedMeshContainer->SetActiveSkinnedMeshCount(1);

        AZ::TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
        CreatePlaneObject();
        ConfigureCamera();
        AddImageBasedLight();
    }

    void SkinnedMeshExampleComponent::CreatePlaneObject()
    {
        auto meshAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel");
        m_planeMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ meshAsset });
        GetMeshFeatureProcessor()->SetTransform(m_planeMeshHandle, AZ::Transform::CreateIdentity());
    }

    void SkinnedMeshExampleComponent::Deactivate()
    {
        m_skinnedMeshContainer = nullptr;
        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        GetMeshFeatureProcessor()->ReleaseMesh(m_planeMeshHandle);
        m_defaultIbl.Reset();
    }

    void SkinnedMeshExampleComponent::AddImageBasedLight()
    {
        m_defaultIbl.Init(AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get());
    }

    void SkinnedMeshExampleComponent::ConfigureCamera()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPosition, AZ::Vector3(0.0f, -1.0f, 1.0f));
        AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequestBus::Events::SetPitch, -0.8f);
    }

    void SkinnedMeshExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        m_runTime += deltaTime;
        DrawSidebar();
        if (!m_useFixedTime)
        {
            m_skinnedMeshContainer->UpdateAnimation(m_runTime, m_useOutOfSyncBoneAnimation);
        }
        if (m_drawBones)
        {
            m_skinnedMeshContainer->DrawBones();
        }
    }

    void SkinnedMeshExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }
        SkinnedMeshConfig config = m_skinnedMeshContainer->GetSkinnedMeshConfig();
        bool configWasModified = false;

        // Imgui limits slider range to half the natural range of the type
        float segmentCountFloat = static_cast<float>(config.m_segmentCount);
        configWasModified |= ScriptableImGui::SliderFloat("Segments Per-Mesh", &segmentCountFloat, 2.0f, 2048.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        configWasModified |= ScriptableImGui::SliderInt("Vertices Per-Segment", &config.m_verticesPerSegment, 4, 2048);
        configWasModified |= ScriptableImGui::SliderInt("Bones Per-Mesh", &config.m_boneCount, 2, 256);
        configWasModified |= ScriptableImGui::SliderInt("Influences Per-Vertex", &config.m_influencesPerVertex, 1, 4);

        if (configWasModified)
        {
            config.m_segmentCount = static_cast<int>(segmentCountFloat);
            m_skinnedMeshContainer->SetSkinnedMeshConfig(config);
        }

        bool animationWasModified = configWasModified;
        animationWasModified |= ScriptableImGui::Checkbox("Use Fixed Animation Time", &m_useFixedTime);
        animationWasModified |= ScriptableImGui::SliderFloat("Fixed Animation Time", &m_fixedAnimationTime, 0.0f, 20.0f);
        animationWasModified |= ScriptableImGui::Checkbox("Use Out of Sync Bone Animation", &m_useOutOfSyncBoneAnimation);

        if (m_useFixedTime && animationWasModified)
        {
            m_skinnedMeshContainer->UpdateAnimation(m_fixedAnimationTime, m_useOutOfSyncBoneAnimation);
        }

        ScriptableImGui::Checkbox("Draw bones", &m_drawBones);

        m_imguiSidebar.End();
    }

    void SkinnedMeshExampleComponent::CreateSkinnedMeshContainer()
    {
        const AZ::RPI::Scene* scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        const auto skinnedMeshFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::SkinnedMeshFeatureProcessorInterface>();
        const auto meshFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::MeshFeatureProcessorInterface>();

        // Default settings for the sample
        SkinnedMeshConfig config;
        config.m_segmentCount = 10;
        config.m_verticesPerSegment = 7;
        config.m_boneCount = 5;
        config.m_influencesPerVertex = 3;
        m_skinnedMeshContainer = AZStd::make_unique<SkinnedMeshContainer>(skinnedMeshFeatureProcessor, meshFeatureProcessor, config);
    }
}
