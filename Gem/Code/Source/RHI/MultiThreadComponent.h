/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <RHI/BasicRHIComponent.h>

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <MultiThreadComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    //! MultiThreadComponent
    //! The purpose of this sample is to test support for multithreaded command list and
    //! evaluate performance numbers to ensure parallelization by FrameScheduler.
    //! There will be one model rendered multiple times over multiple command lists with thousands 
    //! of draw calls with a total of million plus polygons.
    class MultiThreadComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(MultiThreadComponent, "{45950624-28A3-4946-B0FE-E07A640DC6CF}", AZ::Component);
        AZ_DISABLE_COPY(MultiThreadComponent);

        static void Reflect(AZ::ReflectContext* context);

        MultiThreadComponent();
        ~MultiThreadComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    protected:
        // We decrease the number of cubes on mobile due to performance.
        static const uint32_t s_cubesPerLine = ATOMSAMPLEVIEWER_TRAIT_MULTITHREAD_SAMPLE_CUBES_PER_LINE;
        static const uint32_t s_numberOfCubes = s_cubesPerLine* s_cubesPerLine;
        static const uint32_t s_geometryVertexCount = 24;
        static const uint32_t s_geometryIndexCount = 36;

        struct SingleCubeBufferData
        {
            AZStd::array<VertexPosition, s_geometryVertexCount> m_positions;
            AZStd::array<VertexColor, s_geometryVertexCount> m_colors;
            AZStd::array<uint16_t, s_geometryIndexCount> m_indices;
        };

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        SingleCubeBufferData CreateSingleCubeBufferData(const AZ::Vector4 color);
        void CreateInputAssemblyBuffer();
        void CreatePipeline();
        void CreateScope();

        AZ::Matrix4x4 m_viewProjMatrix;
        static constexpr float m_zNear = 1.0f;
        static constexpr float m_zFar = 1000.0f;
        static const AZ::Vector3 m_up;
        AZ::Vector3 m_lookAt;
        static const uint32_t s_cubeSpacing = 3;
        float m_time = 0.0f;

        AZStd::array<AZ::Matrix4x4, s_numberOfCubes> m_cubeTransforms;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZ::RHI::InputStreamLayout m_streamLayoutDescriptor;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;

        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numberOfCubes> m_shaderResourceGroups;
        AZ::RHI::ShaderInputConstantIndex m_shaderIndexWorldMat;
        AZ::RHI::ShaderInputConstantIndex m_shaderIndexViewProj;

        AZ::RHI::AttachmentId m_depthStencilID;
        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;
    };
} // namespace AtomSampleViewer
