/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/ScopeProducer.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    
    //! Compute shader testing
    //! Testing the functionality of dispatch pipeline
    //! Compute scope do the computation, raster scope render the result to screen
    //! Should show Julia Fractals on success
    class ComputeExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ComputeExampleComponent, "{AA5EB226-A6DC-451F-A771-376A4B35C000}", AZ::Component);
        AZ_DISABLE_COPY(ComputeExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        ComputeExampleComponent();
        ~ComputeExampleComponent() = default;

    protected:
        static const char* s_computeExampleName;
        static const int s_numberOfSRGs = 2;

        int m_numThreadsX = 8;
        int m_numThreadsY = 8;
        int m_numThreadsZ = 1;

        struct ConstantData;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void CreateInputAssemblyBuffersAndViews();
        void CreateComputeBuffer();
        void LoadComputeShader();
        void LoadRasterShader();
        void CreateComputeScope();
        void CreateRasterScope();

        // -------------------------------------------------
        // Input Assembly buffer and its Streams/Index Views
        // -------------------------------------------------
        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;
        AZ::RHI::InputStreamLayout m_inputStreamLayout;

        // ----------------------------
        // Compute Buffer
        // ----------------------------
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_computeBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_computeBuffer;
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_computeBufferView;
        AZ::RHI::BufferViewDescriptor m_bufferViewDescriptor;
        AZ::RHI::AttachmentId m_bufferAttachmentId = AZ::RHI::AttachmentId("bufferAttachmentId");

        // ----------------------
        // Pipeline state and SRG
        // ----------------------

        // Dispatch pipeline
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_dispatchPipelineState;
        AZStd::array< AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numberOfSRGs> m_dispatchSRGs;
        AZ::RHI::ShaderInputConstantIndex m_dispatchDimensionConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_dispatchSeedConstantIndex;

        // Draw pipeline
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_drawPipelineState;
        AZStd::array< AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numberOfSRGs> m_drawSRGs;
        AZ::RHI::ShaderInputConstantIndex m_drawDimensionConstantIndex;

        // ------
        // Others
        // ------
        static const uint32_t m_bufferWidth = 1920;
        static const uint32_t m_bufferHeight = 1080;
        float m_time = 0.0f;
    };
}
