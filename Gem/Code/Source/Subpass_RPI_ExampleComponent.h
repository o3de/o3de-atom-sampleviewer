/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/EntityBus.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>

#include <Atom/Bootstrap/DefaultWindowBus.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>

#include <CommonSampleComponentBase.h>

namespace AtomSampleViewer
{
    //! This example demonstrates how to use Subpasses at the RPI level.
    //! The are two Render Pipelines that are identical in terms of expected output,
    //! but they work diffrently to achive the same outcome.
    //! The first (default) pipeline is made of Two Subpasses, Forward followed by SkyBox.
    //! The second pipeline is made of Two Passes, Forward followed by SkyBox.
    //! The user can switch between those two pipelines by using the Keys '1' or '2'.
    class Subpass_RPI_ExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::Render::Bootstrap::DefaultWindowNotificationBus::Handler
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT(Subpass_RPI_ExampleComponent, "{6AD413AE-161D-4A8B-99B3-58E02277F2F0}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static constexpr char LogName[] = "Subpass_RPI_Example";

        Subpass_RPI_ExampleComponent();
        ~Subpass_RPI_ExampleComponent() override = default;

        //! AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        //! AZ::EntityBus::MultiHandler
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        //! AzFramework::InputChannelEventListener
        //! In this example we use keyboard events for the digit keys to allow
        //! the user to change the active pipeline.
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        //! Must be called before ActivateModel();
        void ActivateGroundPlane();
        void ActivateModel();

        void UpdateGroundPlane();

        void UseArcBallCameraController();
        void UseNoClipCameraController();
        void RemoveController();

        void SetArcBallControllerParams();
        void ResetCameraController();

        void DefaultWindowCreated() override;

        enum class AvailablePipelines : size_t
        {
            TwoSubpassesPipeline,
            TwoPassesPipeline,
            Count
        };

        void ChangeActivePipeline(AvailablePipelines pipelineOption);
        void RestoreOriginalPipeline();

        static constexpr char DefaultMaterialPath[] = "objects/hermanubis/hermanubis_stone.azmaterial";
        static constexpr char DefaultModelPath[] = "materialeditor/viewportmodels/hermanubis.fbx.azmodel";
        static constexpr char DefaultGroundPlaneModelPath[] = "objects/plane.fbx.azmodel";

        AvailablePipelines m_activePipelineOption = AvailablePipelines::TwoSubpassesPipeline;

        //! Original render pipeline when the sample was started.
        //! This sample only goes back to this pipeline when it is closed/deactivated.
        AZ::RPI::RenderPipelinePtr m_originalPipeline = nullptr;

        //! The active pipeline from this sample. It can be one of:
        //! 1- TwoSubpassesPipeline (default): One Vulkan Render Pass that contains two Subpasses.
        //!    Manually activated by the user when pressing the '1' keyboard key.
        //! 2- TwoPassesPipeline: Two Vulkan Render Passes.
        //!    Manually activated by the user when pressing the '2' keyboard key.
        AZ::RPI::RenderPipelinePtr m_activePipeline = nullptr;

        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZ::Render::ImGuiActiveContextScope m_imguiScope;

        enum class CameraControllerType : int32_t
        {
            ArcBall = 0,
            NoClip,
            Count
        };
        static const uint32_t CameraControllerCount = static_cast<uint32_t>(CameraControllerType::Count);
        static const char* CameraControllerNameTable[CameraControllerCount];
        CameraControllerType m_currentCameraControllerType = CameraControllerType::ArcBall;

        static constexpr float ArcballRadiusMinModifier = 0.01f;
        static constexpr float ArcballRadiusMaxModifier = 4.0f;
        static constexpr float ArcballRadiusDefaultModifier = 2.0f;

        bool m_cameraControllerDisabled = false;

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;

        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_groundPlandMeshHandle;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_groundPlaneModelAsset;

    };
} // namespace AtomSampleViewer
