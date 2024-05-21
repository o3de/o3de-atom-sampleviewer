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
#include <Atom/RHI/MultiDeviceDrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/MultiDevicePipelineState.h>
#include <Atom/RHI/MultiDeviceBufferPool.h>
#include <Atom/RHI/MultiDeviceImagePool.h>
#include <Atom/RHI/MultiDeviceImage.h>
#include <Atom/RHI/MultiDeviceShaderResourceGroupPool.h>
#include <Atom/RHI/MultiDeviceCopyItem.h>

#include <AzCore/Math/Matrix4x4.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
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
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer> m_inputAssemblyBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::MultiDevicePipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroupShared;
        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;

        struct BufferDataTrianglePass
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        AZStd::array<AZ::RHI::MultiDeviceStreamBufferView, 2> m_streamBufferViews;

        AZ::RHI::Ptr<AZ::RHI::MultiDeviceImagePool> m_imagePool{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceImage> m_image{};
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
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferPool> m_stagingBufferPoolToGPU{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer> m_stagingBufferToGPU{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceImagePool> m_transferImagePool{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceImage> m_transferImage{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferPool> m_inputAssemblyBufferPoolComposite{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer> m_inputAssemblyBufferComposite{};
        AZStd::array<AZ::RHI::MultiDeviceStreamBufferView, 2> m_streamBufferViewsComposite;
        AZ::RHI::ConstPtr<AZ::RHI::MultiDevicePipelineState> m_pipelineStateComposite;
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceShaderResourceGroupPool> m_shaderResourceGroupPoolComposite;
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceShaderResourceGroup> m_shaderResourceGroupComposite;
        AZ::RHI::MultiDeviceShaderResourceGroupData m_shaderResourceGroupDataComposite;
        AZStd::array<AZ::RHI::Scissor, 2> m_scissors{};

        /////////////////////////////////////////////////////////////////////////
        //! Second device methods and members

        AZ::RHI::Ptr<AZ::RHI::Device> m_device_2{};
        AZ::RHI::MultiDevice::DeviceMask m_deviceMask_2{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBufferPool> m_stagingBufferPoolToCPU{};
        AZ::RHI::Ptr<AZ::RHI::MultiDeviceBuffer> m_stagingBufferToCPU{};
        AZStd::vector<AZStd::shared_ptr<AZ::RHI::ScopeProducer>> m_secondaryScopeProducers;
    };
} // namespace AtomSampleViewer
