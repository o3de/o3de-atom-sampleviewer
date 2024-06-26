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
#include <AzCore/Math/Color.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <Atom/RHI/ScopeProducer.h>
#include <Atom/RHI/FrameScheduler.h>
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

        static const uint32_t s_numberOfTrianglesTotal;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // Updates a single constant buffer with multiple triangle instances
        void UploadDataToConstantBuffer(InstanceInfo* data, uint32_t elementSize, uint32_t elementCount);
        void CreateConstantBufferView();
        
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

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_constantBufferPool;

        AZ::RHI::Ptr<AZ::RHI::Buffer> m_constantBuffer;

        AZ::RHI::Ptr<AZ::RHI::BufferView> m_constantBufferView;

        // --------------------------------------------------------
        // Pipeline state and SRG to be constructed from the shader
        // --------------------------------------------------------
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;
    };
} // namespace AtomSampleViewer
