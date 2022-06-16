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
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/BufferPool.h>

#include <AzCore/Math/Matrix4x4.h>

#include <RHI/BasicRHIComponent.h>
#include <AzCore/Component/TickBus.h>

namespace AtomSampleViewer
{
    //! The purpose of this sample is to establish a simple XR sample utilizing a simple VR pipeline
    //! It will render a mesh per controller plus one for the front view. It will prove out all the 
    //! code related related to openxr device, instance, swapchain, session, input, space.       
    class XRExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(XRExampleComponent, "{A7D9A921-1FF9-4078-92BD-169E258456E7}", AZ::Component);
        AZ_DISABLE_COPY(XRExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        XRExampleComponent();
        ~XRExampleComponent() override = default;

    protected:
        // 1 cube for view + 2 cubes for the controller
        static const uint32_t s_numberOfCubes = 3; 
 
        static const uint32_t s_geometryVertexCount = 24;
        static const uint32_t s_geometryIndexCount = 36;

        struct SingleCubeBufferData
        {
            AZStd::array<VertexPosition, s_geometryVertexCount> m_positions;
            AZStd::array<VertexColor, s_geometryVertexCount> m_colors;
            AZStd::array<uint16_t, s_geometryIndexCount> m_indices;
        };

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        //! Create IA data
        void CreateCubeInputAssemblyBuffer();
        //! Create Cube data
        SingleCubeBufferData CreateSingleCubeBufferData(const AZ::Vector4 color);
        //! Create PSO data
        void CreateCubePipeline();
        //! Create the relevant Scope
        void CreateScope();

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::IndexBufferView m_indexBufferView;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;
        AZ::RHI::InputStreamLayout m_streamLayoutDescriptor;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;

        struct BufferData
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::DrawItem m_drawItem;
        float m_time = 0.0f;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numberOfCubes> m_shaderResourceGroups;
        AZStd::array<AZ::Matrix4x4, s_numberOfCubes> m_modelMatrices;
        AZ::Matrix4x4 m_viewProjMatrix;

        AZ::RHI::ShaderInputConstantIndex m_shaderIndexWorldMat;
        AZ::RHI::ShaderInputConstantIndex m_shaderIndexViewProj;  
    };
} // namespace AtomSampleViewer
