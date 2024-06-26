/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/PipelineState.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    
   //! Demonstrates queued uploads of data to the GPU while something is rendering. 
   //! In effect this tests the AsyncUploadQueue class in the RHI back-end implementations.
   //! The expected output is a textured quad where the texture is frequently replaced and the 
   //! position of the quad frequently changes.
    class CopyQueueComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(CopyQueueComponent, "{581AB2F2-C969-4572-9B40-4EE13D862C72}", AZ::Component);
        AZ_DISABLE_COPY(CopyQueueComponent);

        static void Reflect(AZ::ReflectContext* context);

        CopyQueueComponent();
        ~CopyQueueComponent() override = default;

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        void UploadTextureAsAsset(const char* filePath, int index);

        /// Updates the content of the vertex position buffer to animated based on a time value
        void UpdateVertexPositions(float timeValue);
        
        /// Uploads the vertex position buffer to the GPU
        void UploadVertexBuffer();

        AZ::RHI::ShaderInputImageIndex m_textureInputIndex;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_indexBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_positionBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_uvBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;

        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;

        /// Tracks state used for 'animating' the textured quad to adjust its size and the texture in use.
        struct ProcessingState
        {
            static constexpr float ChangeDelay = 0.5f;
            static constexpr int TextureCount = 3;
            static constexpr float TickAmount = 0.005f;

            float m_time = 0.0f;
            float m_timeUntilChange = 0.0f;
            int m_changeCount = 0;
        };

        ProcessingState m_processingState;

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        BufferData m_bufferData;

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;

        static const int numberOfPaths = 3;
        const char* m_filePaths[numberOfPaths] = {
            "textures/streaming/streaming1.dds.streamingimage",
            "textures/streaming/streaming2.dds.streamingimage",
            "textures/streaming/streaming3.dds.streamingimage",
        };
        AZStd::array<AZ::Data::Instance<AZ::RPI::StreamingImage>, 3> m_images;
    };
} // namespace AtomSampleViewer
#pragma once
