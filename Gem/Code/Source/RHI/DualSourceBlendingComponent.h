/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Component/TickBus.h>

#include <Atom/RHI/PipelineState.h>
#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
   //! Dual source blending testing
   //! Using dual source blending to implement alpha blending
   //! Showing a color triangle fade in and fade out on success
    class DualSourceBlendingComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(DualSourceBlendingComponent, "{792A6BF1-6FD8-4834-81DE-2B39F1E2A9BB}", AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        DualSourceBlendingComponent();
        ~DualSourceBlendingComponent() override = default;

    protected:
        AZ_DISABLE_COPY(DualSourceBlendingComponent);

        static const char* s_dualSourceBlendingName;
        static const uint32_t geometryVertexCount = 3;
        static const uint32_t geometryIndexCount = 3;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void CreateInputAssemblyBuffersAndViews();
        void LoadRasterShader();
        void CreateRasterScope();

        // -------------------------------------------------
        // Input Assembly buffer and its Streams/Index Views
        // -------------------------------------------------
        struct BufferData
        {
            AZStd::array<VertexPosition, geometryVertexCount> m_positions;
            AZStd::array<VertexColor, geometryVertexCount> m_colors;
            AZStd::array<uint16_t, geometryIndexCount> m_indices;
        };

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;
        AZ::RHI::InputStreamLayout m_inputStreamLayout;

        // ----------------------------------
        // Pipeline and Shader Resource Group
        // ----------------------------------
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance < AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;
        AZ::RHI::ShaderInputConstantIndex m_blendFactorIndex;

        // ------
        // Others
        // ------
        float m_time = 0.0f;

    };
}
