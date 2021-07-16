/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Matrix3x3.h>
#include <Atom/RHI/PipelineState.h>
#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    /*
    * All Texture testing
    * Testing render to and sample from 1d, 1d array, 2d array, cubemap, cubemap array and 3d texture
    * Showing as flashing rectangles for each texture layer when success
    * From left to right are: 1d, 1d array, 2d array, cubemap, cubemap array, 3d
    */
    class TextureMapExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(TextureMapExampleComponent, "{AF3E2D0C-7C5B-476C-B4D5-41526D88BF3C}", AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        TextureMapExampleComponent();
        ~TextureMapExampleComponent() = default;

    protected:
        AZ_DISABLE_COPY(TextureMapExampleComponent);

        static const char* s_textureMapExampleName;
        static const int s_numOfTargets = 6;
        static const int s_textureWidth = 8;
        static const int s_textureHeight = 8;
        static const int s_textureDepth = 8;
        static const int s_arraySize = 3;
        static const AZ::RHI::Format s_textureFormat = AZ::RHI::Format::R8G8B8A8_UNORM_SRGB;

        enum RenderTargetIndex 
        {
            Texture1D = 0,
            Texture1DArray,
            Texture2DArray,
            TextureCubemap,
            TextureCubemapArray,
            Texture3D,
            BufferViewIndex
        };

        struct BufferViewData
        {
            AZStd::array<AZ::RHI::StreamBufferView, 2> m_streamBufferViews;
            AZ::RHI::IndexBufferView m_indexBufferView;
            AZ::RHI::InputStreamLayout m_inputStreamLayout;
        };
        
        // Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void CreateInputAssemblyBufferPool();
        void InitRenderTargetBufferView();
        void InitTexture1DBufferView();
        void InitTexture1DArrayBufferView();
        void InitTexture2DArrayBufferView();
        void InitCubemapBufferView();
        void InitCubemapArrayBufferView();
        void InitTexture3DBufferView();

        void InitRenderTargets();
        void LoadRenderTargetShader();
        void LoadRasterShader(const char* shaderFilePath, int target);

        void CreateTextureScope(int target, const AZ::RHI::ImageViewDescriptor& imageViewDescriptor, const char* scopeName);
        void CreateRasterScope(int target, const AZ::RHI::ImageViewDescriptor& imageViewDescriptor, const char* scopeName);

        void SetVertexPositionTransform(VertexPosition* positionBuffer, int bufferIndex, AZ::Matrix3x3 transform, float translateX, float translateY);
        void SetVertexPositionArray(VertexPosition* positionBuffer, float scale, float translateX, float translateY );
        void SetVertexPositionCubemap(VertexPosition* positionBuffer, int bufferIndex, float translateX, float translateY);

        void SetVertexIndexRectsCounterClock(uint16_t* indexBuffer, size_t arraySize);
        
        void SetVertexUVWArray(VertexUVW* uvwBuffer, int arraySize);
        void SetVertexUVWCubemap(VertexUVW* uvwBuffer);
        void SetVertexUVWXCubemapArray(VertexUVWX* uvwxBuffer, int arraySize);

        void InitBufferView(int target, 
            uint32_t posSize, void* posData,
            uint32_t uvSize, void* uvData, uint32_t uvTypeSize,
            uint32_t indexSize, void* indexData);

        // -------------------
        // Input Assembly Data
        // -------------------
        AZStd::array<VertexPosition, 72> m_positions;
        AZStd::array<uint16_t, 108> m_indices;

        AZStd::array<VertexUV, 12> m_uvs;
        AZStd::array<VertexUVW, 24> m_uvws;
        AZStd::array<VertexUVWX, 72> m_uvwxs;

        // -------------------------------------
        // Input Assembly buffer and buffer view
        // -------------------------------------
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        // array for buffer and buffer view ( all render targets + screen)
        AZStd::array< AZ::RHI::Ptr<AZ::RHI::Buffer>, s_numOfTargets + 1> m_positionBuffer;
        AZStd::array< AZ::RHI::Ptr<AZ::RHI::Buffer>, s_numOfTargets + 1> m_uvBuffer;
        AZStd::array< AZ::RHI::Ptr<AZ::RHI::Buffer>, s_numOfTargets + 1> m_indexBuffer;
        AZStd::array<BufferViewData, s_numOfTargets + 1> m_bufferViews;

        // ---------------------------------------
        // Pipeline state, SRG, shader input index
        // ---------------------------------------
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, s_numOfTargets> m_targetPipelineStates;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numOfTargets> m_targetSRGs;
        AZStd::array<AZ::RHI::ShaderInputConstantIndex, s_numOfTargets> m_shaderInputConstantIndices;

        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, s_numOfTargets> m_screenPipelineStates;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, s_numOfTargets> m_screenSRGs;
        AZStd::array<AZ::RHI::ShaderInputImageIndex, s_numOfTargets> m_shaderInputImageIndices;

        // ------
        // Others
        // ------
        float m_time = 0.0f;
        AZ::RHI::ClearValue m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);
        AZStd::array<AZ::RHI::TransientImageDescriptor, s_numOfTargets> m_renderTargetImageDescriptors;
        AZStd::array<AZ::RHI::AttachmentId, s_numOfTargets> m_attachmentID;
        AZStd::array<AZ::Vector3, 4> m_baseRectangle;
    };
}