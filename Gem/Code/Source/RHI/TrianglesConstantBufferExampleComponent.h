/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Color.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <Atom/RHI/ScopeProducer.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/BufferPool.h>

#include <AzCore/Math/Matrix4x4.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{

    /**
     * This example is similar to Triangle example, but it uses instancing to render
     * 30 triangles in 1 draw call. The main purpose of this example is show how to use a
     * Constant Buffer to pass data to the shader instead of using SRG Constants.
     * The data of all instances is uploaded with a single Constant Buffer
     * once per frame, then the vertex shader uses the SV_InstanceID to access to the 
     * right instance information from the Constant Buffer.
     */
    class TrianglesConstantBufferExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
        struct InstanceInfo
        {
            AZStd::array<float, 16> m_matrix;
            AZStd::array<float, 4> m_colorMultiplier;
        };
        
    public:
        AZ_COMPONENT(TrianglesConstantBufferExampleComponent, "{EF33DA37-65F8-44E5-BD09-B1C948229374}", AZ::Component);
        AZ_DISABLE_COPY(TrianglesConstantBufferExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        TrianglesConstantBufferExampleComponent();
        ~TrianglesConstantBufferExampleComponent() override = default;

    private:
        static const char* s_trianglesConstantBufferExampleName;

        static const uint32_t s_numberOfTrianglesSingleCB;
        static const uint32_t s_numberOfTrianglesMultipleCB;
        static const uint32_t s_numberOfTrianglesTotal;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // Updates a single buffer with multiple triangle instance data
        void UploadDataToSingleConstantBuffer(InstanceInfo* data, uint32_t elementSize, uint32_t elementCount);
        // Updates multiple buffers, each with a single triangle instance data
        void UploadDataToMultipleConstantBuffers(InstanceInfo* data, uint32_t elementSize);
        
        AZ::RHI::DrawItem m_drawItem;

        float m_time = 0.0f;

        // -------------------------------------------------
        // Input Assembly buffer and its Streams/Index Views
        // -------------------------------------------------
        struct IABufferData
        {
            AZStd::array<VertexPosition, 3> m_positions;
            AZStd::array<VertexColor, 3> m_colors;
            AZStd::array<uint16_t, 3> m_indices;
        };

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;

        // Pool where both buffers are allocated from
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_constantBufferPool;

        // Pool with single Buffer
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_constantBuffer;
        // Pool with multiple Buffers
        AZStd::vector<AZ::RHI::Ptr<AZ::RHI::Buffer>> m_constantBufferArray;

        // Buffer views that hold both from 
        AZStd::vector<AZ::RHI::Ptr<AZ::RHI::BufferView>> m_constantBufferViewArray;

        // Cached Constant Buffer alignment queued from the device
        uint32_t m_constantBufferAlighment = 0u;

        // --------------------------------------------------------
        // Pipeline state and SRG to be constructed from the shader
        // --------------------------------------------------------
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;
    };
} // namespace AtomSampleViewer
