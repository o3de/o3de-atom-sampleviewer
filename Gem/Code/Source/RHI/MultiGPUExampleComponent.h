/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/ImagePool.h>
#include <Atom/RHI/Image.h>
#include <Atom/RHI/ShaderResourceGroupPool.h>
#include <Atom/RHI/CopyItem.h>

#include <AzCore/Math/Matrix4x4.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    // MultiGPU RHI example.
    // Renders a rotating triangle to the screen similar to the TriangleExampleComponent, except, the left half of the screen is rendered by
    // GPU 0 and the right half by GPU 1, which is then copied to GPU 0 to composite and show the final output on GPU 0.
    // At least two devices need to be initialized (by passing "--device-count 2") to run this example.
    class MultiGPUExampleComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(MultiGPUExampleComponent, "{BBA75A38-F111-4F52-AD5E-334B6DD58827}", AZ::Component);
        AZ_DISABLE_COPY(MultiGPUExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        MultiGPUExampleComponent();
        ~MultiGPUExampleComponent() override = default;

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        void FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

    private:
        /////////////////////////////////////////////////////////////////////////
        //! Shared Resources

        void CreateRenderScopeProducer();

        AZ::RHI::MultiDevice::DeviceMask m_deviceMask;
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroupShared;
        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;

        struct BufferDataTrianglePass
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;

        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_imagePool{};
        AZ::RHI::Ptr<AZ::RHI::Image> m_image{};
        AZStd::array<AZ::RHI::AttachmentId, 2> m_imageAttachmentIds = { { AZ::RHI::AttachmentId("MultiGPURenderTexture1"),
                                                                          AZ::RHI::AttachmentId("MultiGPURenderTexture2") } };
        AZStd::array<AZ::RHI::AttachmentId, 2> m_bufferAttachmentIds = { { AZ::RHI::AttachmentId("MultiGPUBufferToGPU"),
                                                                          AZ::RHI::AttachmentId("MultiGPUBufferToCPU") } };
        uint32_t m_imageWidth{0};
        uint32_t m_imageHeight{0};

        /////////////////////////////////////////////////////////////////////////
        //! First device methods and members

        void CreateCopyToGPUScopeProducer();
        void CreateCopyToCPUScopeProducer();
        void CreateCompositeScopeProducer();

        struct BufferDataCompositePass
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZStd::array<AZ::RHI::ShaderInputImageIndex, 2> m_textureInputIndices;

        AZ::RHI::Ptr<AZ::RHI::Device> m_device_1{};
        AZ::RHI::MultiDevice::DeviceMask m_deviceMask_1{};
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_stagingBufferPoolToGPU{};
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_stagingBufferToGPU{};
        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_transferImagePool{};
        AZ::RHI::Ptr<AZ::RHI::Image> m_transferImage{};
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPoolComposite{};
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBufferComposite{};
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViewsComposite;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineStateComposite;
        AZ::RHI::Ptr<AZ::RHI::ShaderResourceGroupPool> m_shaderResourceGroupPoolComposite;
        AZ::RHI::Ptr<AZ::RHI::ShaderResourceGroup> m_shaderResourceGroupComposite;
        AZ::RHI::ShaderResourceGroupData m_shaderResourceGroupDataComposite;
        AZStd::array<AZ::RHI::Scissor, 2> m_scissors{};

        /////////////////////////////////////////////////////////////////////////
        //! Second device methods and members

        AZ::RHI::Ptr<AZ::RHI::Device> m_device_2{};
        AZ::RHI::MultiDevice::DeviceMask m_deviceMask_2{};
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_stagingBufferPoolToCPU{};
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_stagingBufferToCPU{};
        AZStd::vector<AZStd::shared_ptr<AZ::RHI::ScopeProducer>> m_secondaryScopeProducers;
    };
} // namespace AtomSampleViewer
