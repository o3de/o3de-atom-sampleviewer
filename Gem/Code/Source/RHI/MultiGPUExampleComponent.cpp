/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/MultiGPUExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RHI.Reflect/ImageScopeAttachmentDescriptor.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/CopyItem.h>
#include <Atom/RHI.Reflect/BufferDescriptor.h>

using namespace AZ;

namespace AtomSampleViewer
{
    void MultiGPUExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiGPUExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void MultiGPUExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        static float time = 0.0f;
        time += 0.005f;

        // Move the triangle around.
        AZ::Vector3 translation(
            sinf(time) * 0.25f,
            cosf(time) * 0.25f,
            0.0f);

        if (m_shaderResourceGroupShared)
        {
            [[maybe_unused]] bool success =
                m_shaderResourceGroupShared->SetConstant(m_objectMatrixConstantIndex, AZ::Matrix4x4::CreateTranslation(translation));
            AZ_Warning("MultiGPUExampleComponent", success, "Failed to set SRG Constant m_objectMatrix");
            m_shaderResourceGroupShared->Compile();
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void MultiGPUExampleComponent::FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        if (m_outputWidth != m_imageWidth || m_outputHeight != m_imageHeight)
        {
            // Image used as color attachment
            {
                m_image = aznew RHI::Image;
                RHI::ImageInitRequest initImageRequest;
                initImageRequest.m_image = m_image.get();
                initImageRequest.m_descriptor = RHI::ImageDescriptor::Create2D(
                    RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite | RHI::ImageBindFlags::CopyRead, m_outputWidth, m_outputHeight, m_outputFormat);
                m_imagePool->InitImage(initImageRequest);
            }

            // Image holds rendered texture from GPU1 (on GPU0)
            {
                m_transferImage = aznew RHI::Image;
                RHI::ImageInitRequest initImageRequest;
                initImageRequest.m_image = m_transferImage.get();
                initImageRequest.m_descriptor = RHI::ImageDescriptor::Create2D(
                    RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead | RHI::ImageBindFlags::CopyWrite, m_outputWidth, m_outputHeight,
                    m_outputFormat);
                m_transferImagePool->InitImage(initImageRequest);
            }

            RHI::BufferBindFlags stagingBufferBindFlags{ RHI::BufferBindFlags::CopyWrite | RHI::BufferBindFlags::CopyRead };

            {
                m_stagingBufferToGPU = aznew RHI::Buffer;
                AZStd::vector<unsigned int> initialData(m_outputWidth * m_outputHeight, 0xFFFF00FFu);

                RHI::BufferInitRequest request;
                request.m_buffer = m_stagingBufferToGPU.get();
                request.m_descriptor = RHI::BufferDescriptor{stagingBufferBindFlags, initialData.size() * sizeof(unsigned int)};
                //? Check BindFlags
                request.m_initialData = initialData.data();
                if (m_stagingBufferPoolToGPU->InitBuffer(request) != RHI::ResultCode::Success)
                {
                    AZ_Error("MultiGPUExampleComponent", false, "StagingBufferToGPU was not created");
                }
            }

            {
                m_stagingBufferToCPU = aznew RHI::Buffer;
                RHI::BufferInitRequest request;
                request.m_buffer = m_stagingBufferToCPU.get();
                request.m_descriptor =
                    RHI::BufferDescriptor{ stagingBufferBindFlags, m_outputWidth * m_outputHeight * sizeof(unsigned int) }; //? Check BindFlags
                if (m_stagingBufferPoolToCPU->InitBuffer(request) != RHI::ResultCode::Success)
                {
                    AZ_Error("MultiGPUExampleComponent", false, "StagingBufferToCPU was not created");
                }
            }

            m_scissors[0].m_maxX = m_outputWidth / 2 + 1;
            m_scissors[1].m_minX = m_outputWidth / 2;
            m_scissors[1].m_maxX = m_outputWidth;

            m_imageWidth = m_outputWidth;
            m_imageHeight = m_outputHeight;
        }

        frameGraphBuilder.GetAttachmentDatabase().ImportImage(
            m_imageAttachmentIds[0], m_image);

        frameGraphBuilder.GetAttachmentDatabase().ImportImage(
            m_imageAttachmentIds[1], m_transferImage);

        frameGraphBuilder.GetAttachmentDatabase().ImportBuffer(
            m_bufferAttachmentIds[0], m_stagingBufferToGPU);

        frameGraphBuilder.GetAttachmentDatabase().ImportBuffer(
            m_bufferAttachmentIds[1], m_stagingBufferToCPU);

        RHI::DeviceBufferMapRequest request{};
        request.m_buffer = m_stagingBufferToCPU->GetDeviceBuffer(1).get();
        request.m_byteCount = m_imageWidth * m_imageHeight * sizeof(uint32_t);

        RHI::DeviceBufferMapResponse response{};

        m_stagingBufferPoolToCPU->GetDeviceBufferPool(1)->MapBuffer(request, response);

        [[maybe_unused]] uint32_t* source = reinterpret_cast<uint32_t*>(response.m_data);

        request.m_buffer = m_stagingBufferToGPU->GetDeviceBuffer(0).get();

        m_stagingBufferPoolToGPU->GetDeviceBufferPool(0)->MapBuffer(request, response);

        uint32_t* destination = reinterpret_cast<uint32_t*>(response.m_data);

        //memset(destination, 0x80, request.m_byteCount);

        memcpy(destination, source, request.m_byteCount);

        /*for (auto i= 0 ; i < 1920 * 8; i++)
            destination[i + 1080/2*1920] = 0xFFFFFFFF;*/

        m_stagingBufferPoolToCPU->GetDeviceBufferPool(1)->UnmapBuffer(*m_stagingBufferToCPU->GetDeviceBuffer(1));
        m_stagingBufferPoolToGPU->GetDeviceBufferPool(0)->UnmapBuffer(*m_stagingBufferToGPU->GetDeviceBuffer(0));
    }

