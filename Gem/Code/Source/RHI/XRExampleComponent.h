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
#include <Atom/RPI.Public/XR/XRSpaceNotificationBus.h>

#include <Atom/RHI/FrameScheduler.h>
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
    //! There will be three instances of this class active.
    //! 1- The main pipline.
    //! 2- The Left eye pipeline.
    //! 3- The Right eye pipeline.       
    class XRExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
        , public AZ::RPI::XRSpaceNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(XRExampleComponent, "{A7D9A921-1FF9-4078-92BD-169E258456E7}");
        AZ_DISABLE_COPY(XRExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        XRExampleComponent();
        ~XRExampleComponent() override = default;

    protected:
        // 1 cube for view + 2 cubes for the controller
        static const uint32_t NumberOfCubes = 3; 
        static const uint32_t GeometryVertexCount = 24;
        static const uint32_t GeometryIndexCount = 36;

        struct SingleCubeBufferData
        {
            AZStd::array<VertexPosition, GeometryVertexCount> m_positions;
            AZStd::array<VertexColor, GeometryVertexCount> m_colors;
            AZStd::array<uint16_t, GeometryIndexCount> m_indices;
        };

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::RPI::XRSpaceNotificationBus::Handler overrides
        // This notification arrives before this.OnFramePrepare, additionally it is called
        // after XR System calls XrSystem::BeginFrame. Why this is important? Because these
        // transformations are calculated by the XR Device based on a predicted display time
        // for the current frame. If the shaders update the viewSrg with this freash data
        // the rendered image appears more stable and less jittery.
        void OnXRSpaceLocationsChanged(
            const AZ::Transform& baseSpaceToHeadTm,
            const AZ::Transform& headToLeftEyeTm,
            const AZ::Transform& headToRightEyeTm) override;

        // Controller (aka Joysticks) poses/transforms are relative to the head transform.
        AZ::Transform m_baseSpaceToHeadTm;

        //! Create IA data
        void CreateCubeInputAssemblyBuffer();
        //! Create Cube data
        SingleCubeBufferData CreateSingleCubeBufferData();
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
        float m_time = 0.0f;
        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, NumberOfCubes> m_shaderResourceGroups;
        AZStd::array<AZ::Matrix4x4, NumberOfCubes> m_modelMatrices;
        AZ::Matrix4x4 m_viewProjMatrix;

        AZ::RHI::ShaderInputConstantIndex m_shaderIndexWorldMat;
        AZ::RHI::ShaderInputConstantIndex m_shaderIndexViewProj;  
        AZ::RHI::AttachmentId m_depthStencilID;
    };
} // namespace AtomSampleViewer
