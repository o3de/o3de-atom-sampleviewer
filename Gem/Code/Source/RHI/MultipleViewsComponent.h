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
#include <Atom/RPI.Public/Image/AttachmentImage.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Math/Vector3.h>

namespace AtomSampleViewer
{
    
    //!  MultipleViewsComponent
    //!  The purpose of this sample is to test multiple views by doing shadow mapping,
    //!  to have more complex shaders, test resource bindings and comprehensive support of depth/stencil buffer.
    //!  For this sample, there will be 2 views: the main view (or camera view) and the light view.
    //!  As for the geometries, there is a plane with a cube sitting on it.
     class MultipleViewsComponent final
        : public BasicRHIComponent
        , public AZ::EntitySystemBus::Handler
    {
    public:
        AZ_COMPONENT(MultipleViewsComponent, "{A8CC6572-7FD4-4708-88B3-E24034C8F6FC}", AZ::Component);
        AZ_DISABLE_COPY(MultipleViewsComponent);

        static void Reflect(AZ::ReflectContext* context);

        MultipleViewsComponent();
        ~MultipleViewsComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    protected:

        static const uint32_t geometryVertexCount = 28;
        static const uint32_t geometryIndexCount = 42;
        struct BufferData
        {
            AZStd::array<VertexPosition, geometryVertexCount> m_positions;
            AZStd::array<VertexColor, geometryVertexCount> m_colors;
            AZStd::array<VertexNormal, geometryVertexCount> m_normals;
            AZStd::array<uint16_t, geometryIndexCount> m_indices;
        };

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;
        
        // Create Buffers for Input Assembly
        BufferData CreateBufferData();
        void CreateInputAssemblyBuffersAndViews();
        void SetupScene();
        void InitRenderTarget();
        void CreateDepthScope();
        void CreateMainScope();
        void ReadDepthShader();
        void ReadMainShader();

        // shadow map required variable
        AZ::Matrix4x4 m_worldMatrix;
        AZ::Matrix4x4 m_viewProjectionMatrix;
        AZ::Vector3 m_worldPosition;
        AZ::Matrix4x4 m_lightViewProjectionMatrix;
        AZ::Matrix4x4 m_lightProjectionMatrix;
        AZ::Vector4 m_lightPosition;
        AZ::Vector4 m_ambientColor;
        AZ::Vector4 m_diffuseColor;

        static constexpr float m_zNear = 1.0f;
        static constexpr float m_zFar = 100.0f;
        static const AZ::Vector3 m_up;
        static const AZ::Vector3 m_lookAt;
        
        static const int s_shadowMapSize = 1024;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;

        AZ::RHI::InputStreamLayout m_inputStreamLayout;
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, 2> m_pipelineStates;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, 2> m_shaderResourceGroups;
        AZStd::array<AZ::RHI::ShaderInputConstantIndex, 6> m_shaderInputConstantIndices;
        AZ::RHI::ShaderInputImageIndex m_shaderInputImageIndex;

        AZStd::array<AZ::RHI::StreamBufferView, 3> m_streamBufferViews;
        AZ::RHI::IndexBufferView m_indexBufferView;
        AZ::RHI::AttachmentId m_depthMapID;
        AZ::RHI::AttachmentId m_depthStencilID;
        AZ::RHI::ClearValue m_depthClearValue;

        AZ::RHI::TransientImageDescriptor m_transientImageDescriptor;
    };
} // namespace AtomSampleViewer
