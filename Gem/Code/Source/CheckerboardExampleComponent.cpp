/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <CheckerboardExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void CheckerboardExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < CheckerboardExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    CheckerboardExampleComponent::CheckerboardExampleComponent()
    {
    }

    void CheckerboardExampleComponent::Activate()
    {
        AZ::RPI::AssetUtils::TraceLevel traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        auto meshAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/shaderball_simple.azmodel", traceLevel);
        auto materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultPbrMaterialPath, traceLevel);

        AZ::Render::MaterialAssignmentMap materials;
        AZ::Render::MaterialAssignment& defaultMaterial = materials[AZ::Render::DefaultMaterialAssignmentId];
        defaultMaterial.m_materialAsset = materialAsset;
        defaultMaterial.m_materialInstance = AZ::RPI::Material::FindOrCreate(defaultMaterial.m_materialAsset);

        AZ::RPI::ScenePtr scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_meshFeatureProcessor = scene->GetFeatureProcessor<AZ::Render::MeshFeatureProcessorInterface>();
        m_meshHandle = m_meshFeatureProcessor->AcquireMesh(AZ::Render::MeshHandleDescriptor{ meshAsset }, materials);
        m_meshFeatureProcessor->SetTransform(m_meshHandle, AZ::Transform::CreateIdentity());
        
        AZ::Debug::CameraControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());

        // Add an Image based light.
        m_defaultIbl.Init(scene.get());


        AZ::TickBus::Handler::BusConnect();

        // connect to the bus before creating new pipeline
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        ActivateCheckerboardPipeline();

        // Create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass(AZ::RPI::PassHierarchyFilter({ "CheckerboardPipeline", "ImGuiPass" }));
        m_imguiSidebar.Activate();
    }

    void CheckerboardExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        m_imguiScope = {}; // restores previous ImGui context.

        DeactivateCheckerboardPipeline();

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();

        AZ::TickBus::Handler::BusDisconnect();

        m_defaultIbl.Reset();

        m_meshFeatureProcessor->ReleaseMesh(m_meshHandle);
        m_meshFeatureProcessor = nullptr;
    }
    
    bool CheckerboardExampleComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        auto config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        AZ_Assert(config && config->IsValid(), "SampleComponentConfig required for sample component configuration.");
        m_cameraEntityId = config->m_cameraEntityId;
        m_entityContextId = config->m_entityContextId;
        return true;
    }

    void CheckerboardExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext,
            &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void CheckerboardExampleComponent::ActivateCheckerboardPipeline()
    {        
        // save original render pipeline first and remove it from the scene
        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_originalPipeline = defaultScene->GetDefaultRenderPipeline();
        defaultScene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // add the checker board pipeline
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = "Checkerboard";
        pipelineDesc.m_rootPassTemplate = "CheckerboardPipeline";
        m_cbPipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);
        defaultScene->AddRenderPipeline(m_cbPipeline);
        m_cbPipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
    }

    void CheckerboardExampleComponent::DeactivateCheckerboardPipeline()
    {
        // remove cb pipeline before adding original pipeline.
        AZ::RPI::ScenePtr defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        defaultScene->RemoveRenderPipeline(m_cbPipeline->GetId());

        defaultScene->AddRenderPipeline(m_originalPipeline);

        m_cbPipeline = nullptr;
    }

    void CheckerboardExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawSidebar();
    }

    void CheckerboardExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Text("Checkerboard debug render");

        m_imguiSidebar.End();
    }
}
