/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Public/Pass/ComputePass.h>

namespace AZ::Render
{
    class VertexAnimationPass : public RPI::ComputePass
    {
        using BaseClass = RPI::ComputePass;

    public:
        AZ_RTTI(VertexAnimationPass, "{5796A3AC-E306-4F0C-8B2D-DDC25EE446F8}", BaseClass);
        AZ_CLASS_ALLOCATOR(VertexAnimationPass, SystemAllocator, 0);
        static RPI::Ptr<VertexAnimationPass> Create(const RPI::PassDescriptor& descriptor);

        void SetSourceBuffer(Data::Instance<RPI::Buffer> sourceBuffer);
        void SetTargetBuffer(Data::Instance<RPI::Buffer> targetBuffer);
        void SetInstanceOffsetBuffer(Data::Instance<RPI::Buffer> instanceOffsetBuffer);
        void SetInstanceCount(u32 instanceCount);
        void SetVertexCountPerInstance(u32 vertexCountPerInstance);
        void SetTargetVertexStridePerInstance(u32 targetVertexStridePerInstance);

    protected:
        explicit VertexAnimationPass(const RPI::PassDescriptor& descriptor);

        void BuildInternal() override;
        void FrameBeginInternal(FramePrepareParams params) override;

    private:
        Data::Instance<RPI::Buffer> m_sourceBuffer;
        Data::Instance<RPI::Buffer> m_targetBuffer;
        Data::Instance<RPI::Buffer> m_instanceOffsetBuffer;
        u32 m_instanceCount;
        u32 m_vertexCountPerInstance;
        u32 m_targetVertexStridePerInstance;

        RHI::ShaderInputNameIndex m_frameNumberNameIndex{ "m_frameNumber" };
        RHI::ShaderInputNameIndex m_vertexCountPerInstanceNameIndex{ "m_vertexCountPerInstance" };
        RHI::ShaderInputNameIndex m_targetVertexStridePerInstanceNameIndex{ "m_targetVertexStridePerInstance" };
        unsigned m_frameNumber{ 0 };
    };
} // namespace AZ::Render