    MultiGPUExampleComponent::MultiGPUExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void MultiGPUExampleComponent::Activate()
    {
        AZ_Error("MultiGPUExampleComponent", RHI::RHISystemInterface::Get()->GetDeviceCount() >= 2, "At least 2 devices required to run this sample");

        m_device_1 = RHI::RHISystemInterface::Get()->GetDevice(0);
        m_device_2 = RHI::RHISystemInterface::Get()->GetDevice(1);

        m_deviceMask_1 = RHI::MultiDevice::DeviceMask{ 1u << 0 };
        m_deviceMask_2 = RHI::MultiDevice::DeviceMask{ 1u << 1 };
        m_deviceMask = m_deviceMask_1 | m_deviceMask_2;

        // Create multi-device resources

        // ImagePool for the render target texture
        {
            m_imagePool = aznew RHI::ImagePool;
            m_imagePool->SetName(Name("RenderTexturePool"));

            RHI::ImagePoolDescriptor imagePoolDescriptor{};
            imagePoolDescriptor.m_bindFlags =
                RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite | RHI::ImageBindFlags::CopyRead | RHI::ImageBindFlags::CopyWrite;

            if (m_imagePool->Init(m_deviceMask, imagePoolDescriptor) != RHI::ResultCode::Success)
            {
                AZ_Error("MultiGPUExampleComponent", false, "Failed to initialize render texture image pool.");
                return;
            }
        }

        // ImagePool used to transfer the rendered texture from GPU 1 -> GPU 0
        {
            m_transferImagePool = aznew RHI::ImagePool;
            m_transferImagePool->SetName(Name("TransferImagePool"));

            RHI::ImagePoolDescriptor imagePoolDescriptor{};
            imagePoolDescriptor.m_bindFlags =
                RHI::ImageBindFlags::ShaderReadWrite | RHI::ImageBindFlags::CopyRead | RHI::ImageBindFlags::CopyWrite;

            if (m_transferImagePool->Init(m_deviceMask_1, imagePoolDescriptor) != RHI::ResultCode::Success)
            {
                AZ_Error("MultiGPUExampleComponent", false, "Failed to initialize transfer image pool.");
                return;
            }
        }

        RHI::BufferBindFlags stagingBufferBindFlags{ RHI::BufferBindFlags::CopyWrite | RHI::BufferBindFlags::CopyRead };

        // Create staging buffer pool for buffer copy to the GPU
        {
            m_stagingBufferPoolToGPU = aznew RHI::BufferPool;

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = stagingBufferBindFlags;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
            bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
            if (m_stagingBufferPoolToGPU->Init(m_deviceMask_1, bufferPoolDesc) != RHI::ResultCode::Success)
            {
                AZ_Error("MultiGPUExampleComponent", false, "StagingBufferPoolToGPU was not initialized");
            }
        }

        // Create staging buffer pools for buffer copy to the CPU
        {
            m_stagingBufferPoolToCPU = aznew RHI::BufferPool;

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = stagingBufferBindFlags;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
            bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Read;
            if (m_stagingBufferPoolToCPU->Init(m_deviceMask_2, bufferPoolDesc) != RHI::ResultCode::Success)
            {
                AZ_Error("MultiGPUExampleComponent", false, "StagingBufferPoolToCPU was not created");
            }
        }

        // Setup main and secondary pipeline
        CreateRenderScopeProducer();
        CreateCopyToCPUScopeProducer();
        CreateCopyToGPUScopeProducer();
        CreateCompositeScopeProducer();

        RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MultiGPUExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroupShared = nullptr;

        m_stagingBufferPoolToGPU = nullptr;
        m_stagingBufferToGPU = nullptr;
        m_inputAssemblyBufferPoolComposite = nullptr;
        m_inputAssemblyBufferComposite = nullptr;
        m_pipelineStateComposite = nullptr;
        m_shaderResourceGroupComposite = nullptr;
        m_shaderResourceGroupDataComposite = RHI::ShaderResourceGroupData{};
        m_shaderResourceGroupPoolComposite = nullptr;

        m_stagingBufferPoolToCPU = nullptr;
        m_stagingBufferToCPU = nullptr;

        RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
        m_secondaryScopeProducers.clear();
    }

