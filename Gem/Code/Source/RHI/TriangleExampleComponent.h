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
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/BufferPool.h>

#include <AzCore/Math/Matrix4x4.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    class TriangleExampleComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(TriangleExampleComponent, "{4807DFB6-4530-4D37-AF26-E8A4C130F7DC}", AZ::Component);
        AZ_DISABLE_COPY(TriangleExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        TriangleExampleComponent();
        ~TriangleExampleComponent() override = default;

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;


        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;
        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;

        struct BufferData
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
    };
} // namespace AtomSampleViewer
