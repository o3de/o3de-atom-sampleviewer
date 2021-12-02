/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI/ScopeProducer.h>
#include <Atom/RPI.Public/Pass/RenderPass.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RHI/RayTracingBufferPools.h>
#include <Atom/RHI/RayTracingPipelineState.h>
#include <Atom/RHI/RayTracingShaderTable.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <RayTracing/RayTracingFeatureProcessor.h>

namespace AZ
{
    namespace Render
    {
        //! A pass to generate rays g-buffer for ambient occlusion.
        class RayTracingAmbientOcclusionPass final
            : public RPI::RenderPass
        {
        public:
            AZ_RPI_PASS(RayTracingAmbientOcclusionPass);

            AZ_RTTI(RayTracingAmbientOcclusionPass, "{4E8F814F-F7C4-4788-B793-D7F118992819}", RPI::RenderPass);
            AZ_CLASS_ALLOCATOR(RayTracingAmbientOcclusionPass, SystemAllocator, 0);

            virtual ~RayTracingAmbientOcclusionPass() override;

            //! Creates a RayTracingAmbientOcclusionPass
            static RPI::Ptr<RayTracingAmbientOcclusionPass> Create(const RPI::PassDescriptor& descriptor);

            //! Get/Set some raytracing ao settings
            uint32_t GetRayNumberPerPixel();
            void SetRayNumberPerPixel(uint32_t rayNumber);
            float GetRayExtentMin();
            void SetRayExtentMin(float minT);
            float GetRayExtentMax();
            void SetRayExtentMax(float maxT);

        private:
            explicit RayTracingAmbientOcclusionPass(const RPI::PassDescriptor& descriptor);

            void CreateRayTracingPipelineState();

            // Scope producer functions
            void SetupFrameGraphDependencies(RHI::FrameGraphInterface frameGraph) override;
            void CompileResources(const RHI::FrameGraphCompileContext& context) override;
            void BuildCommandListInternal(const RHI::FrameGraphExecuteContext& context) override;

            // Pass overrides
            void FrameBeginInternal(FramePrepareParams params) override;

            // ray tracing shader and pipeline state
            Data::Instance<RPI::Shader> m_rayGenerationShader;
            Data::Instance<RPI::Shader> m_missShader;
            Data::Instance<RPI::Shader> m_hitShader;
            RHI::Ptr<RHI::RayTracingPipelineState> m_rayTracingPipelineState;

            // ray tracing shader table
            RHI::Ptr<RHI::RayTracingShaderTable> m_rayTracingShaderTable;

            // ray tracing global pipeline state
            RHI::ConstPtr<RHI::PipelineState> m_globalPipelineState;

            Render::RayTracingFeatureProcessor* m_rayTracingFeatureProcessor = nullptr;

            uint32_t m_frameCount = 0;

            // ray tracing ambient occlusion parameters
            float m_rayMaxT = 0.4f;          // The ray's far distance
            float m_rayMinT = 0.01f;        // The ray's near distance
            uint32_t m_rayNumber = 8;      // Ray casted per pixel

            bool m_createRayTracingPipelineState = true;
        };
    }   // namespace RPI
}   // namespace AZ
