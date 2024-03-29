/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MSAA_RPI_ExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Shader/ShaderSystemInterface.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Shader/ShaderCommonTypes.h>

#include <Utils/Utils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void MSAA_RPI_ExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < MSAA_RPI_ExampleComponent, AZ::Component>()
                ->Version(0)
            ;
        }
    }

    void MSAA_RPI_ExampleComponent::Activate()
    {
        m_imguiSidebar.Activate();
        AZ::TickBus::Handler::BusConnect();
        ExampleComponentRequestBus::Handler::BusConnect(GetEntityId());
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();

        // save the current render pipeline
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();
        m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());

        // switch to the sample render pipeline
        ChangeRenderPipeline();
  
        // set ArcBall camera controller
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
    
        ActivateModel();
        ActivateIbl();
    }

    void MSAA_RPI_ExampleComponent::Deactivate()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);
        m_defaultIbl.Reset();

        // reset the number of MSAA samples and the RPI scene
        SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::ResetNumMSAASamples);
        SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::ClearRPIScene);

        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();
        ExampleComponentRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
    }

    void MSAA_RPI_ExampleComponent::ChangeRenderPipeline()
    {
        m_imguiScope = {}; // restores previous ImGui context.

        // remove currently running sample pipeline, if any
        if (m_samplePipeline)
        {
            m_scene->RemoveRenderPipeline(m_samplePipeline->GetId());
            m_samplePipeline = nullptr;
        }
        
        bool isNonMsaaPipeline = (m_numSamples == 1);
        
        // if we are changing between the MSAA and non-MSAA pipelines we need to force a reset on the RPI scene.  This is
        // necessary to re-initialize all of the feature processor shaders and Srgs
        if (isNonMsaaPipeline != m_isNonMsaaPipeline)
        {      
            // set the number of MSAA samples and reset the RPI scene
            SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::SetNumMSAASamples, static_cast<uint16_t>(m_numSamples));
            SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::ResetRPIScene);

            // reset internal sample scene related data
            ResetScene();

            // remove the default render pipeline
            AZ::RPI::RenderPipelinePtr defaultPipeline = m_scene->GetDefaultRenderPipeline();
            m_scene->RemoveRenderPipeline(defaultPipeline->GetId());

            // scene IBL is cleared after the reset, re-activate it
            ActivateIbl();

            m_isNonMsaaPipeline = isNonMsaaPipeline;          
        }
        
        // create the new sample pipeline
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_name = "MSAA";
        pipelineDesc.m_rootPassTemplate = "MainPipeline";
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = static_cast<uint16_t>(m_numSamples);
        m_samplePipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);

        // add the sample pipeline to the scene
        m_scene->AddRenderPipeline(m_samplePipeline);
        m_samplePipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
        m_scene->SetDefaultRenderPipeline(m_samplePipeline->GetId());

        // create an ImGuiActiveContextScope to ensure the ImGui context on the new pipeline's ImGui pass is activated.
        m_imguiScope = AZ::Render::ImGuiActiveContextScope::FromPass({ m_samplePipeline->GetId().GetCStr(), "ImGuiPass" });
    }

    void MSAA_RPI_ExampleComponent::ResetCamera()
    {
        const float pitch = -AZ::Constants::QuarterPi - 0.025f;
        const float heading = AZ::Constants::QuarterPi - 0.05f;
        const float distance = 3.0f;
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, AZ::Vector3(0.f));
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, distance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetMaxDistance, 50.0f);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, pitch);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetHeading, heading);
    }

    void MSAA_RPI_ExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext, &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    AZ::Data::Asset<AZ::RPI::MaterialAsset> MSAA_RPI_ExampleComponent::GetMaterialAsset()
    {
        AZ::RPI::AssetUtils::TraceLevel traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        return AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultPbrMaterialPath, traceLevel);
    }

    AZ::Data::Asset<AZ::RPI::ModelAsset> MSAA_RPI_ExampleComponent::GetModelAsset()
    {
        AZ::RPI::AssetUtils::TraceLevel traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        switch (m_modelType)
        {
        case 0:
            return AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cylinder.fbx.azmodel", traceLevel);
        case 1:
            return AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cube.fbx.azmodel", traceLevel);
        case 2:
            return AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/shaderball_simple.fbx.azmodel", traceLevel);
        }

        AZ_Warning("MSAA_RPI_ExampleComponent", false, "Unsupported model type");
        return AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cylinder.fbx.azmodel", traceLevel);
    }

    void MSAA_RPI_ExampleComponent::ActivateModel()
    {
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);

        AZ::Render::MeshHandleDescriptor descriptor(GetModelAsset(), AZ::RPI::Material::FindOrCreate(GetMaterialAsset()));
        descriptor.m_modelChangedEventHandler =
            AZ::Render::MeshHandleDescriptor::ModelChangedEvent::Handler{ [this](const AZ::Data::Instance<AZ::RPI::Model>& model)
                                                                          {
                                                                              OnModelReady(model);
                                                                          } };

        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(descriptor);
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, AZ::Transform::CreateIdentity());
    }

    void MSAA_RPI_ExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = model->GetModelAsset();
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void MSAA_RPI_ExampleComponent::ActivateIbl()
    {
        m_defaultIbl.Init(m_scene);

        // reduce the exposure so the model isn't overly bright
        m_defaultIbl.SetExposure(-0.5f);
    }

    void MSAA_RPI_ExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawSidebar();
    }

    void MSAA_RPI_ExampleComponent::DrawSidebar()
    {
        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        bool refresh = false;
        refresh = DrawSidebarModeChooser(refresh);
        refresh = DrawSideBarModelChooser(refresh);
        m_imguiSidebar.End();

        if (refresh)
        {
            // Note that the model's have some multisample information embedded into their pipeline state, so delete and recreate the model
            GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
            ChangeRenderPipeline();
            ActivateModel();
        }
    }

    bool MSAA_RPI_ExampleComponent::DrawSidebarModeChooser(bool refresh)
    {
        ScriptableImGui::ScopedNameContext context{ "Mode" };

        ImGui::Text("Num Samples");
        refresh |= ScriptableImGui::RadioButton("MSAA None", &m_numSamples, 1);
        refresh |= ScriptableImGui::RadioButton("MSAA 2x", &m_numSamples, 2);
        refresh |= ScriptableImGui::RadioButton("MSAA 4x", &m_numSamples, 4);
        refresh |= ScriptableImGui::RadioButton("MSAA 8x", &m_numSamples, 8);
        return refresh;
    }

    bool MSAA_RPI_ExampleComponent::DrawSideBarModelChooser(bool refresh)
    {
        ScriptableImGui::ScopedNameContext context{ "Model" };

        ImGui::NewLine();
        ImGui::Text("Model");
        refresh |= ScriptableImGui::RadioButton("Cylinder", &m_modelType, 0);
        refresh |= ScriptableImGui::RadioButton("Cube", &m_modelType, 1);
        refresh |= ScriptableImGui::RadioButton("ShaderBall", &m_modelType, 2);
        return refresh;
    }
}
