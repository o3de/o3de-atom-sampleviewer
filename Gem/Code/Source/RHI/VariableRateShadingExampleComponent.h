/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/std/containers/array.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/DeviceCopyItem.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/PipelineState.h>

#include <RHI/BasicRHIComponent.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiProgressList.h>
#include <Atom/Utils/AssetCollectionAsyncLoader.h>

namespace AtomSampleViewer
{
    //! This samples demonstrates the use of Variable Rate Shading on the RHI.
    //! Shading rates can be specified in 3 different ways: PerDraw, PerPrimtive and PerRegion.
    //! This samples only uses the PerDraw and PerRegion modes.
    //! The samples render a full screen quad using different shading rates.
    //! When the PerRegion mode is used, an image is generated using a compute shader with different
    //! rates in a circular pattern from the center (or the pointer position).
    //! When a PerDraw mode is used, the rate is applied equally to the whole quad. The rate can be changed
    //! using the GUI of the sample.
    //! Combinator operations are also exposed when both PerDraw and PerRegion are being used.
    class VariableRateShadingExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT(VariableRateShadingExampleComponent, "{B98E1C6A-8C23-4AA4-82E6-4B652F6151DD}", AZ::Component);
        AZ_DISABLE_COPY(VariableRateShadingExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        VariableRateShadingExampleComponent();
        ~VariableRateShadingExampleComponent() override = default;

    private:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        // Draw the ImGUI settings
        void DrawSettings();
        // Loads the compute and graphics shaders.
        void LoadShaders();
        // Creates the image pool and images used for shading rate attachments.
        void CreateShadingRateImage();
        // Creates the shading resource groups used by the compute and graphic scopes.
        void CreateShaderResourceGroups();
        // Creates the IA resources for the full screen quad.
        void CreateInputAssemblyBuffersAndViews();
        // Creates the necessary pipelines.
        void CreatePipelines();
        // Creates the scope used for rendering the full screen quad.
        void CreateRenderScope();
        // Creates the scope for showing the shading rate image.
        void CreatImageDisplayScope();
        // Creates the compute used for updating the shading rate image.
        void CreateComputeScope();

        // ImGUI sidebar that handles the options of the sample.
        ImGuiSidebar m_imguiSidebar;
        // Whether to use a shading rate image.
        bool m_useImageShadingRate = true;
        // Whether to show the shading rate image.
        bool m_showShadingRateImage = false;
        // Whether the center of the shading rate image follows the position of the pointer.
        bool m_followPointer = false;
        // Whether to use the PerDraw mode.
        bool m_useDrawShadingRate = false;
        // Combinator operation to applied between the PerDraw and PerRegion shading rate.
        AZ::RHI::ShadingRateCombinerOp m_combinerOp = AZ::RHI::ShadingRateCombinerOp::Passthrough;
        // Shading rate when using the PerDraw mode.
        AZ::RHI::ShadingRate m_shadingRate = AZ::RHI::ShadingRate::Rate1x1;

        // Pipelines used for rendering the full screen quad with and without a shading rate attachments.
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_modelPipelineState[2];
        // Pipeline used for updating the shading rate image.
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_computePipelineState;
        // Pipeline used for showing the shading rate image.
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_imagePipelineState;
        // Compute and graphics shaders.
        AZStd::vector<AZ::Data::Instance<AZ::RPI::Shader>> m_shaders;
        // SRG with information when rendering the full screen quad.
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_modelShaderResourceGroup;
        // SRG with information when updating the shading rate iamge.
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_computeShaderResourceGroup;
        // SRG with information when displaying the shading rate image.
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_imageShaderResourceGroup;
        
        // Indices into the SRGs for properties that are updated.
        AZ::RHI::ShaderInputConstantIndex m_centerIndex;
        AZ::RHI::ShaderInputImageIndex m_shadingRateIndex;
        AZ::RHI::ShaderInputImageIndex m_shadingRateDisplayIndex;

        // Size of the shading rate image tile (in pixels)
        AZ::Vector2 m_shadingRateImageSize;
        int m_numThreadsX = 8;
        int m_numThreadsY = 8;
        int m_numThreadsZ = 1;

        // Bufferpool for creating the IA buffer
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        // Buffer for the IA of the full screen quad.
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;
        // Bufferviews into the full screen quad IA
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        // Indexview of the full screen quad index buffer
        AZ::RHI::IndexBufferView m_indexBufferView;
        // Layout of the full screen quad.
        AZ::RHI::InputStreamLayout m_inputStreamLayout;

        // Image pool containing the shading rate images.
        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_imagePool;
        // List of shading rate images used as attachments.
        AZStd::fixed_vector<AZ::RHI::Ptr<AZ::RHI::Image>, AZ::RHI::Limits::Device::FrameCountMax> m_shadingRateImages;

        // Cursor position (mouse or touch)
        AZ::Vector2 m_cursorPos;
        // Selected format to be used for the shading rate image.
        AZ::RHI::Format m_rateShadingImageFormat = AZ::RHI::Format::Unknown;
        // Frame counter for selecting the proper shading rate image. 
        uint32_t m_frameCount = 0;
        // List of supported shading rate values.
        AZStd::fixed_vector<AZ::RHI::ShadingRate, static_cast<uint32_t>(AZ::RHI::ShadingRate::Count)> m_supportedModes;
    };
} // namespace AtomSampleViewer
