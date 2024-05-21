/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Passes/RayTracingAmbientOcclusionPass.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/SingleDeviceDispatchRaysItem.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI/ScopeProducerFunction.h>
#include <Atom/RPI.Public/Buffer/BufferSystemInterface.h>
#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Pass/PassUtils.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/Feature/TransformService/TransformServiceFeatureProcessor.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<RayTracingAmbientOcclusionPass> RayTracingAmbientOcclusionPass::Create(const RPI::PassDescriptor& descriptor)
        {
            RPI::Ptr<RayTracingAmbientOcclusionPass> pass = aznew RayTracingAmbientOcclusionPass(descriptor);
            return AZStd::move(pass);
        }

        RayTracingAmbientOcclusionPass::RayTracingAmbientOcclusionPass(const RPI::PassDescriptor& descriptor)
            : RPI::RenderPass(descriptor)
        {
            RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();
            if (device->GetFeatures().m_rayTracing == false)
            {
                // ray tracing is not supported on this platform
                SetEnabled(false);
            }
        }

        RayTracingAmbientOcclusionPass::~RayTracingAmbientOcclusionPass()
        {
        }

        void RayTracingAmbientOcclusionPass::CreateRayTracingPipelineState()
        {
            // load ray generation shader
            const char* rayGenerationShaderFilePath = "Shaders/RayTracing/RTAOGeneration.azshader";
            m_rayGenerationShader = RPI::LoadShader(rayGenerationShaderFilePath);
            AZ_Assert(m_rayGenerationShader, "Failed to load ray generation shader");

            auto rayGenerationShaderVariant = m_rayGenerationShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForRayTracing rayGenerationShaderDescriptor;
            rayGenerationShaderVariant.ConfigurePipelineState(rayGenerationShaderDescriptor);

            // load miss shader
            const char* missShaderFilePath = "Shaders/RayTracing/RTAOMiss.azshader";
            m_missShader = RPI::LoadShader(missShaderFilePath);
            AZ_Assert(m_missShader, "Failed to load miss shader");

            auto missShaderVariant = m_missShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForRayTracing missShaderDescriptor;
            missShaderVariant.ConfigurePipelineState(missShaderDescriptor);
                       
            // Load closest hit shader
            // This can be removed when the following issue is fixed.
            // [ATOM-15087] RayTracingShaderTable shouldn't report an error if there is no hit group in the descriptor
            const char* hitShaderFilePath = "Shaders/RayTracing/RTAOClosestHit.azshader";
            m_hitShader = RPI::LoadShader(hitShaderFilePath);
            AZ_Assert(m_hitShader, "Failed to load closest hit shader");

            auto hitShaderVariant = m_hitShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForRayTracing hitShaderDescriptor;
            hitShaderVariant.ConfigurePipelineState(hitShaderDescriptor);

            // global pipeline state
            m_globalPipelineState = m_rayGenerationShader->AcquirePipelineState(rayGenerationShaderDescriptor);
            AZ_Assert(m_globalPipelineState, "Failed to acquire ray tracing global pipeline state");

            //Get pass srg
            m_shaderResourceGroup = RPI::ShaderResourceGroup::Create(m_rayGenerationShader->GetAsset(), Name { "RayTracingGlobalSrg" });
            AZ_Assert(m_shaderResourceGroup, "[RayTracingAmbientOcclusionPass '%s']: Failed to create SRG from shader asset '%s'",
                GetPathName().GetCStr(), rayGenerationShaderFilePath);
                        
            RHI::MultiDeviceRayTracingPipelineStateDescriptor descriptor;
            descriptor.Build()
                ->PipelineState(m_globalPipelineState.get())
                ->ShaderLibrary(rayGenerationShaderDescriptor)
                ->RayGenerationShaderName(AZ::Name("AoRayGen"))
                ->ShaderLibrary(missShaderDescriptor)
                ->MissShaderName(AZ::Name("AoMiss"))
                ->ShaderLibrary(hitShaderDescriptor)
                ->ClosestHitShaderName(AZ::Name("AoClosestHit"))
                ->HitGroup(AZ::Name("ClosestHitGroup"))
                ->ClosestHitShaderName(AZ::Name("AoClosestHit"))
                ;

            // create the ray tracing pipeline state object
            m_rayTracingPipelineState = aznew RHI::MultiDeviceRayTracingPipelineState;
            m_rayTracingPipelineState->Init(RHI::MultiDevice::AllDevices, descriptor);
        }

        void RayTracingAmbientOcclusionPass::FrameBeginInternal(FramePrepareParams params)
        {
            if (m_createRayTracingPipelineState)
            {
                RPI::Scene* scene = m_pipeline->GetScene();
                m_rayTracingFeatureProcessor = scene->GetFeatureProcessor<RayTracingFeatureProcessor>();

                CreateRayTracingPipelineState();
                m_createRayTracingPipelineState = false;
            }

            if (!m_rayTracingShaderTable)
            {
                RHI::MultiDeviceRayTracingBufferPools& rayTracingBufferPools = m_rayTracingFeatureProcessor->GetBufferPools();

                // Build shader table once. Since we are not using local srg so we don't need to rebuild it even when scene changed 
                m_rayTracingShaderTable = aznew RHI::MultiDeviceRayTracingShaderTable;
                m_rayTracingShaderTable->Init(RHI::MultiDevice::AllDevices, rayTracingBufferPools);

                AZStd::shared_ptr<RHI::MultiDeviceRayTracingShaderTableDescriptor> descriptor = AZStd::make_shared<RHI::MultiDeviceRayTracingShaderTableDescriptor>();
                descriptor->Build(AZ::Name("RayTracingAOShaderTable"), m_rayTracingPipelineState)
                    ->RayGenerationRecord(AZ::Name("AoRayGen"))
                    ->MissRecord(AZ::Name("AoMiss"))
                    ->HitGroupRecord(AZ::Name("ClosestHitGroup"))
                    ;

                m_rayTracingShaderTable->Build(descriptor);
            }

            RenderPass::FrameBeginInternal(params);
        }        

        void RayTracingAmbientOcclusionPass::SetupFrameGraphDependencies(RHI::FrameGraphInterface frameGraph)
        {
            RenderPass::SetupFrameGraphDependencies(frameGraph);
            frameGraph.SetEstimatedItemCount(1);
        }

        void RayTracingAmbientOcclusionPass::CompileResources(const RHI::FrameGraphCompileContext& context)
        {
            if (!m_shaderResourceGroup)
            {
                return;
            }

            // Bind pass attachments to global srg 
            BindPassSrg(context, m_shaderResourceGroup);

            // Bind others for global srg
            const RHI::ShaderResourceGroupLayout* srgLayout = m_shaderResourceGroup->GetLayout();
            RHI::ShaderInputBufferIndex bufferIndex;
            RHI::ShaderInputConstantIndex constantIndex;

            // Bind scene TLAS buffer
            auto tlasBuffer = m_rayTracingFeatureProcessor->GetTlas()->GetTlasBuffer();
            if (tlasBuffer)
            {
                // TLAS
                uint32_t tlasBufferByteCount = aznumeric_cast<uint32_t>(tlasBuffer->GetDescriptor().m_byteCount);
                RHI::BufferViewDescriptor bufferViewDescriptor = RHI::BufferViewDescriptor::CreateRayTracingTLAS(tlasBufferByteCount);

                bufferIndex = srgLayout->FindShaderInputBufferIndex(AZ::Name("m_scene"));
                m_shaderResourceGroup->SetBufferView(bufferIndex, tlasBuffer->BuildBufferView(bufferViewDescriptor).get());
            }

            // Bind constants
            // float m_aoRadius
            constantIndex = srgLayout->FindShaderInputConstantIndex(AZ::Name("m_aoRadius"));
            m_shaderResourceGroup->SetConstant(constantIndex, m_rayMaxT);

            // uint m_frameCount
            constantIndex = srgLayout->FindShaderInputConstantIndex(AZ::Name("m_frameCount"));
            m_shaderResourceGroup->SetConstant(constantIndex, m_frameCount++);

            // float m_rayMinT
            constantIndex = srgLayout->FindShaderInputConstantIndex(AZ::Name("m_rayMinT"));
            m_shaderResourceGroup->SetConstant(constantIndex, m_rayMinT);

            // uint m_numRays
            constantIndex = srgLayout->FindShaderInputConstantIndex(AZ::Name("m_numRays"));
            m_shaderResourceGroup->SetConstant(constantIndex, m_rayNumber);

            // Matrix4x4 m_viewProjectionInverseMatrix. This is the copy of same constant from ViewSrg.
            // Although we don't have access to ViewSrg in ray tracing shader at this moment
            constantIndex = srgLayout->FindShaderInputConstantIndex(AZ::Name("m_viewProjectionInverseMatrix"));
            const AZStd::vector<RPI::ViewPtr>& views = m_pipeline->GetViews(RPI::PipelineViewTag{"MainCamera"});
            Matrix4x4 clipToWorld = views[0]->GetWorldToClipMatrix();
            clipToWorld.InvertFull();
            m_shaderResourceGroup->SetConstant(constantIndex, clipToWorld);

            m_shaderResourceGroup->Compile();
        }
    
        void RayTracingAmbientOcclusionPass::BuildCommandListInternal([[maybe_unused]] const RHI::FrameGraphExecuteContext& context)
        {
            RPI::Scene* scene = m_pipeline->GetScene();
            RayTracingFeatureProcessor* rayTracingFeatureProcessor = scene->GetFeatureProcessor<RayTracingFeatureProcessor>();
            AZ_Assert(rayTracingFeatureProcessor, "RayTracingAmbientOcclusionPass requires the RayTracingFeatureProcessor");

            if (!rayTracingFeatureProcessor->GetSubMeshCount())
            {
                return;
            }

            if (!m_rayTracingShaderTable)
            {
                return;
            }

            RPI::PassAttachment* outputAttachment = GetOutputBinding(0).GetAttachment().get();
            RHI::Size targetImageSize = outputAttachment->m_descriptor.m_image.m_size;

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] =
            {
                m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::SingleDeviceDispatchRaysItem dispatchRaysItem;
            dispatchRaysItem.m_arguments.m_direct.m_width = targetImageSize.m_width;
            dispatchRaysItem.m_arguments.m_direct.m_height = targetImageSize.m_height;
            dispatchRaysItem.m_arguments.m_direct.m_depth = 1;
            dispatchRaysItem.m_rayTracingPipelineState = m_rayTracingPipelineState->GetDeviceRayTracingPipelineState(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_rayTracingShaderTable = m_rayTracingShaderTable->GetDeviceRayTracingShaderTable(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_shaderResourceGroupCount = 1;
            dispatchRaysItem.m_shaderResourceGroups = shaderResourceGroups;
            dispatchRaysItem.m_globalPipelineState = m_globalPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();

            // submit the DispatchRays item
            context.GetCommandList()->Submit(dispatchRaysItem);
        }

        uint32_t RayTracingAmbientOcclusionPass::GetRayNumberPerPixel()
        {
            return m_rayNumber;
        }

        void RayTracingAmbientOcclusionPass::SetRayNumberPerPixel(uint32_t rayNumber)
        {
            m_rayNumber = rayNumber;
        }

        float RayTracingAmbientOcclusionPass::GetRayExtentMin()
        {
            return m_rayMinT;
        }

        void RayTracingAmbientOcclusionPass::SetRayExtentMin(float minT)
        {
            m_rayMinT = minT;
        }

        float RayTracingAmbientOcclusionPass::GetRayExtentMax()
        {
            return m_rayMaxT;
        }

        void RayTracingAmbientOcclusionPass::SetRayExtentMax(float maxT)
        {
            m_rayMaxT = maxT;
        }
    }   // namespace RPI
}   // namespace AZ
