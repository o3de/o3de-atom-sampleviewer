/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ShadingExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/Feature/LuxCore/LuxCoreBus.h>

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
    const char* ShadingExampleComponent::CameraControllerNameTable[CameraControllerCount] =
    {
        "ArcBall",
        "NoClip"
    };

    void ShadingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class < ShadingExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    ShadingExampleComponent::ShadingExampleComponent()
    {
    }

    void ShadingExampleComponent::Activate()
    {
        AZ::RPI::AssetUtils::TraceLevel traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        auto meshAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/shaderball_simple.azmodel", traceLevel);
        auto materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(DefaultPbrMaterialPath, traceLevel);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ meshAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, AZ::Transform::CreateIdentity());

        UseArcBallCameraController();

        InitLightingPresets(true);
        
        // Add an Image based light.
        m_defaultIbl.Init(AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get());

        // Load default IBL texture asset for LuxCore
        // We should be able to extract this information from SkyBox in the future
        LoadIBLImage("textures/sampleenvironment/example_iblskyboxcm.dds.streamingimage");

        AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::SetCameraEntityID, GetCameraEntityId());
        AzFramework::InputChannelEventListener::Connect();
        AZ::TickBus::Handler::BusConnect();
    }

    void ShadingExampleComponent::Deactivate()
    {
        RemoveController();
        m_defaultIbl.Reset();
        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        AzFramework::InputChannelEventListener::Disconnect();
        AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::ClearLuxCore);
        AZ::TickBus::Handler::BusDisconnect();
    }

    void ShadingExampleComponent::UseArcBallCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
    }

    void ShadingExampleComponent::UseNoClipCameraController()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
    }

    void ShadingExampleComponent::LoadIBLImage(const char* imagePath)
    {
        auto imageAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>
            (imagePath, AZ::RPI::AssetUtils::TraceLevel::Assert);

        // FindOrCreate will synchronously load the image if necessary.
        IBLImage = AZ::RPI::StreamingImage::FindOrCreate(imageAsset);
        AZ_Assert(IBLImage != nullptr, "Failed to find or create an image instance from image asset");
    }

    bool ShadingExampleComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
        switch (inputChannel.GetState())
        {
            case AzFramework::InputChannel::State::Ended:
            {
                // L key pressed
                if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericL)
                {
                    if (!m_renderSent)
                    {
                        m_renderSent = true;
                    }
                }
            }
            default:
            {
                break;
            }
        }
        return false;
    }
    
    void ShadingExampleComponent::RemoveController()
    {

    }

    void ShadingExampleComponent::SetArcBallControllerParams()
    {

    }

    void ShadingExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(time);
        
        if (m_renderSent)
        {
            if (!m_dataLoaded)
            {
                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::ClearLuxCore);

                AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = GetMeshFeatureProcessor()->GetModel(m_meshHandle)->GetModelAsset();
                AZ::Data::Instance<AZ::RPI::Material> materialInstance = GetMeshFeatureProcessor()->GetMaterialAssignmentMap(m_meshHandle).at(AZ::Render::DefaultMaterialAssignmentId).m_materialInstance;

                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::AddMesh, modelAsset);
                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::AddMaterial, materialInstance);
                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::AddObject, modelAsset, materialInstance->GetId());

                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::AddTexture, IBLImage, AZ::Render::LuxCoreTextureType::IBL);

                m_dataLoaded = true;
            }

            // wait till texture ready
            AZ::Render::LuxCoreRequestsBus::BroadcastResult(m_textureReady, &AZ::Render::LuxCoreRequestsBus::Events::CheckTextureStatus);
            if (m_textureReady)
            {
                AZ::Render::LuxCoreRequestsBus::Broadcast(&AZ::Render::LuxCoreRequestsBus::Events::RenderInLuxCore);

                m_renderSent = false;
                m_dataLoaded = false;
            }
        }
    }
}
