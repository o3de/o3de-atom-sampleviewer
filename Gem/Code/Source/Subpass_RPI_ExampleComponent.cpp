/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Subpass_RPI_ExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

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
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    const char* Subpass_RPI_ExampleComponent::CameraControllerNameTable[CameraControllerCount] =
    {
        "ArcBall",
        "NoClip"
    };

    void Subpass_RPI_ExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Subpass_RPI_ExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    Subpass_RPI_ExampleComponent::Subpass_RPI_ExampleComponent()
    {
    }

    void Subpass_RPI_ExampleComponent::DefaultWindowCreated()
    {
        AZ::Render::Bootstrap::DefaultWindowBus::BroadcastResult(m_windowContext, &AZ::Render::Bootstrap::DefaultWindowBus::Events::GetDefaultWindowContext);
    }

    void Subpass_RPI_ExampleComponent::ChangeActivePipeline(AvailablePipelines pipelineOption)
    {
        if ((pipelineOption == m_activePipelineOption) && (m_activePipeline != nullptr))
        {
            return;
        }

        // remove currently running sample pipeline, if any
        bool removeOriginalPipeline = true;
        if (m_activePipeline)
        {
            m_scene->RemoveRenderPipeline(m_activePipeline->GetId());
            m_activePipeline = nullptr;
            removeOriginalPipeline = false;
        }

        // Create the pipeline.
        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_mainViewTagName = "MainCamera";
        pipelineDesc.m_materialPipelineTag = "MultiViewPipeline";
        pipelineDesc.m_name = "SubpassesPipeline";
        pipelineDesc.m_rootPassTemplate = "SubpassesPipelineTemplate";
        pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 1;
        pipelineDesc.m_allowSubpassMerging = pipelineOption == AvailablePipelines::TwoSubpassesPipeline;

        m_activePipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);
        AZ_Assert(
            m_activePipeline != nullptr, "Failed to create render pipeline with name='%s' and template='%s'.", pipelineDesc.m_name.c_str(),
            pipelineDesc.m_rootPassTemplate.c_str());

        // Activate the pipeline
        m_activePipeline->GetRootPass()->SetEnabled(true); // PassSystem::RemoveRenderPipeline was calling SetEnabled(false)
        m_scene->AddRenderPipeline(m_activePipeline);
        m_activePipeline->SetDefaultView(m_originalPipeline->GetDefaultView());
        if (removeOriginalPipeline)
        {
            m_scene->RemoveRenderPipeline(m_originalPipeline->GetId());
        }

        m_activePipelineOption = pipelineOption;
        AZ_TracePrintf(
            LogName, "New active pipeline is '%s' from template '%s'", pipelineDesc.m_name.c_str(),
            pipelineDesc.m_rootPassTemplate.c_str());
    }

    void Subpass_RPI_ExampleComponent::RestoreOriginalPipeline()
    {
        if (m_activePipeline)
        {
            m_scene->RemoveRenderPipeline(m_activePipeline->GetId());
            m_activePipeline = nullptr;
        }

        m_originalPipeline->GetRootPass()->SetEnabled(true); // PassSystem::RemoveRenderPipeline was calling SetEnabled(false)
        m_scene->AddRenderPipeline(m_originalPipeline);
    }


    void Subpass_RPI_ExampleComponent::Activate()
    {
        UseArcBallCameraController();

        InitLightingPresets(true);

        ActivateGroundPlane();
        ActivateModel();

        // Need to connect to this EBus before creating the pipeline because
        // we need the window context.
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::BusConnect();

        // Save a pointer to the original default pipeline.
        m_originalPipeline = m_scene->GetDefaultRenderPipeline();

        // Start with our default pipeline 
        ChangeActivePipeline(m_activePipelineOption);
    }

    void Subpass_RPI_ExampleComponent::ActivateGroundPlane()
    {
        // Load the material asset first.
        const auto traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        auto groundPlaneMaterialAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultPbrMaterialPath, traceLevel);
        AZ_Assert(groundPlaneMaterialAsset.GetId().IsValid(), "Invalid material asset from path='%s'", DefaultPbrMaterialPath);

        m_groundPlaneModelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(DefaultGroundPlaneModelPath, traceLevel);
        AZ_Assert(m_groundPlaneModelAsset.GetId().IsValid(), "Invalid ground plane model asset from path='%s'", DefaultGroundPlaneModelPath);
        auto meshHandleDescriptor = AZ::Render::MeshHandleDescriptor(m_groundPlaneModelAsset, AZ::RPI::Material::FindOrCreate(groundPlaneMaterialAsset));
        m_groundPlandMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(meshHandleDescriptor);

        GetMeshFeatureProcessor()->SetTransform(m_groundPlandMeshHandle, AZ::Transform::CreateIdentity());
    }

    void Subpass_RPI_ExampleComponent::ActivateModel()
    {
        // Load the material asset first.
        const auto traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        auto materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultMaterialPath, traceLevel);
        AZ_Assert(materialAsset.GetId().IsValid(), "Invalid material asset from path='%s'", DefaultMaterialPath);
        m_modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(DefaultModelPath, traceLevel);
        AZ_Assert(m_modelAsset.GetId().IsValid(), "Invalid model asset from path='%s'", DefaultModelPath);

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        AZ::Render::MeshHandleDescriptor descriptor(m_modelAsset, AZ::RPI::Material::FindOrCreate(materialAsset));
        descriptor.m_modelChangedEventHandler = AZ::Render::MeshHandleDescriptor::ModelChangedEvent::Handler{
            [this](const AZ::Data::Instance<AZ::RPI::Model>& /*model*/)
            {
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

                // This handler will be connected to the feature processor so that when the model is updated, the camera
                // controller will reset. This ensures the camera is a reasonable distance from the model when it resizes.
                ResetCameraController();

                UpdateGroundPlane();
            }
        };

        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(descriptor);
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, AZ::Transform::CreateIdentity());
    }

    void Subpass_RPI_ExampleComponent::Deactivate()
    {
        AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::BusDisconnect();

        GetMeshFeatureProcessor()->ReleaseMesh(m_groundPlandMeshHandle);
        m_groundPlandMeshHandle = {};
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);
        m_modelAsset = {};

        RestoreOriginalPipeline();

        RemoveController();

        ShutdownLightingPresets();
    }

    void Subpass_RPI_ExampleComponent::UpdateGroundPlane()
    {
        if (m_groundPlandMeshHandle.IsValid())
        {
            AZ::Transform groundPlaneTransform = AZ::Transform::CreateIdentity();

            if (m_modelAsset)
            {
                AZ::Vector3 modelCenter;
                float modelRadius;
                m_modelAsset->GetAabb().GetAsSphere(modelCenter, modelRadius);

                static const float GroundPlaneRelativeScale = 4.0f;
                static const float GroundPlaneOffset = 0.01f;

                groundPlaneTransform.SetUniformScale(GroundPlaneRelativeScale * modelRadius);
                groundPlaneTransform.SetTranslation(AZ::Vector3(0.0f, 0.0f, m_modelAsset->GetAabb().GetMin().GetZ() - GroundPlaneOffset));
            }

            GetMeshFeatureProcessor()->SetTransform(m_groundPlandMeshHandle, groundPlaneTransform);
        }
    }

    void Subpass_RPI_ExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);
    }

    // AzFramework::InputChannelEventListener
    bool Subpass_RPI_ExampleComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const auto& inputDevice = inputChannel.GetInputDevice();
        if (!AzFramework::InputDeviceKeyboard::IsKeyboardDevice(inputDevice.GetInputDeviceId()))
        {
            return false;
        }

        const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
        switch (inputChannel.GetState())
        {
        case AzFramework::InputChannel::State::Ended:
            {
                if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::Alphanumeric1)
                {
                    ChangeActivePipeline(AvailablePipelines::TwoSubpassesPipeline);
                }
                else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::Alphanumeric2)
                {
                    ChangeActivePipeline(AvailablePipelines::TwoPassesPipeline);
                }
                else
                {
                    AZ_TracePrintf(LogName, "Invalid Key=%s. Only '1' or '2' are supported\n", inputChannelId.GetName());
                }
                break;
            }
        default:
            {
                break;
            }
        }

        return false;
    }

    void Subpass_RPI_ExampleComponent::UseArcBallCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
    }

    void Subpass_RPI_ExampleComponent::UseNoClipCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
    }

    void Subpass_RPI_ExampleComponent::RemoveController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }

    void Subpass_RPI_ExampleComponent::SetArcBallControllerParams()
    {
        if (!m_modelAsset.IsReady())
        {
            return;
        }

        // Adjust the arc-ball controller so that it has bounds that make sense for the current model
        
        AZ::Vector3 center;
        float radius;
        m_modelAsset->GetAabb().GetAsSphere(center, radius);

        const float startingDistance = radius * ArcballRadiusDefaultModifier;
        const float minDistance = radius * ArcballRadiusMinModifier;
        const float maxDistance = radius * ArcballRadiusMaxModifier;

        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, center);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, startingDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetMinDistance, minDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetMaxDistance, maxDistance);
    }
    void Subpass_RPI_ExampleComponent::ResetCameraController()
    {
        RemoveController();
        if (m_currentCameraControllerType == CameraControllerType::ArcBall)
        {
            UseArcBallCameraController();
            SetArcBallControllerParams();
        }
        else if (m_currentCameraControllerType == CameraControllerType::NoClip)
        {
            UseNoClipCameraController();
        }
    }
} // namespace AtomSampleViewer
