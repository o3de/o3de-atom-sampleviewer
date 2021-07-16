/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/ScopeProducer.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{    
    //! This samples shows how to handle vertex buffer generation on a compute shader and how
    //! to properly declare shader inputs.
    //! There's 2 scopes in this sample, one runs a compute shader that is in charge of generating and animating the
    //! vertices of a 2D hexagon. The second scope is in charge of drawing the previously generated hexagon vertex buffer.
    class InputAssemblyExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(InputAssemblyExampleComponent, "{1F061564-DB4A-4B68-B361-0B427B3CA5B5}", AZ::Component);
        AZ_DISABLE_COPY(InputAssemblyExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        InputAssemblyExampleComponent();
        ~InputAssemblyExampleComponent() = default;

    protected:
        // We have only float4 vertex positions as data.
        using BufferData = AZStd::array<AZStd::array<float, 4>, 6>;

        int m_numThreadsX = 1;
        int m_numThreadsY = 1;
        int m_numThreadsZ = 1;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        void FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void CreateInputAssemblyLayout();
        void CreateBuffers();
        void LoadComputeShader();
        void LoadRasterShader();
        void CreateComputeScope();
        void CreateRasterScope();

        // -------------------------------------------------
        // Input Assembly buffer and its Streams/Index Views
        // -------------------------------------------------
        AZ::RHI::StreamBufferView m_streamBufferView[2];
        AZ::RHI::InputStreamLayout m_inputStreamLayout;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        // ----------------------
        // Pipeline state and SRG
        // ----------------------

        // Dispatch pipeline
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_dispatchPipelineState;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_dispatchSRG[2];
        AZ::RHI::ShaderInputConstantIndex m_dispatchTimeConstantIndex;
        AZ::RHI::ShaderInputBufferIndex m_dispatchIABufferIndex;

        // Draw pipeline
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_drawPipelineState;
        AZ::RHI::ShaderInputConstantIndex m_drawMatrixIndex;
        AZ::RHI::ShaderInputConstantIndex m_drawColorIndex;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_drawSRG[2];

        // This is used to animate the hexagon.
        float m_time = 0.0f;
    };
}
