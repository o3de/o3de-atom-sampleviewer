/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/TickBus.h>
#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <CommonSampleComponentBase.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>


namespace AtomSampleViewer
{
    //!
    //! This component creates a simple scene that tests VR using a special multi-view VR pipeline. We setup two pipelines, one for each eye and use stereoscopic view for this pipeline.
    //! This sample supports Quest 2 controllers to fly around the world. It also has support to use button presses for specific functionality 
    //! in the scene. The schema for each controller is below
    //! Left controller
    //!         Joystick - Camera movement, Button X - Camera Up (View space y-axis), Button Y - Camera Down (View space Y axis), Squeeze - Scales Controller model     
     //! Right controller
    //!         Joystick - View Orientation if Trigger button is pressed, otherwise it will use device for view tracking, 
    //!         Button B - Iterate through lighting preset, Button B - Iterate through ground plane material, Squeeze - Scales Controller model     
    //!
    class XRRPIExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AtomSampleViewer::XRRPIExampleComponent, "{3122B48E-2553-4568-8B8B-532C105CB83B}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        XRRPIExampleComponent() = default;
        ~XRRPIExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(XRRPIExampleComponent);

        void CreateModels();
        void CreateGroundPlane();
        
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);

        // meshes
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_statueMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_boxMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_shaderBallMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_groundMeshHandle;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_groundMaterialAsset;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_leftControllerMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_rightControllerMeshHandle;

        // ground plane default material setting index
        int m_groundPlaneMaterial = 2;

        // IBL and skybox
        Utils::DefaultIBL m_defaultIbl;
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_skyboxImageAsset;

        bool m_resetCamera = true;
        float m_originalFarClipDistance = 0.0f;

        bool m_xButtonPressed = false;
        bool m_yButtonPressed = false;
        bool m_aButtonPressed = false;
        bool m_bButtonPressed = false;
        bool m_rightTriggerButtonPressed = false;

        AZ::RPI::XRRenderingInterface* m_xrSystem = nullptr;
        AZ::u32 m_numXrViews = 0;
    };
} // namespace AtomSampleViewer