    void MultiGPUExampleComponent::CreateRenderScopeProducer()
    {
        RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_inputAssemblyBufferPool = aznew RHI::BufferPool;

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(m_deviceMask, bufferPoolDesc);

            BufferDataTrianglePass bufferData;

            SetVertexPosition(bufferData.m_positions.data(), 0,  0.0,  0.5, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 1, -0.5, -0.5, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 2,  0.5, -0.5, 0.0);

            SetVertexColor(bufferData.m_colors.data(), 0, 1.0, 0.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 1.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);

            SetVertexIndexIncreasing(bufferData.m_indices.data(), bufferData.m_indices.size());

            m_inputAssemblyBuffer = aznew RHI::Buffer;

            RHI::BufferInitRequest request;
            request.m_buffer = m_inputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            m_inputAssemblyBufferPool->InitBuffer(request);

            m_streamBufferViews[0] = { *m_inputAssemblyBuffer,
                                       offsetof(BufferDataTrianglePass, m_positions), sizeof(BufferDataTrianglePass::m_positions),
                                       sizeof(VertexPosition) };

            m_streamBufferViews[1] = { *m_inputAssemblyBuffer,
                                       offsetof(BufferDataTrianglePass, m_colors), sizeof(BufferDataTrianglePass::m_colors),
                                       sizeof(VertexColor) };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        {
            const char* triangleShaderFilePath = "Shaders/RHI/triangle.azshader";
            const char* sampleName = "MultiGPUExample";

            auto shader = LoadShader(triangleShaderFilePath, sampleName);
            if (shader == nullptr)
                return;

            auto shaderOptionGroup = shader->CreateShaderOptionGroup();
            shaderOptionGroup.SetUnspecifiedToDefaultValues();

            // This is an example of how to set different shader options when searching for the shader variant you want to display
            // Searching by id is simple, but suboptimal. Here it's only used to demonstrate the principle,
            // but in practice the ShaderOptionIndex and the ShaderOptionValue should be cached for better performance
            // You can also try DrawMode::Green, DrawMode::Blue or DrawMode::White. The specified color will appear on top of the triangle.
            shaderOptionGroup.SetValue(AZ::Name("o_drawMode"),  AZ::Name("DrawMode::Red"));

            auto shaderVariant = shader->GetVariant(shaderOptionGroup.GetShaderVariantId());

            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", triangleShaderFilePath);
                return;
            }

            m_shaderResourceGroupShared = CreateShaderResourceGroup(shader, "TriangleInstanceSrg", sampleName);

            const Name objectMatrixConstantId{ "m_objectMatrix" };
            FindShaderInputIndex(&m_objectMatrixConstantIndex, m_shaderResourceGroupShared, objectMatrixConstantId, sampleName);

            // In practice m_shaderResourceGroupShared should be one of the cached SRGs owned by the DrawItem
            if (!shaderVariant.IsFullyBaked() && m_shaderResourceGroupShared->HasShaderVariantKeyFallbackEntry())
            {
                // Normally if the requested variant isn't an exact match we have to set it by SetShaderVariantKeyFallbackValue
                // In most cases this should be the preferred behavior:
                m_shaderResourceGroupShared->SetShaderVariantKeyFallbackValue(shaderOptionGroup.GetShaderVariantKeyFallbackValue());
                AZ_Warning(
                    sampleName, false, "Check the Triangle.shader file - some program variants haven't been baked ('%s')",
                    triangleShaderFilePath);
            }
        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
                bool second{false};
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment. Clears it to white.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_imageAttachmentIds[0];
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                descriptor.m_loadStoreAction.m_clearValue.m_vector4Uint = {0, 0, 0, 0};
                frameGraph.UseColorAttachment(descriptor);

                // We will submit a single draw item.
                frameGraph.SetEstimatedItemCount(1);
            };

