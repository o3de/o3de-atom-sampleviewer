/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <XRRPIExampleComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>
#include <Utils/Utils.h>

#include <SSRExampleComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    static const float ControllerOffsetScale = 2.0f;
    static const float ViewOrientationScale = 10.0f;
    static const float PixelToDegree = 1.0 / 360.0f;

    void XRRPIExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<XRRPIExampleComponent, AZ::Component>()
                ->Version(0);
        }
    }

    void XRRPIExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        // setup the camera
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 180.f);

        m_xrSystem = AZ::RPI::RPISystemInterface::Get()->GetXRSystem();
        m_numXrViews = m_xrSystem->GetNumViews();

        // create scene
        CreateModels();
        CreateGroundPlane();

        InitLightingPresets(true);
    }

    void XRRPIExampleComponent::Deactivate()
    {
        ShutdownLightingPresets();

        GetMeshFeatureProcessor()->ReleaseMesh(m_statueMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_boxMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_shaderBallMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_groundMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_leftControllerMeshHandle);
        GetMeshFeatureProcessor()->ReleaseMesh(m_rightControllerMeshHandle);

        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        AZ::TickBus::Handler::BusDisconnect();
    }

    void XRRPIExampleComponent::CreateModels()
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
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/cube.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform.SetTranslation(-4.5f, 0.0f, 0.49f);

            m_boxMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_boxMeshHandle, transform);
        }

        // shader ball
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("Materials/Presets/PBR/default_grid.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/ShaderBall_simple.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            transform *= AZ::Transform::CreateRotationZ(AZ::Constants::Pi);
            transform.SetTranslation(4.5f, 0.0f, 0.89f);

            m_shaderBallMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_shaderBallMeshHandle, transform);
        }

        // controller meshes
        {
            AZ::Data::Asset<AZ::RPI::MaterialAsset> materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>("Materials/XR/XR_Hand_Controller_ControlerMAT.azmaterial", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/left_hand_controller.azmodel", AZ::RPI::AssetUtils::TraceLevel::Assert);
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            
            m_leftControllerMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            m_rightControllerMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, AZ::RPI::Material::FindOrCreate(materialAsset));
            GetMeshFeatureProcessor()->SetTransform(m_leftControllerMeshHandle, transform);
            GetMeshFeatureProcessor()->SetTransform(m_rightControllerMeshHandle, transform);
        }
    }

    void XRRPIExampleComponent::CreateGroundPlane()
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
        AZ::Data::Asset<AZ::RPI::ModelAsset> planeModel = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>("objects/plane.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
        m_groundMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ planeModel }, AZ::RPI::Material::FindOrCreate(m_groundMaterialAsset));

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        const AZ::Vector3 nonUniformScale(15.0f, 15.0f, 1.0f);
        GetMeshFeatureProcessor()->SetTransform(m_groundMeshHandle, transform, nonUniformScale);
    }

    void XRRPIExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
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

        if (m_xrSystem && m_xrSystem->ShouldRender())
        {
            AZ::RPI::PoseData frontPoseData;
            AZ::RHI::ResultCode resultCode = m_xrSystem->GetViewLocalPose(frontPoseData);
            for (AZ::u32 i = 0; i < m_numXrViews; i++)
            {
                //Pose data for the controller
                AZ::RPI::PoseData controllerPose;
                resultCode = m_xrSystem->GetControllerPose(i, controllerPose);

                if(resultCode == AZ::RHI::ResultCode::Success && !controllerPose.m_orientation.IsZero())
                { 
                    AZ::Vector3 camPosition;
                    AZ::TransformBus::EventResult(camPosition, GetCameraEntityId(), &AZ::TransformBus::Events::GetWorldTranslation);
                    AZ::Vector3 controllerPositionOffset = controllerPose.m_position * ControllerOffsetScale;
                    AZ::Vector3 newControllerPos = camPosition + AZ::Vector3(controllerPositionOffset.GetX(), -controllerPositionOffset.GetZ(), controllerPositionOffset.GetY());

                    // Go from y up to z up as a right handed coord system
                    AZ::Quaternion controllerOrientation = controllerPose.m_orientation;
                    controllerOrientation.SetX(controllerPose.m_orientation.GetX());
                    controllerOrientation.SetY(-controllerPose.m_orientation.GetZ());
                    controllerOrientation.SetZ(controllerPose.m_orientation.GetY());

                    //Apply a Rotation of 90 deg around X axis in order to orient the model to face away from you as default pose
                    AZ::Transform controllerTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
                                controllerOrientation * AZ::Quaternion::CreateRotationX(-AZ::Constants::Pi / 2), AZ::Vector3(newControllerPos.GetX(), newControllerPos.GetY(),newControllerPos.GetZ()));
                
                    AZ::Render::MeshFeatureProcessorInterface::MeshHandle* controllerMeshhandle = &m_leftControllerMeshHandle;
                    if (i == 1)
                    {
                        controllerMeshhandle = &m_rightControllerMeshHandle;
                    }   
                    GetMeshFeatureProcessor()->SetTransform(*controllerMeshhandle, controllerTransform, AZ::Vector3(m_xrSystem->GetControllerScale(i)));
                }
            }
            
            //Update Camera movement (left, right forward, back) based on left JoyStick controller
            float m_xJoyStickValue = m_xrSystem->GetXJoyStickState(0);
            float m_yJoyStickValue = m_xrSystem->GetYJoyStickState(0);
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateForward, m_yJoyStickValue);
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateBack, m_yJoyStickValue);
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateLeft, m_xJoyStickValue);
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateRight, m_xJoyStickValue);

            //Update Camera movement (Up, Down) based on X,Y button presses on the left controller
            float yButtonState = m_xrSystem->GetYButtonState();
            float xButtonState = m_xrSystem->GetXButtonState();
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateUp, yButtonState);
            AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetCameraStateDown, xButtonState);

            // Switch to updating the view using right joystick controller if the Trigger button on the right controller is pressed
            m_xrSystem->GetTriggerState(1) > 0.1f ? m_rightTriggerButtonPressed = true : m_rightTriggerButtonPressed = false;
            if (m_rightTriggerButtonPressed)
            { 
                //Update Camera view based on right JoyStick controller
                float m_xRightJoyStickValue = m_xrSystem->GetXJoyStickState(1);
                float m_yRightJoyStickValue = m_xrSystem->GetYJoyStickState(1);
                float heading, pitch = 0.0;
                AZ::Debug::NoClipControllerRequestBus::EventResult(heading, GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::GetHeading);
                AZ::Debug::NoClipControllerRequestBus::EventResult(pitch, GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::GetPitch);
                heading -= m_xRightJoyStickValue * PixelToDegree * ViewOrientationScale;
                pitch += m_yRightJoyStickValue * PixelToDegree * ViewOrientationScale;
                pitch = AZStd::max(pitch, -AZ::Constants::HalfPi);
                pitch = AZStd::min(pitch, AZ::Constants::HalfPi);
                AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetHeading, heading);
                AZ::Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::NoClipControllerRequests::SetPitch, pitch);
            }
            else
            {
                for (AZ::u32 i = 0; i < m_numXrViews; i++)
                {
                    //Convert to O3de's coordinate system and update the camera orientation for the correct eye view
                    AZ::Quaternion viewLocalPoseOrientation = frontPoseData.m_orientation;
                    viewLocalPoseOrientation.SetX(-frontPoseData.m_orientation.GetX());
                    viewLocalPoseOrientation.SetY(frontPoseData.m_orientation.GetZ());
                    viewLocalPoseOrientation.SetZ(-frontPoseData.m_orientation.GetY());
                    Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetXRViewQuaternion, viewLocalPoseOrientation, i);
                }
            }

            // Switch to the next lighting preset using the B-button
            if (m_xrSystem->GetBButtonState() > 0.0f)
            {
                if (!m_bButtonPressed)
                {
                    m_bButtonPressed = true;
                    IterateToNextLightingPreset();
                }
            }
            else
            {
                m_bButtonPressed = false;
            }

            // Switch to the next ground floor using the A-button
            if (m_xrSystem->GetAButtonState() > 0.0f)
            {
                if (!m_aButtonPressed)
                {
                    m_aButtonPressed = true;
                    m_groundPlaneMaterial = (m_groundPlaneMaterial + 1) % 4;
                    CreateGroundPlane();
                }
            }
            else
            {
                m_aButtonPressed = false;
            }
        }
    }
}
