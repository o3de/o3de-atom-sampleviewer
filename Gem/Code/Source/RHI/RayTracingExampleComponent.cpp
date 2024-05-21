/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/RayTracingExampleComponent.h>
#include <Utils/Utils.h>
#include <SampleComponentManager.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/MultiDeviceRayTracingPipelineState.h>
#include <Atom/RHI/MultiDeviceRayTracingShaderTable.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

static const char* RayTracingExampleName = "RayTracingExample";

namespace AtomSampleViewer
{
    void RayTracingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RayTracingExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    RayTracingExampleComponent::RayTracingExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void RayTracingExampleComponent::Activate()
    {
        CreateResourcePools();
        CreateGeometry();
        CreateFullScreenBuffer();
        CreateOutputTexture();
        CreateRasterShader();
        CreateRayTracingAccelerationStructureObjects();
        CreateRayTracingPipelineState();
        CreateRayTracingShaderTable();
        CreateRayTracingAccelerationTableScope();
        CreateRayTracingDispatchScope();
        CreateRasterScope();

        RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void RayTracingExampleComponent::Deactivate()
    {
        RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void RayTracingExampleComponent::CreateResourcePools()
    {
        // create input assembly buffer pool
        {
            m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
            [[maybe_unused]] RHI::ResultCode resultCode = m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "Failed to initialize input assembly buffer pool");
        }

        // create output image pool
        {
            RHI::ImagePoolDescriptor imagePoolDesc;
            imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::ShaderReadWrite;

            m_imagePool = aznew RHI::MultiDeviceImagePool();
            [[maybe_unused]] RHI::ResultCode result = m_imagePool->Init(RHI::MultiDevice::AllDevices, imagePoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image pool");
        }

        // initialize ray tracing buffer pools
        m_rayTracingBufferPools = aznew RHI::MultiDeviceRayTracingBufferPools;
        m_rayTracingBufferPools->Init(RHI::MultiDevice::AllDevices);
    }

    void RayTracingExampleComponent::CreateGeometry()
    {
        // triangle
        {
            // vertex buffer
            SetVertexPosition(m_triangleVertices.data(), 0, 0.0f, 0.5f, 1.0);
            SetVertexPosition(m_triangleVertices.data(), 1, 0.5f, -0.5f, 1.0);
            SetVertexPosition(m_triangleVertices.data(), 2, -0.5f, -0.5f, 1.0);

            m_triangleVB = aznew RHI::MultiDeviceBuffer();
            m_triangleVB->SetName(AZ::Name("Triangle VB"));

            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_triangleVB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_triangleVertices) };
            request.m_initialData = m_triangleVertices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);

            // index buffer
            SetVertexIndexIncreasing(m_triangleIndices.data(), m_triangleIndices.size());

            m_triangleIB = aznew RHI::MultiDeviceBuffer();
            m_triangleIB->SetName(AZ::Name("Triangle IB"));

            request.m_buffer = m_triangleIB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_triangleIndices) };
            request.m_initialData = m_triangleIndices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);
        }

        // rectangle
        {
            // vertex buffer
            SetVertexPosition(m_rectangleVertices.data(), 0, -0.5f,  0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 1,  0.5f,  0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 2,  0.5f, -0.5f, 1.0);
            SetVertexPosition(m_rectangleVertices.data(), 3, -0.5f, -0.5f, 1.0);

            m_rectangleVB = aznew RHI::MultiDeviceBuffer();
            m_rectangleVB->SetName(AZ::Name("Rectangle VB"));

            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_rectangleVB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_rectangleVertices) };
            request.m_initialData = m_rectangleVertices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);

            // index buffer
            m_rectangleIndices[0] = 0;
            m_rectangleIndices[1] = 1;
            m_rectangleIndices[2] = 2;
            m_rectangleIndices[3] = 0;
            m_rectangleIndices[4] = 2;
            m_rectangleIndices[5] = 3;

            m_rectangleIB = aznew RHI::MultiDeviceBuffer();
            m_rectangleIB->SetName(AZ::Name("Rectangle IB"));

            request.m_buffer = m_rectangleIB.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(m_rectangleIndices) };
            request.m_initialData = m_rectangleIndices.data();
            m_inputAssemblyBufferPool->InitBuffer(request);
        }
    }

    void RayTracingExampleComponent::CreateFullScreenBuffer()
    {
        FullScreenBufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_fullScreenInputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();

        RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_fullScreenInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        m_inputAssemblyBufferPool->InitBuffer(request);

        m_fullScreenStreamBufferViews[0] =
        {
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_positions),
            sizeof(FullScreenBufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_fullScreenStreamBufferViews[1] =
        {
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_uvs),
            sizeof(FullScreenBufferData::m_uvs),
            sizeof(VertexUV)
        };

        m_fullScreenIndexBufferView =
        {
            *m_fullScreenInputAssemblyBuffer,
            offsetof(FullScreenBufferData, m_indices),
            sizeof(FullScreenBufferData::m_indices),
            RHI::IndexFormat::Uint16
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_fullScreenInputStreamLayout = layoutBuilder.End();
    }

    void RayTracingExampleComponent::CreateOutputTexture()
    {
        // create output image
        m_outputImage = aznew RHI::MultiDeviceImage();

        RHI::MultiDeviceImageInitRequest request;
        request.m_image = m_outputImage.get();
        request.m_descriptor = RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::ShaderReadWrite, m_imageWidth, m_imageHeight, RHI::Format::R8G8B8A8_UNORM);
        [[maybe_unused]] RHI::ResultCode result = m_imagePool->InitImage(request);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize output image");

        m_outputImageViewDescriptor = RHI::ImageViewDescriptor::Create(RHI::Format::R8G8B8A8_UNORM, 0, 0);
        m_outputImageView = m_outputImage->BuildImageView(m_outputImageViewDescriptor);
        AZ_Assert(m_outputImageView.get(), "Failed to create output image view");
        AZ_Assert(m_outputImageView->GetDeviceImageView(RHI::MultiDevice::DefaultDeviceIndex)->IsFullView(), "Image View initialization IsFullView() failed");
    }

    void RayTracingExampleComponent::CreateRayTracingAccelerationStructureObjects()
    {
        m_triangleRayTracingBlas = aznew AZ::RHI::MultiDeviceRayTracingBlas;
        m_rectangleRayTracingBlas = aznew AZ::RHI::MultiDeviceRayTracingBlas;
        m_rayTracingTlas = aznew AZ::RHI::MultiDeviceRayTracingTlas;
    }

    void RayTracingExampleComponent::CreateRasterShader()
    {
        const char* shaderFilePath = "Shaders/RHI/RayTracingDraw.azshader";

        auto drawShader = LoadShader(shaderFilePath, RayTracingExampleName);
        AZ_Assert(drawShader, "Failed to load Draw shader");

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        drawShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_fullScreenInputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create draw render attachment layout");

        m_drawPipelineState = drawShader->AcquirePipelineState(pipelineDesc);
        AZ_Assert(m_drawPipelineState, "Failed to acquire draw pipeline state");

        m_drawSRG = CreateShaderResourceGroup(drawShader, "BufferSrg", RayTracingExampleName);
    }

    void RayTracingExampleComponent::CreateRayTracingPipelineState()
    {
        // load ray generation shader
        const char* rayGenerationShaderFilePath = "Shaders/RHI/RayTracingDispatch.azshader";
        m_rayGenerationShader = LoadShader(rayGenerationShaderFilePath, RayTracingExampleName);
        AZ_Assert(m_rayGenerationShader, "Failed to load ray generation shader");

        auto rayGenerationShaderVariant = m_rayGenerationShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing rayGenerationShaderDescriptor;
        rayGenerationShaderVariant.ConfigurePipelineState(rayGenerationShaderDescriptor);

        // load miss shader
        const char* missShaderFilePath = "Shaders/RHI/RayTracingMiss.azshader";
        m_missShader = LoadShader(missShaderFilePath, RayTracingExampleName);
        AZ_Assert(m_missShader, "Failed to load miss shader");

        auto missShaderVariant = m_missShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing missShaderDescriptor;
        missShaderVariant.ConfigurePipelineState(missShaderDescriptor);

        // load closest hit gradient shader
        const char* closestHitGradientShaderFilePath = "Shaders/RHI/RayTracingClosestHitGradient.azshader";
        m_closestHitGradientShader = LoadShader(closestHitGradientShaderFilePath, RayTracingExampleName);
        AZ_Assert(m_closestHitGradientShader, "Failed to load closest hit gradient shader");

        auto closestHitGradientShaderVariant = m_closestHitGradientShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing closestHitGradiantShaderDescriptor;
        closestHitGradientShaderVariant.ConfigurePipelineState(closestHitGradiantShaderDescriptor);

        // load closest hit solid shader
        const char* closestHitSolidShaderFilePath = "Shaders/RHI/RayTracingClosestHitSolid.azshader";
        m_closestHitSolidShader = LoadShader(closestHitSolidShaderFilePath, RayTracingExampleName);
        AZ_Assert(m_closestHitSolidShader, "Failed to load closest hit solid shader");

        auto closestHitSolidShaderVariant = m_closestHitSolidShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        RHI::PipelineStateDescriptorForRayTracing closestHitSolidShaderDescriptor;
        closestHitSolidShaderVariant.ConfigurePipelineState(closestHitSolidShaderDescriptor);

        // global pipeline state and srg
        m_globalPipelineState = m_rayGenerationShader->AcquirePipelineState(rayGenerationShaderDescriptor);
        AZ_Assert(m_globalPipelineState, "Failed to acquire ray tracing global pipeline state");
        m_globalSrg = CreateShaderResourceGroup(m_rayGenerationShader, "RayTracingGlobalSrg", RayTracingExampleName);

        // build the ray tracing pipeline state descriptor
        RHI::MultiDeviceRayTracingPipelineStateDescriptor descriptor;
        descriptor.Build()
            ->PipelineState(m_globalPipelineState.get())
            ->ShaderLibrary(rayGenerationShaderDescriptor)
                ->RayGenerationShaderName(AZ::Name("RayGenerationShader"))
            ->ShaderLibrary(missShaderDescriptor)
                ->MissShaderName(AZ::Name("MissShader"))
            ->ShaderLibrary(closestHitGradiantShaderDescriptor)
                ->ClosestHitShaderName(AZ::Name("ClosestHitGradientShader"))
            ->ShaderLibrary(closestHitSolidShaderDescriptor)
                ->ClosestHitShaderName(AZ::Name("ClosestHitSolidShader"))
            ->HitGroup(AZ::Name("HitGroupGradient"))
                ->ClosestHitShaderName(AZ::Name("ClosestHitGradientShader"))
            ->HitGroup(AZ::Name("HitGroupSolid"))
                ->ClosestHitShaderName(AZ::Name("ClosestHitSolidShader"));
    
        // create the ray tracing pipeline state object
        m_rayTracingPipelineState = aznew RHI::MultiDeviceRayTracingPipelineState;
        m_rayTracingPipelineState->Init(RHI::MultiDevice::AllDevices, descriptor);
    }

    void RayTracingExampleComponent::CreateRayTracingShaderTable()
    {
        m_rayTracingShaderTable = aznew RHI::MultiDeviceRayTracingShaderTable;
        m_rayTracingShaderTable->Init(RHI::MultiDevice::AllDevices, *m_rayTracingBufferPools);
    }

    void RayTracingExampleComponent::CreateRayTracingAccelerationTableScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // create triangle BLAS buffer if necessary
            if (!m_triangleRayTracingBlas->IsValid())
            {
                RHI::MultiDeviceStreamBufferView triangleVertexBufferView =
                {
                    *m_triangleVB,
                    0,
                    sizeof(m_triangleVertices),
                    sizeof(VertexPosition)
                };

                RHI::MultiDeviceIndexBufferView triangleIndexBufferView =
                {
                    *m_triangleIB,
                    0,
                    sizeof(m_triangleIndices),
                    RHI::IndexFormat::Uint16
                };

                RHI::MultiDeviceRayTracingBlasDescriptor triangleBlasDescriptor;
                triangleBlasDescriptor.Build()
                    ->Geometry()
                        ->VertexFormat(RHI::Format::R32G32B32_FLOAT)
                        ->VertexBuffer(triangleVertexBufferView)
                        ->IndexBuffer(triangleIndexBufferView)
                ;

                m_triangleRayTracingBlas->CreateBuffers(RHI::MultiDevice::AllDevices, &triangleBlasDescriptor, *m_rayTracingBufferPools);
            }

            // create rectangle BLAS if necessary
            if (!m_rectangleRayTracingBlas->IsValid())
            {
                RHI::MultiDeviceStreamBufferView rectangleVertexBufferView =
                {
                    *m_rectangleVB,
                    0,
                    sizeof(m_rectangleVertices),
                    sizeof(VertexPosition)
                };

                RHI::MultiDeviceIndexBufferView rectangleIndexBufferView =
                {
                    *m_rectangleIB,
                    0,
                    sizeof(m_rectangleIndices),
                    RHI::IndexFormat::Uint16
                };

                RHI::MultiDeviceRayTracingBlasDescriptor rectangleBlasDescriptor;
                rectangleBlasDescriptor.Build()
                    ->Geometry()
                        ->VertexFormat(RHI::Format::R32G32B32_FLOAT)
                        ->VertexBuffer(rectangleVertexBufferView)
                        ->IndexBuffer(rectangleIndexBufferView)
                ;

                m_rectangleRayTracingBlas->CreateBuffers(RHI::MultiDevice::AllDevices, &rectangleBlasDescriptor, *m_rayTracingBufferPools);
            }

            m_time += 0.005f;

            // transforms
            AZ::Transform triangleTransform1 = AZ::Transform::CreateIdentity();
            triangleTransform1.SetTranslation(sinf(m_time) * -100.0f, cosf(m_time) * -100.0f, 1.0f);
            triangleTransform1.MultiplyByUniformScale(100.0f);

            AZ::Transform triangleTransform2 = AZ::Transform::CreateIdentity();
            triangleTransform2.SetTranslation(sinf(m_time) * -100.0f, cosf(m_time) * 100.0f, 2.0f);
            triangleTransform2.MultiplyByUniformScale(100.0f);

            AZ::Transform triangleTransform3 = AZ::Transform::CreateIdentity();
            triangleTransform3.SetTranslation(sinf(m_time) * 100.0f, cosf(m_time) * 100.0f, 3.0f);
            triangleTransform3.MultiplyByUniformScale(100.0f);

            AZ::Transform rectangleTransform = AZ::Transform::CreateIdentity();
            rectangleTransform.SetTranslation(sinf(m_time) * 100.0f, cosf(m_time) * -100.0f, 4.0f);
            rectangleTransform.MultiplyByUniformScale(100.0f);

            // create the TLAS
            RHI::MultiDeviceRayTracingTlasDescriptor tlasDescriptor;
            tlasDescriptor.Build()
                ->Instance()
                    ->InstanceID(0)
                    ->HitGroupIndex(0)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform1)
                ->Instance()
                    ->InstanceID(1)
                    ->HitGroupIndex(1)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform2)
                ->Instance()
                    ->InstanceID(2)
                    ->HitGroupIndex(2)
                    ->Blas(m_triangleRayTracingBlas)
                    ->Transform(triangleTransform3)
                ->Instance()
                    ->InstanceID(3)
                    ->HitGroupIndex(3)
                    ->Blas(m_rectangleRayTracingBlas)
                    ->Transform(rectangleTransform)
                ;

            m_rayTracingTlas->CreateBuffers(RHI::MultiDevice::AllDevices, &tlasDescriptor, *m_rayTracingBufferPools);

            m_tlasBufferViewDescriptor = RHI::BufferViewDescriptor::CreateRaw(0, (uint32_t)m_rayTracingTlas->GetTlasBuffer()->GetDescriptor().m_byteCount);

            [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportBuffer(m_tlasBufferAttachmentId, m_rayTracingTlas->GetTlasBuffer());
            AZ_Error(RayTracingExampleName, result == RHI::ResultCode::Success, "Failed to import TLAS buffer with error %d", result);

            RHI::BufferScopeAttachmentDescriptor desc;
            desc.m_attachmentId = m_tlasBufferAttachmentId;
            desc.m_bufferViewDescriptor = m_tlasBufferViewDescriptor;
            desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;

            frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            commandList->BuildBottomLevelAccelerationStructure(*m_triangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()));
            commandList->BuildBottomLevelAccelerationStructure(*m_rectangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()));
            commandList->BuildTopLevelAccelerationStructure(
                *m_rayTracingTlas->GetDeviceRayTracingTlas(context.GetDeviceIndex()), { m_triangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()).get(), m_rectangleRayTracingBlas->GetDeviceRayTracingBlas(context.GetDeviceIndex()).get() });
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "RayTracingBuildAccelerationStructure" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void RayTracingExampleComponent::CreateRayTracingDispatchScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach output image
            {
                [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportImage(m_outputImageAttachmentId, m_outputImage);
                AZ_Error(RayTracingExampleName, result == RHI::ResultCode::Success, "Failed to import output image with error %d", result);

                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_outputImageAttachmentId;
                desc.m_imageViewDescriptor = m_outputImageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            // attach TLAS buffer
            if (m_rayTracingTlas->GetTlasBuffer())
            {
                RHI::BufferScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_tlasBufferAttachmentId;
                desc.m_bufferViewDescriptor = m_tlasBufferViewDescriptor;
                desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this]([[maybe_unused]] const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (m_rayTracingTlas->GetTlasBuffer())
            {
                // set the TLAS and output image in the ray tracing global Srg
                RHI::ShaderInputBufferIndex tlasConstantIndex;
                FindShaderInputIndex(&tlasConstantIndex, m_globalSrg, AZ::Name{ "m_scene" }, RayTracingExampleName);

                uint32_t tlasBufferByteCount = aznumeric_cast<uint32_t>(m_rayTracingTlas->GetTlasBuffer()->GetDescriptor().m_byteCount);
                RHI::BufferViewDescriptor bufferViewDescriptor = RHI::BufferViewDescriptor::CreateRayTracingTLAS(tlasBufferByteCount);
                m_globalSrg->SetBufferView(tlasConstantIndex, m_rayTracingTlas->GetTlasBuffer()->BuildBufferView(bufferViewDescriptor).get());

                RHI::ShaderInputImageIndex outputConstantIndex;
                FindShaderInputIndex(&outputConstantIndex, m_globalSrg, AZ::Name{ "m_output" }, RayTracingExampleName);
                m_globalSrg->SetImageView(outputConstantIndex, m_outputImageView.get());

                // set hit shader data, each array element corresponds to the InstanceIndex() of the geometry in the TLAS
                // Note: this method is used instead of LocalRootSignatures for compatibility with non-DX12 platforms

                // set HitGradient values
                RHI::ShaderInputConstantIndex hitGradientDataConstantIndex;
                FindShaderInputIndex(&hitGradientDataConstantIndex, m_globalSrg, AZ::Name{"m_hitGradientData"}, RayTracingExampleName);

                struct HitGradientData
                {
                    AZ::Vector4 m_color;
                };

                AZStd::array<HitGradientData, 4> hitGradientData = {{
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f)}, // triangle1
                    {AZ::Vector4(0.0f, 1.0f, 0.0f, 1.0f)}, // triangle2
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                }};

                m_globalSrg->SetConstantArray(hitGradientDataConstantIndex, hitGradientData);

                // set HitSolid values
                RHI::ShaderInputConstantIndex hitSolidDataConstantIndex;
                FindShaderInputIndex(&hitSolidDataConstantIndex, m_globalSrg, AZ::Name{"m_hitSolidData"}, RayTracingExampleName);

                struct HitSolidData
                {
                    AZ::Vector4 m_color1;
                    float m_lerp;
                    float m_pad[3];
                    AZ::Vector4 m_color2;
                };

                AZStd::array<HitSolidData, 4> hitSolidData = {{
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 0.0f, 0.0f)}, // unused
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f), 0.5f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 1.0f, 0.0f, 1.0f)}, // triangle3
                    {AZ::Vector4(1.0f, 0.0f, 0.0f, 1.0f), 0.5f, {0.0f, 0.0f, 0.0f}, AZ::Vector4(0.0f, 0.0f, 1.0f, 1.0f)}, // rectangle
                }};

                m_globalSrg->SetConstantArray(hitSolidDataConstantIndex, hitSolidData);
                m_globalSrg->Compile();

                // update the ray tracing shader table
                AZStd::shared_ptr<RHI::MultiDeviceRayTracingShaderTableDescriptor> descriptor = AZStd::make_shared<RHI::MultiDeviceRayTracingShaderTableDescriptor>();
                descriptor->Build(AZ::Name("RayTracingExampleShaderTable"), m_rayTracingPipelineState)
                    ->RayGenerationRecord(AZ::Name("RayGenerationShader"))
                    ->MissRecord(AZ::Name("MissShader"))
                    ->HitGroupRecord(AZ::Name("HitGroupGradient")) // triangle1
                    ->HitGroupRecord(AZ::Name("HitGroupGradient")) // triangle2
                    ->HitGroupRecord(AZ::Name("HitGroupSolid")) // triangle3
                    ->HitGroupRecord(AZ::Name("HitGroupSolid")) // rectangle
                    ;

                m_rayTracingShaderTable->Build(descriptor);
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (!m_rayTracingTlas->GetTlasBuffer())
            {
                return;
            }

            RHI::CommandList* commandList = context.GetCommandList();

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_globalSrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::SingleDeviceDispatchRaysItem dispatchRaysItem;
            dispatchRaysItem.m_arguments.m_direct.m_width = m_imageWidth;
            dispatchRaysItem.m_arguments.m_direct.m_height = m_imageHeight;
            dispatchRaysItem.m_arguments.m_direct.m_depth = 1;
            dispatchRaysItem.m_rayTracingPipelineState = m_rayTracingPipelineState->GetDeviceRayTracingPipelineState(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_rayTracingShaderTable = m_rayTracingShaderTable->GetDeviceRayTracingShaderTable(context.GetDeviceIndex()).get();
            dispatchRaysItem.m_shaderResourceGroupCount = RHI::ArraySize(shaderResourceGroups);
            dispatchRaysItem.m_shaderResourceGroups = shaderResourceGroups;
            dispatchRaysItem.m_globalPipelineState = m_globalPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();

            // submit the DispatchRays item
            commandList->Submit(dispatchRaysItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "RayTracingDispatch" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void RayTracingExampleComponent::CreateRasterScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach swapchain
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseColorAttachment(descriptor);
            }

            // attach output buffer
            {
                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_outputImageAttachmentId;
                desc.m_imageViewDescriptor = m_outputImageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);

                const Name outputImageId{ "m_output" };
                RHI::ShaderInputImageIndex outputImageIndex = m_drawSRG->FindShaderInputImageIndex(outputImageId);
                AZ_Error(RayTracingExampleName, outputImageIndex.IsValid(), "Failed to find shader input image %s.", outputImageId.GetCStr());
                m_drawSRG->SetImageView(outputImageIndex, m_outputImageView.get());
                m_drawSRG->Compile();
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_drawSRG->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_drawPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_fullScreenIndexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_fullScreenStreamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_fullScreenStreamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_fullScreenStreamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;

            // submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "Raster" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
} // namespace AtomSampleViewer
