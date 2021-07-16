/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Input/Events/InputChannelEventListener.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RHI.Reflect/MultisampleState.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ScopeId.h>

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/StreamBufferView.h>

#include <RHI/BasicRHIComponent.h>

namespace AZ
{
    namespace RHI
    {
        class FrameGraphBuilder;
    }
}

namespace AtomSampleViewer
{
    // MSAA RHI example.
    // Test rendering a triangle to a MSAA texture using different settings and resolving it to non MSAA swapchan image for presentation.
    // The example cycles through the following modes when detecting a mouse click:
    // 1 - 2xMSAA
    // 2 - 4xMSAA
    // 3 - 4xMSAA with custom sample positions
    // 4 - 4xMSAA with custom resolve using a shader
    // 5 - No MSAA
    class MSAAExampleComponent final
        : public BasicRHIComponent
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT(MSAAExampleComponent, "{3F35D60E-4EB5-4319-8925-27BE0EC2B7E0}", AZ::Component);
        AZ_DISABLE_COPY(MSAAExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        MSAAExampleComponent();
        ~MSAAExampleComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        void CreateTriangleResources();
        void CreateQuadResources();

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        enum class MSAAType : uint32_t
        {
            MSAA2X = 0,
            MSAA4X,
            MSAA4X_Custom_Positions,
            MSAA4X_Custom_Resolve,
            NoAA,
            Count
        };

        static const uint32_t s_numMSAAExamples = static_cast<uint32_t>(MSAAType::Count);

        struct SampleProperties
        {
            uint32_t m_samplesCount;
            bool m_resolve;
            AZStd::vector<AZ::RHI::SamplePosition> m_customPositions;
            AZ::RHI::AttachmentStoreAction m_storeAction;
            AZ::RHI::AttachmentId m_attachmentId;
            bool m_isTransient;
        };

        void OnMSAATypeChanged(MSAAType type);

        void CreateScopeResources(MSAAType type);
        void CreateScopeProducer(MSAAType type);
        void CreateCustomMSAAResolveResources();
        void CreateCustomMSAAResolveScope();


        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_triangleInputAssemblyBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_quadInputAssemblyBuffer;
        
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, s_numMSAAExamples> m_pipelineStates;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_customResolveMSAAPipelineState;

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_triangleShaderResourceGroup;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_customMSAAResolveShaderResourceGroup;

        AZ::Data::Instance<AZ::RPI::Shader> m_triangleShader;
        AZ::Data::Instance<AZ::RPI::Shader> m_customMSAAResolveShader;

        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;
        AZ::RHI::ShaderInputImageIndex m_customMSAAResolveTextureInputIndex;

        struct TriangleBufferData
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        struct QuadBufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_triangleStreamBufferViews;
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_quadStreamBufferViews;
        AZ::RHI::InputStreamLayout m_triangleInputStreamLayout;
        AZ::RHI::InputStreamLayout m_quadInputStreamLayout;

        AZStd::array<SampleProperties, s_numMSAAExamples> m_sampleProperties;
        AZStd::array<AZStd::vector<AZStd::shared_ptr<AZ::RHI::ScopeProducer>>, s_numMSAAExamples> m_MSAAScopeProducers;

        MSAAType m_currentType = MSAAType::MSAA2X;
    };
} // namespace AtomSampleViewer
