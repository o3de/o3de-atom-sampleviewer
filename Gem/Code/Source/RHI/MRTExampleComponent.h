/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/ScopeProducer.h>

#include <Atom/RPI.Public/Image/AttachmentImage.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>

#include <AzCore/Component/Component.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
   //! Multiple Render Targets testing, using 2 scopes
   //! First scope render to 3 render targets with different R, G, B values
   //! Second scope merge the result from 3 render targets and render to screen
    class MRTExampleComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(MRTExampleComponent, "{A18A98CB-1274-474F-81CC-E9238B3AD0AF}", AZ::Component);
        AZ_DISABLE_COPY(MRTExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        MRTExampleComponent();
        ~MRTExampleComponent() = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        void CreateInputAssemblyBuffersAndViews();
        void InitRenderTargets();
        void CreateRenderTargetScope();
        void CreateScreenScope();

        // init pipeline state
        void ReadRenderTargetShader();
        void ReadScreenShader();
        
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZStd::array<AZ::RHI::TransientImageDescriptor, 3> m_renderTargetImageDescriptors;

        AZ::RHI::InputStreamLayout m_inputStreamLayout;
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>,2> m_pipelineStates;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, 2> m_shaderResourceGroups;

        AZStd::array<AZ::RHI::ShaderInputConstantIndex, 3> m_shaderInputConstantIndices;
        AZStd::array<AZ::RHI::ShaderInputImageIndex, 3> m_shaderInputImageIndices;

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;

        AZStd::array<AZ::RHI::AttachmentId, 3> m_attachmentID;
        AZ::RHI::ClearValue m_clearValue;
        float m_time;
    };
}
