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
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RHI.Reflect/Format.h>
#include <Atom/RHI.Reflect/MultisampleState.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ScopeId.h>

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/StreamBufferView.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    // Alpha To Coverage example.
    // Draw 2 overlapping rectangles with alpha tested texture with the followings 3 ways, from left to right:
    // - Src Over
    // - Alpha Testing
    // - Alpha To Coverage (4xMSAA)

    class AlphaToCoverageExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AlphaToCoverageExampleComponent, "{202755F5-443F-4877-ACE7-0A5D0C8414E6}", AZ::Component);
        AZ_DISABLE_COPY(AlphaToCoverageExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        AlphaToCoverageExampleComponent();
        ~AlphaToCoverageExampleComponent() override = default;

    protected:
        struct ScopeData
        {
        };

        enum class BlendType : uint32_t
        {
            SrcOver = 0,
            AlphaTest, 
            A2C_MSAA,
            Count
        };
        static const uint32_t s_numBlendTypes = static_cast<uint32_t>(BlendType::Count);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        void CreateResources();

        void CreateScopeResources(BlendType type);
        void CreateRectangleScopeProducer(BlendType type);

        static const uint32_t s_numRectangles = 2; // number of rectangles to overlap for each blend type
        static const AZ::RHI::Format s_depthFormat = AZ::RHI::Format::D32_FLOAT;
        static const uint32_t s_sampleCount = 4; // sample count for A2C_MSAA

        float m_time = 0.f;
        static constexpr float m_fieldOfView = AZ::Constants::QuarterPi;
        static constexpr float m_targetDepth = 1.f;

        // Input Assembly buffer and its Stream/Index Views
        struct RectangleBufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_rectangleInputAssemblyBuffer;
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_rectangleStreamBufferViews;
        AZ::RHI::InputStreamLayout m_rectangleInputStreamLayout;

        // Shader Resource
        AZ::RHI::ShaderInputNameIndex m_worldMatrixIndex = "m_worldMatrix";
        AZ::RHI::ShaderInputNameIndex m_viewProjectionMatrixIndex = "m_viewProjectionMatrix";
        AZ::RHI::ShaderInputNameIndex m_alphaTestRefValueIndex = "m_alphaTestRefValue";
        AZ::RHI::AttachmentId m_depthImageAttachmentId, m_multisamleDepthImageAttachmentId;
        AZ::RHI::AttachmentId m_resolveImageAttachmentId;
        AZStd::array<AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numRectangles>, s_numBlendTypes> m_shaderResourceGroups;

        // Pipeline State and Shader
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, s_numBlendTypes> m_pipelineStates;
        AZ::Data::Instance<AZ::RPI::Shader> m_shader;
    };
} // namespace AtomSampleViewer
