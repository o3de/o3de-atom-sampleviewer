/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/Debug/RayTracingDebugFeatureProcessorInterface.h>
#include <Atom/Feature/RayTracing/RayTracingFeatureProcessorInterface.h>
#include <Atom/RPI.Public/Buffer/BufferPool.h>
#include <AzCore/Math/PackedVector3.h>
#include <CommonSampleComponentBase.h>
#include <Passes/VertexAnimationPass.h>

namespace AtomSampleViewer
{
    //! This test adds many instances of a basic shape to the ray-tracing scene and animates the shapes' vertices each frame. The
    //! visualization is done with the DebugRayTracingPass. The goal of this test is to benchmark the ray tracing acceleration structure
    //! build/update time for animated meshes.
    class RayTracingVertexAnimationExampleComponent
        : public CommonSampleComponentBase
        , public AZ::RPI::SceneNotificationBus::Handler
    {
        using Base = CommonSampleComponentBase;

    public:
        AZ_COMPONENT(RayTracingVertexAnimationExampleComponent, "{9B43FED7-8BD0-4159-BDEC-BBF7EA39C1EC}", Base);

        static void Reflect(AZ::ReflectContext* context);

        RayTracingVertexAnimationExampleComponent();

        void Activate() override;
        void Deactivate() override;

        void OnRenderPipelineChanged(AZ::RPI::RenderPipeline* renderPipeline, RenderPipelineChangeType changeType) override;

    private:
        AZ_DISABLE_COPY_MOVE(RayTracingVertexAnimationExampleComponent);

        struct RayTracingMesh
        {
            AZ::Uuid m_uuid;
            AZ::Render::RayTracingFeatureProcessorInterface::Mesh m_rtMesh;
            AZ::Render::RayTracingFeatureProcessorInterface::SubMeshVector m_rtSubMeshes;
        };
        struct BasicGeometry
        {
            AZStd::vector<AZ::PackedVector3f> m_positions;
            AZStd::vector<AZ::PackedVector3f> m_normals;
            AZStd::vector<AZ::u32> m_indices;
        };

        BasicGeometry GenerateBasicGeometry();
        void CreateBufferPools();
        void CreateRayTracingGeometry();
        void AddVertexAnimationPass(AZ::RPI::RenderPipeline* renderPipeline);
        AZ::Render::RayTracingFeatureProcessorInterface& GetRayTracingFeatureProcessor();
        AZ::Render::RayTracingDebugFeatureProcessorInterface& GetRayTracingDebugFeatureProcessor();

        AZ::Render::RayTracingFeatureProcessorInterface* m_rayTracingFeatureProcessor{ nullptr };
        AZ::Render::RayTracingDebugFeatureProcessorInterface* m_rayTracingDebugFeatureProcessor{ nullptr };

        AZ::RHI::BufferBindFlags m_geometryDataBufferBindFlags{ AZ::RHI::BufferBindFlags::ShaderReadWrite |
                                                                AZ::RHI::BufferBindFlags::DynamicInputAssembly };
        AZ::Data::Asset<AZ::RPI::ResourcePoolAsset> m_geometryDataBufferPoolAsset;
        AZ::Data::Instance<AZ::RPI::Buffer> m_sourceGeometryBuffer;
        AZ::Data::Instance<AZ::RPI::Buffer> m_targetGeometryBuffer;
        AZStd::vector<RayTracingMesh> m_rayTracingData;
        int m_geometryCount{ 1024 };
        AZ::u32 m_vertexCountPerInstance;
        AZ::u32 m_targetVertexStridePerInstance;
        AZ::RPI::Ptr<AZ::Render::VertexAnimationPass> m_vertexAnimationPass;
    };
} // namespace AtomSampleViewer
