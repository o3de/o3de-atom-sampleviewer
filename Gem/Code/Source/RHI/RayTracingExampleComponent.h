/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/Device.h>
#include <Atom/RHI/DeviceBufferPool.h>
#include <Atom/RHI/DeviceDispatchRaysItem.h>
#include <Atom/RHI/DevicePipelineState.h>
#include <Atom/RHI/DeviceRayTracingAccelerationStructure.h>
#include <Atom/RHI/DeviceRayTracingBufferPools.h>
#include <Atom/RHI/DeviceRayTracingPipelineState.h>
#include <Atom/RHI/DeviceRayTracingShaderTable.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Math/Matrix4x4.h>
#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    // This sample demonstrates the use of Atom Ray Tracing through the RHI abstraction layer.
    // It creates three triangles and one rectangle in a scene, and ray traces that scene to
    // an output image and displays it.
    class RayTracingExampleComponent final : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(RayTracingExampleComponent, "{FC4636BC-9C5C-4D7D-8FEF-41A02C56B62D}", AZ::Component);
        AZ_DISABLE_COPY(RayTracingExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        RayTracingExampleComponent();
        ~RayTracingExampleComponent() override
        {
        }

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        void CreateResourcePools();
        void CreateGeometry();
        void CreateFullScreenBuffer();
        void CreateOutputTexture();
        void CreateRasterShader();
        void CreateRayTracingAccelerationStructureObjects();
        void CreateRayTracingPipelineState();
        void CreateRayTracingShaderTable();
        void CreateRayTracingAccelerationTableScope();
        void CreateRayTracingDispatchScope();
        void CreateRasterScope();

        static const uint32_t m_imageWidth = 1920;
        static const uint32_t m_imageHeight = 1080;

        // resource pools
        RHI::Ptr<RHI::DeviceBufferPool> m_inputAssemblyBufferPool;
        RHI::Ptr<RHI::DeviceImagePool> m_imagePool;
        RHI::Ptr<RHI::DeviceRayTracingBufferPools> m_rayTracingBufferPools;

        // triangle vertex/index buffers
        AZStd::array<VertexPosition, 3> m_triangleVertices;
        AZStd::array<uint16_t, 3> m_triangleIndices;

        RHI::Ptr<RHI::DeviceBuffer> m_triangleVB;
        RHI::Ptr<RHI::DeviceBuffer> m_triangleIB;

        // rectangle vertex/index buffers
        AZStd::array<VertexPosition, 4> m_rectangleVertices;
        AZStd::array<uint16_t, 6> m_rectangleIndices;

        RHI::Ptr<RHI::DeviceBuffer> m_rectangleVB;
        RHI::Ptr<RHI::DeviceBuffer> m_rectangleIB;

        // ray tracing acceleration structures
        RHI::Ptr<RHI::DeviceRayTracingBlas> m_triangleRayTracingBlas;
        RHI::Ptr<RHI::DeviceRayTracingBlas> m_rectangleRayTracingBlas;
        RHI::Ptr<RHI::DeviceRayTracingTlas> m_rayTracingTlas;
        RHI::BufferViewDescriptor m_tlasBufferViewDescriptor;
        RHI::AttachmentId m_tlasBufferAttachmentId = RHI::AttachmentId("tlasBufferAttachmentId");

        // ray tracing shaders
        Data::Instance<RPI::Shader> m_rayGenerationShader;
        Data::Instance<RPI::Shader> m_missShader;
        Data::Instance<RPI::Shader> m_closestHitGradientShader;
        Data::Instance<RPI::Shader> m_closestHitSolidShader;

        // ray tracing pipeline state
        RHI::Ptr<RHI::DeviceRayTracingPipelineState> m_rayTracingPipelineState;

        // ray tracing shader table
        RHI::Ptr<RHI::DeviceRayTracingShaderTable> m_rayTracingShaderTable;

        // ray tracing global shader resource group and pipeline state
        Data::Instance<RPI::ShaderResourceGroup> m_globalSrg;
        RHI::ConstPtr<RHI::DevicePipelineState> m_globalPipelineState;

        // ray tracing local shader resource groups, one for each object in the scene
        enum LocalSrgs
        {
            Triangle1,
            Triangle2,
            Triangle3,
            Rectangle,
            Count
        };
        AZStd::array<Data::Instance<RPI::ShaderResourceGroup>, LocalSrgs::Count> m_localSrgs;
        bool m_buildLocalSrgs = true;

        // output image, written to by the ray tracing shader and displayed in the fullscreen draw shader
        RHI::Ptr<RHI::DeviceImage> m_outputImage;
        RHI::Ptr<RHI::DeviceImageView> m_outputImageView;
        RHI::ImageViewDescriptor m_outputImageViewDescriptor;
        RHI::AttachmentId m_outputImageAttachmentId = RHI::AttachmentId("outputImageAttachmentId");

        // fullscreen buffer for the raster pass to display the output image
        struct FullScreenBufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        RHI::Ptr<RHI::DeviceBuffer> m_fullScreenInputAssemblyBuffer;
        AZStd::array<RHI::DeviceStreamBufferView, 2> m_fullScreenStreamBufferViews;
        RHI::DeviceIndexBufferView m_fullScreenIndexBufferView;
        RHI::InputStreamLayout m_fullScreenInputStreamLayout;

        RHI::ConstPtr<RHI::DevicePipelineState> m_drawPipelineState;
        Data::Instance<RPI::ShaderResourceGroup> m_drawSRG;
        RHI::ShaderInputConstantIndex m_drawDimensionConstantIndex;
        RHI::DeviceDrawItem m_drawItem;

        // time variable for moving the triangles and rectangle each frame
        float m_time = 0.0f;
    };
} // namespace AtomSampleViewer