            RHI::EmptyCompileFunction<ScopeData> compileFunction;

            const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                // Set persistent viewport and scissor state.
                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissors[int(scopeData.second)], 1);

                const RHI::DeviceIndexBufferView indexBufferView = { *m_inputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                                                                     offsetof(BufferDataTrianglePass, m_indices),
                                                                     sizeof(BufferDataTrianglePass::m_indices), RHI::IndexFormat::Uint16 };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 3;
                drawIndexed.m_instanceCount = 1;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroupShared->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                    m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                // Submit the triangle draw item.
                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(
                aznew
                    RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                        RHI::ScopeId{ "MultiGPUTriangle0" }, ScopeData{}, prepareFunction, compileFunction, executeFunction, 0));

            m_scopeProducers.emplace_back(
                aznew
                    RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                        RHI::ScopeId{ "MultiGPUTriangle1" }, ScopeData{true}, prepareFunction, compileFunction, executeFunction, 1));
        }
    }

    void MultiGPUExampleComponent::CreateCompositeScopeProducer()
    {
        BufferDataCompositePass bufferData;
        RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        // Setup input assembly for fullscreen pass
        {
            m_inputAssemblyBufferPoolComposite = aznew RHI::BufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPoolComposite->Init(m_deviceMask_1, bufferPoolDesc);

            SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

            m_inputAssemblyBufferComposite = aznew RHI::Buffer;

            RHI::BufferInitRequest request;
            request.m_buffer = m_inputAssemblyBufferComposite.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            m_inputAssemblyBufferPoolComposite->InitBuffer(request);

            m_streamBufferViewsComposite[0] = { *m_inputAssemblyBufferComposite,
                                                offsetof(BufferDataCompositePass, m_positions),
                                                sizeof(BufferDataCompositePass::m_positions), sizeof(VertexPosition) };

            m_streamBufferViewsComposite[1] = { *m_inputAssemblyBufferComposite,
                                                offsetof(BufferDataCompositePass, m_uvs), sizeof(BufferDataCompositePass::m_uvs),
                                                sizeof(VertexUV) };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViewsComposite);
        }

        // Load shader and connect inputs
        {
            const char* compositeShaderFilePath = "Shaders/RHI/multigpucomposite.azshader";
            const char* sampleName = "MultiGPUExample";

            auto shader = LoadShader(compositeShaderFilePath, sampleName);
            if (shader == nullptr)
            {
                AZ_Error("MultiGPUExampleComponent", false, "Could not load shader");
                return;
            }

            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] RHI::ResultCode result =
                attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineStateComposite = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineStateComposite)
            {
                AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", compositeShaderFilePath);
                return;
            }

            RHI::ShaderResourceGroupPoolDescriptor srgPoolDescriptor{};
            srgPoolDescriptor.m_layout = shader->GetAsset()->FindShaderResourceGroupLayout(AZ::Name { "CompositeSrg" }, shader->GetSupervariantIndex()).get();

            m_shaderResourceGroupPoolComposite = aznew RHI::ShaderResourceGroupPool;
            m_shaderResourceGroupPoolComposite->Init(m_deviceMask_1, srgPoolDescriptor);

            m_shaderResourceGroupComposite = aznew RHI::ShaderResourceGroup;
            m_shaderResourceGroupPoolComposite->InitGroup(*m_shaderResourceGroupComposite);

            m_shaderResourceGroupDataComposite = RHI::ShaderResourceGroupData{*m_shaderResourceGroupPoolComposite};

            {
                const AZ::Name inputTextureShaderInput{ "m_inputTextureLeft" };
                m_textureInputIndices[0] = srgPoolDescriptor.m_layout->FindShaderInputImageIndex(inputTextureShaderInput);
            }
            {
                const AZ::Name inputTextureShaderInput{ "m_inputTextureRight" };
                m_textureInputIndices[1] = srgPoolDescriptor.m_layout->FindShaderInputImageIndex(inputTextureShaderInput);
            }
        }

        // Setup ScopeProducer
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor{};
                    descriptor.m_attachmentId = m_imageAttachmentIds[0];
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                    frameGraph.UseShaderAttachment(descriptor, RHI::ScopeAttachmentAccess::Read);
                }

                {
                    RHI::ImageScopeAttachmentDescriptor descriptor{};
                    descriptor.m_attachmentId = m_imageAttachmentIds[1];
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                    frameGraph.UseShaderAttachment(descriptor, RHI::ScopeAttachmentAccess::Read);
                }

                {
                    RHI::ImageScopeAttachmentDescriptor desc{};
                    desc.m_attachmentId = m_outputAttachmentId;
                    frameGraph.UseColorAttachment(desc);
                }

                frameGraph.SetEstimatedItemCount(1);

                frameGraph.ExecuteAfter(RHI::ScopeId{ "MultiGPUTriangle0" });
                frameGraph.ExecuteAfter(RHI::ScopeId{ "MultiGPUCopyToGPU" });
            };

            const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                m_shaderResourceGroupDataComposite.SetImageView(m_textureInputIndices[0], context.GetImageView(m_imageAttachmentIds[0]));
                m_shaderResourceGroupDataComposite.SetImageView(m_textureInputIndices[1], context.GetImageView(m_imageAttachmentIds[1]));

                m_shaderResourceGroupComposite->Compile(m_shaderResourceGroupDataComposite);
            };

            const auto executeFunction = [=](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                const RHI::DeviceIndexBufferView indexBufferView = {
                    *m_inputAssemblyBufferComposite->GetDeviceBuffer(context.GetDeviceIndex()),
                    offsetof(BufferDataCompositePass, m_indices), sizeof(BufferDataCompositePass::m_indices), RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroupComposite->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineStateComposite->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViewsComposite.size());
                AZStd::array<RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                    m_streamBufferViewsComposite[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViewsComposite[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(
                aznew
                    RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                        RHI::ScopeId{ "MultiGPUComposite" }, ScopeData{}, prepareFunction, compileFunction, executeFunction));
        }
    }

    void MultiGPUExampleComponent::CreateCopyToGPUScopeProducer()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                RHI::ImageScopeAttachmentDescriptor descriptor{};
                descriptor.m_attachmentId = m_imageAttachmentIds[1];
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                frameGraph.UseCopyAttachment(descriptor, RHI::ScopeAttachmentAccess::Write);
            }
        };

        const auto compileFunction = []([[maybe_unused]] const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::DeviceCopyBufferToImageDescriptor copyDescriptor{};
            copyDescriptor.m_sourceBuffer = m_stagingBufferToGPU->GetDeviceBuffer(context.GetDeviceIndex()).get();
            copyDescriptor.m_sourceOffset = 0;
            copyDescriptor.m_sourceBytesPerRow = m_imageWidth * sizeof(uint32_t);
            copyDescriptor.m_sourceBytesPerImage = static_cast<uint32_t>(m_stagingBufferToGPU->GetDescriptor().m_byteCount);
            copyDescriptor.m_sourceSize = RHI::Size{ m_imageWidth, m_imageHeight, 1 };
            copyDescriptor.m_destinationImage = m_transferImage->GetDeviceImage(context.GetDeviceIndex()).get();

            RHI::DeviceCopyItem copyItem(copyDescriptor);
            context.GetCommandList()->Submit(copyItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                RHI::ScopeId{ "MultiGPUCopyToGPU" }, ScopeData{}, prepareFunction, compileFunction, executeFunction));
    }

    void MultiGPUExampleComponent::CreateCopyToCPUScopeProducer()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this]([[maybe_unused]] RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                RHI::BufferScopeAttachmentDescriptor descriptor{};
                descriptor.m_attachmentId = m_bufferAttachmentIds[1];
                descriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateRaw(0, m_stagingBufferToCPU->GetDescriptor().m_byteCount);
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                frameGraph.UseCopyAttachment(descriptor, RHI::ScopeAttachmentAccess::Write);
            }

            {
                RHI::ImageScopeAttachmentDescriptor descriptor{};
                descriptor.m_attachmentId = m_imageAttachmentIds[0];
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                frameGraph.UseCopyAttachment(descriptor, RHI::ScopeAttachmentAccess::Read);
            }

            frameGraph.ExecuteAfter(RHI::ScopeId{ "MultiGPUTriangle1" });
        };

        const auto compileFunction = []([[maybe_unused]] const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::DeviceCopyImageToBufferDescriptor copyDescriptor{};
            copyDescriptor.m_sourceImage = m_image->GetDeviceImage(context.GetDeviceIndex()).get();
            copyDescriptor.m_sourceSize = RHI::Size{ m_imageWidth, m_imageHeight, 1 };
            copyDescriptor.m_destinationBuffer = m_stagingBufferToCPU->GetDeviceBuffer(context.GetDeviceIndex()).get();
            copyDescriptor.m_destinationOffset = 0;
            copyDescriptor.m_destinationBytesPerRow = m_imageWidth * sizeof(uint32_t);
            copyDescriptor.m_destinationBytesPerImage = static_cast<uint32_t>(m_stagingBufferToCPU->GetDescriptor().m_byteCount);
            copyDescriptor.m_destinationFormat = m_outputFormat;

            RHI::DeviceCopyItem copyItem(copyDescriptor);
            context.GetCommandList()->Submit(copyItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<ScopeData, decltype(prepareFunction), decltype(compileFunction), decltype(executeFunction)>(
                RHI::ScopeId{ "MultiGPUCopyToCPU" }, ScopeData{}, prepareFunction, compileFunction, executeFunction, 1));
    }
} // namespace AtomSampleViewer
