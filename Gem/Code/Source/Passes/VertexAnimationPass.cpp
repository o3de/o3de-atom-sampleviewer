/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "VertexAnimationPass.h"

namespace AZ::Render
{
    RPI::Ptr<VertexAnimationPass> VertexAnimationPass::Create(const RPI::PassDescriptor& descriptor)
    {
        return aznew VertexAnimationPass{ descriptor };
    }

    VertexAnimationPass::VertexAnimationPass(const RPI::PassDescriptor& descriptor)
        : BaseClass{ descriptor }
    {
    }

    void VertexAnimationPass::SetSourceBuffer(Data::Instance<RPI::Buffer> sourceBuffer)
    {
        m_sourceBuffer = sourceBuffer;
    }

    void VertexAnimationPass::SetTargetBuffer(Data::Instance<RPI::Buffer> targetBuffer)
    {
        m_targetBuffer = targetBuffer;
    }

    void VertexAnimationPass::SetInstanceCount(u32 instanceCount)
    {
        m_instanceCount = instanceCount;
    }

    void VertexAnimationPass::SetVertexCountPerInstance(u32 vertexCountPerInstance)
    {
        m_vertexCountPerInstance = vertexCountPerInstance;
    }

    void VertexAnimationPass::SetTargetVertexStridePerInstance(u32 targetVertexStridePerInstance)
    {
        m_targetVertexStridePerInstance = targetVertexStridePerInstance;
    }

    void VertexAnimationPass::BuildInternal()
    {
        AttachBufferToSlot("SourceData", m_sourceBuffer);
        AttachBufferToSlot("TargetData", m_targetBuffer);

        RHI::BufferViewDescriptor sourceBufferView;
        sourceBufferView.m_elementOffset = 0;
        sourceBufferView.m_elementCount = m_vertexCountPerInstance;
        sourceBufferView.m_elementSize = 12;
        FindAttachmentBinding(Name{ "SourceData" })->m_unifiedScopeDesc.SetAsBuffer(sourceBufferView);

        RHI::BufferViewDescriptor targetBufferView;
        targetBufferView.m_elementOffset = 0;
        targetBufferView.m_elementCount = m_targetVertexStridePerInstance * m_instanceCount;
        targetBufferView.m_elementSize = 12;
        FindAttachmentBinding(Name{ "TargetData" })->m_unifiedScopeDesc.SetAsBuffer(targetBufferView);

        BaseClass::BuildInternal();
    }

    void VertexAnimationPass::FrameBeginInternal(FramePrepareParams params)
    {
        m_shaderResourceGroup->SetConstant(m_frameNumberNameIndex, m_frameNumber++);
        m_shaderResourceGroup->SetConstant(m_vertexCountPerInstanceNameIndex, m_vertexCountPerInstance);
        m_shaderResourceGroup->SetConstant(m_targetVertexStridePerInstanceNameIndex, m_targetVertexStridePerInstance);

        BaseClass::FrameBeginInternal(params);
    }
} // namespace AZ::Render
