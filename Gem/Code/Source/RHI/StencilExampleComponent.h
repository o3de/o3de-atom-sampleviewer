/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <RHI/BasicRHIComponent.h>

#include <AzCore/Math/Vector3.h>

#include <Atom/RHI/Device.h>
#include <Atom/RHI/DeviceBufferPool.h>
#include <Atom/RHI/DeviceDrawItem.h>
#include <Atom/RHI/DeviceImagePool.h>
#include <Atom/RHI/DevicePipelineState.h>

namespace AtomSampleViewer
{
    /*
    * Testing different Stencil Comparison Function
    * First render all color triangles with stencil set to 1
    * Then render white triangles with different functions
    * From top left to bottom right :
    * Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
    */
    class StencilExampleComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(StencilExampleComponent, "{30979B47-9B6C-49D5-BA4D-A88452247D9F}", AZ::Component);
        AZ_DISABLE_COPY(StencilExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        StencilExampleComponent();
        ~StencilExampleComponent() override = default;

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // Triangles setting
        void SetTriangleVertices(int startIndex, VertexPosition* vertexData, AZ::Vector3 center, float offset);

        AZ::RHI::Ptr<AZ::RHI::DeviceBufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::DeviceBuffer> m_inputAssemblyBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::DevicePipelineState> m_pipelineStateBasePass;
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::DevicePipelineState>, 8> m_pipelineStateStencil;

        static const size_t s_numberOfVertices = 48;

        struct BufferData
        {
            AZStd::array<VertexPosition, s_numberOfVertices> m_positions;
            AZStd::array<VertexColor, s_numberOfVertices> m_colors;
            AZStd::array<uint16_t, s_numberOfVertices> m_indices;
        };

        AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::DeviceDrawItem m_drawItem;

        AZ::RHI::AttachmentId m_depthStencilID;
        AZ::RHI::ClearValue m_depthClearValue;
    };
}
