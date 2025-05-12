/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/DeviceDispatchRaysItem.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/RayTracingAccelerationStructure.h>
#include <Atom/RHI/RayTracingBufferPools.h>
#include <Atom/RHI/RayTracingPipelineState.h>
#include <Atom/RHI/RayTracingShaderTable.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/PackedVector3.h>
#include <RHI/BasicRHIComponent.h>

namespace AZ::RHI
{
    using uint = uint32_t;
#include "../../Assets/ShaderLib/Atom/Features/IndirectRayTracing.azsli"
} // namespace AZ::RHI::CLAS

namespace AtomSampleViewer
{
    using namespace AZ;

    // This sample demonstrates the use of Atom Ray Tracing through the RHI abstraction layer.
    // It creates three triangles and one rectangle in a scene, and ray traces that scene to
    // an output image and displays it.
    class RayTracingClusterExampleComponent final
        : public BasicRHIComponent
    {
    public:
        AZ_COMPONENT(RayTracingClusterExampleComponent, "{85D46D8F-84C6-4178-ABE5-CADBBC6E6E9C}", AZ::Component);
        AZ_DISABLE_COPY(RayTracingClusterExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        RayTracingClusterExampleComponent();
        ~RayTracingClusterExampleComponent() override {}

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
        RHI::Ptr<RHI::BufferPool> m_inputAssemblyBufferPool;
        RHI::Ptr<RHI::ImagePool> m_imagePool;
        RHI::Ptr<RHI::RayTracingBufferPools> m_rayTracingBufferPools;

        // triangle vertex/index buffers
        AZStd::array<VertexPosition, 3> m_triangleVertices;
        AZStd::array<uint16_t, 3> m_triangleIndices;

        RHI::Ptr<RHI::Buffer> m_triangleVB;
        RHI::Ptr<RHI::Buffer> m_triangleIB;

        // rectangle vertex/index buffers
        AZStd::array<VertexPosition, 4> m_rectangleVertices;
        AZStd::array<uint16_t, 6> m_rectangleIndices;

        RHI::Ptr<RHI::Buffer> m_rectangleVB;
        RHI::Ptr<RHI::Buffer> m_rectangleIB;

        // CLAS data
        using ClusterVertexType = PackedVector3f;
        using ClusterIndexType = PackedVector3<uint32_t>;
        AZStd::vector<RHI::RayTracingClasBuildTriangleClusterInfoExpanded> m_clusterSourceInfosExpanded;
        RHI::Ptr<RHI::Buffer> m_clusterVertexBuffer;
        RHI::Ptr<RHI::Buffer> m_clusterIndexBuffer;
        uint32_t m_maxClusterTriangleCount = 0;
        uint32_t m_maxClusterVertexCount = 0;
        uint32_t m_maxGeometryIndex = 0;
        uint32_t m_maxTotalTriangleCount = 0;
        uint32_t m_maxTotalVertexCount = 0;

        // ray tracing acceleration structures
        RHI::Ptr<RHI::RayTracingBlas> m_triangleRayTracingBlas;
        RHI::Ptr<RHI::RayTracingBlas> m_rectangleRayTracingBlas;
        RHI::Ptr<RHI::RayTracingClusterBlas> m_clusterRayTracingBlas;
        bool m_clusterRayTracingBlasInitialized = false;
        RHI::Ptr<RHI::RayTracingTlas> m_rayTracingTlas;
        RHI::BufferViewDescriptor m_tlasBufferViewDescriptor;
        RHI::AttachmentId m_tlasBufferAttachmentId = RHI::AttachmentId("tlasBufferAttachmentId");

        // ray tracing shaders
        Data::Instance<RPI::Shader> m_rayGenerationShader;
        Data::Instance<RPI::Shader> m_missShader;
        Data::Instance<RPI::Shader> m_closestHitGradientShader;
        Data::Instance<RPI::Shader> m_closestHitSolidShader;

        // ray tracing pipeline state
        RHI::Ptr<RHI::RayTracingPipelineState> m_rayTracingPipelineState;

        // ray tracing shader table
        RHI::Ptr<RHI::RayTracingShaderTable> m_rayTracingShaderTable;

        // ray tracing global shader resource group and pipeline state
        Data::Instance<RPI::ShaderResourceGroup> m_globalSrg;
        RHI::ConstPtr<RHI::PipelineState> m_globalPipelineState;

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
        RHI::Ptr<RHI::Image> m_outputImage;
        RHI::Ptr<RHI::ImageView> m_outputImageView;
        RHI::ImageViewDescriptor m_outputImageViewDescriptor;
        RHI::AttachmentId m_outputImageAttachmentId = RHI::AttachmentId("outputImageAttachmentId");

        // fullscreen buffer for the raster pass to display the output image
        struct FullScreenBufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        RHI::Ptr<RHI::Buffer> m_fullScreenInputAssemblyBuffer;
        RHI::GeometryView m_geometryView;
        RHI::InputStreamLayout m_fullScreenInputStreamLayout;

        RHI::ConstPtr<RHI::PipelineState> m_drawPipelineState;
        Data::Instance<RPI::ShaderResourceGroup> m_drawSRG;
        RHI::ShaderInputConstantIndex m_drawDimensionConstantIndex;

        // time variable for moving the triangles and rectangle each frame
        float m_time = 0.0f;
    };
} // namespace AtomSampleViewer
