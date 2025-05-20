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
#include <Utils/ImGuiHistogramQueue.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    //! This test adds many instances of a basic shape to the ray-tracing scene and animates the shapes' vertices each frame. The
    //! visualization is done with the DebugRayTracingPass. The goal of this test is to benchmark the ray tracing acceleration structure
    //! build/update time for animated meshes.
    class RayTracingVertexAnimationExampleComponent
        : public CommonSampleComponentBase
        , public AZ::RPI::SceneNotificationBus::Handler
        , public AZ::TickBus::Handler
    {
        using Base = CommonSampleComponentBase;

        enum class AccelerationStructureType : int
        {
            TriangleBLAS,
            CLAS_ClusterBLAS,
        };

    public:
        AZ_COMPONENT(RayTracingVertexAnimationExampleComponent, "{9B43FED7-8BD0-4159-BDEC-BBF7EA39C1EC}", Base);

        static void Reflect(AZ::ReflectContext* context);

        RayTracingVertexAnimationExampleComponent();

        void Activate() override;
        void Deactivate() override;
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

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

        void SaveVSyncStateAndDisableVsync();
        void RestoreVSyncState();
        BasicGeometry GenerateBasicGeometry();
        void CreateBufferPools();
        void CreateRayTracingGeometry();
        void AddVertexAnimationPass(AZ::RPI::RenderPipeline* renderPipeline);
        void DrawSidebar();

        AZ::Render::RayTracingFeatureProcessorInterface& GetRayTracingFeatureProcessor();
        AZ::Render::RayTracingDebugFeatureProcessorInterface& GetRayTracingDebugFeatureProcessor();

        AZ::Render::RayTracingFeatureProcessorInterface* m_rayTracingFeatureProcessor{ nullptr };
        AZ::Render::RayTracingDebugFeatureProcessorInterface* m_rayTracingDebugFeatureProcessor{ nullptr };
        uint32_t m_preActivateVSyncInterval{ 0 };

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

        ImGuiSidebar m_imguiSidebar;
        AccelerationStructureType m_accelerationStructureType{ AccelerationStructureType::TriangleBLAS };
        ImGuiHistogramQueue m_imGuiFrameTimer{ 60, 60 };
        ImGuiHistogramQueue m_accelerationStructureTimer{ 60, 60 };
        AZ::RHI::Ptr<AZ::RPI::Pass> m_rayTracingAccelerationStructurePass;
    };
} // namespace AtomSampleViewer
