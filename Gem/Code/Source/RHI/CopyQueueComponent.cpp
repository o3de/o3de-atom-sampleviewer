/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/CopyQueueComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void CopyQueueComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CopyQueueComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    CopyQueueComponent::CopyQueueComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void CopyQueueComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        m_processingState.m_time += ProcessingState::TickAmount;
        m_processingState.m_timeUntilChange -= ProcessingState::TickAmount;

        if (m_processingState.m_timeUntilChange < 0.0f)
        {
            m_shaderResourceGroup->Compile();

            int nextTexture = m_processingState.m_changeCount % ProcessingState::TextureCount;
            if (!m_shaderResourceGroup->SetImage(m_textureInputIndex, m_images[nextTexture]))
            {
                AZ_Error("CopyQueueExample", false, "Failed to set image into shader resource group");
                return;
            };

            UpdateVertexPositions(m_processingState.m_time);
            UploadVertexBuffer();

            m_processingState.m_timeUntilChange = ProcessingState::ChangeDelay;
            m_processingState.m_changeCount++;
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void CopyQueueComponent::Activate()
    {
        using namespace AZ;
        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        m_bufferData = BufferData();

        const auto positionBufSize = static_cast<uint32_t>(m_bufferData.m_positions.size() * sizeof(VertexPosition));
        const auto indexBufSize = static_cast<uint32_t>(m_bufferData.m_indices.size() * sizeof(uint16_t));
        const auto uvBufSize = static_cast<uint32_t>(m_bufferData.m_uvs.size() * sizeof(VertexUV));

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        {
            m_bufferPool = aznew RHI::MultiDeviceBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            UpdateVertexPositions(0);

            m_bufferData.m_indices = { { 0, 3, 1, 1, 3, 2 } };

            SetVertexUV(m_bufferData.m_uvs.data(), 0, 0.0f, 0.0f);
            SetVertexUV(m_bufferData.m_uvs.data(), 1, 0.0f, 1.0f);
            SetVertexUV(m_bufferData.m_uvs.data(), 2, 1.0f, 1.0f);
            SetVertexUV(m_bufferData.m_uvs.data(), 3, 1.0f, 0.0f);

            m_positionBuffer = aznew RHI::MultiDeviceBuffer();
            m_indexBuffer = aznew RHI::MultiDeviceBuffer();
            m_uvBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::MultiDeviceBufferInitRequest request;
            
            request.m_buffer = m_indexBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, indexBufSize };
            request.m_initialData = m_bufferData.m_indices.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("CopyQueueExample", false, "Failed to initialize index buffer with error code %d", result);
                return;
            }

            // We specifically make the position buffer *not* the first one, to test a specific failure that
            // can occur in AsyncUploadQueue: when we call BufferPool::StreamBuffer to update the position
            // buffer, AsyncUploadQueue::QueueUpload needs to account for paged allocation offsets.

            request.m_buffer = m_positionBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, positionBufSize };
            request.m_initialData = m_bufferData.m_positions.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("CopyQueueExample", false, "Failed to initialize position buffer with error code %d", result);
                return;
            }

            request.m_buffer = m_uvBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, uvBufSize };
            request.m_initialData = m_bufferData.m_uvs.data();
            result = m_bufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error("CopyQueueExample", false, "Failed to initialize uv buffer with error code %d", result);
                return;
            }

            m_streamBufferViews[0] = {
                *m_positionBuffer,
                0,
                positionBufSize,
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] = {
                *m_uvBuffer,
                0,
                uvBufSize,
                sizeof(VertexUV)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        {
            const char* copyQueueShaderFilePath = "Shaders/RHI/CopyQueue.azshader";
            const char* sampleName = "CopyQueueExample";
            
            auto shader = LoadShader(copyQueueShaderFilePath, sampleName);
            if (shader == nullptr)
                return;
            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            m_pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!m_pipelineState)
            {
                AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", copyQueueShaderFilePath);
                return;
            }

            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "CopyQueueSrg", sampleName);

            FindShaderInputIndex(&m_textureInputIndex, m_shaderResourceGroup, AZ::Name{"m_texture"}, sampleName);

            for (int index = 0; index < numberOfPaths; index++)
            {
                UploadTextureAsAsset(m_filePaths[index], index);
            }

            if (!m_shaderResourceGroup->SetImage(m_textureInputIndex, m_images[0]))
            {
                AZ_Error(sampleName, false, "Failed to set image into shader resource group");
                return;
            }
        }

        {
            struct ScopeData
            {
                //UserDataParam - Empty for this samples
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] const ScopeData& scopeData)
            {
                {
                    RHI::ImageScopeAttachmentDescriptor desc;
                    desc.m_attachmentId = m_outputAttachmentId;
                    frameGraph.UseColorAttachment(desc);
                }

                frameGraph.SetEstimatedItemCount(1);
            };

            RHI::EmptyCompileFunction<ScopeData> compileFunction;

            const auto executeFunction = [=](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                
                RHI::CommandList* commandList = context.GetCommandList();

                commandList->SetViewports(&m_viewport, 1);
                commandList->SetScissors(&m_scissor, 1);

                const RHI::SingleDeviceIndexBufferView indexBufferView =
                {
                    *m_indexBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                    0,
                    indexBufSize,
                    RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"CopyQueue"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        m_processingState = ProcessingState{};

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void CopyQueueComponent::Deactivate()
    {
        m_positionBuffer = nullptr;
        m_indexBuffer = nullptr;
        m_uvBuffer = nullptr;
        m_bufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;
        
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void CopyQueueComponent::UploadTextureAsAsset(const char* filePath, int index)
    {
        using namespace AZ;

        // Load a texture asset from the cache
        Data::AssetId streamingImageAssetId;
        Data::AssetCatalogRequestBus::BroadcastResult(
            streamingImageAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            filePath, azrtti_typeid<RPI::StreamingImageAsset>(), false);
        if (!streamingImageAssetId.IsValid())
        {
            AZ_Error("CopyQueueExample", false, "Failed to get shader asset id with path %s", filePath);
            return;
        }

        auto streamingImageAsset = Data::AssetManager::Instance().GetAsset<RPI::StreamingImageAsset>(
            streamingImageAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
        streamingImageAsset.BlockUntilLoadComplete();
        if (!streamingImageAsset.IsReady())
        {
            AZ_Error("CopyQueueExample", false, "Failed to get asset '%s'", filePath);
            return;
        }

        auto image = RPI::StreamingImage::FindOrCreate(streamingImageAsset);
        if (!image)
        {
            AZ_Error("CopyQueueExample", false, "Failed to find or create an image instance from from image asset '%s'", filePath);
            return;
        }
        m_images[index] = image;
    }

    void CopyQueueComponent::UpdateVertexPositions(float timeValue)
    {
        float scale = 1.0f + sin(timeValue) * 0.1f;

        SetVertexPosition(m_bufferData.m_positions.data(), 0, scale * AZ::Vector3(-0.5f, -0.5f, 0.0f));
        SetVertexPosition(m_bufferData.m_positions.data(), 1, scale * AZ::Vector3(-0.5f, 0.5f, 0.0f));
        SetVertexPosition(m_bufferData.m_positions.data(), 2, scale * AZ::Vector3(0.5f, 0.5f, 0.0f));
        SetVertexPosition(m_bufferData.m_positions.data(), 3, scale * AZ::Vector3(0.5f, -0.5f, 0.0f));
    }

    void CopyQueueComponent::UploadVertexBuffer()
    {
        AZ::RHI::MultiDeviceBufferStreamRequest request;
        request.m_buffer = m_positionBuffer.get();
        request.m_byteCount = static_cast<uint32_t>(m_bufferData.m_positions.size() * sizeof(VertexPosition));
        request.m_sourceData = m_bufferData.m_positions.data();

        AZ::RHI::ResultCode resultCode = m_bufferPool->StreamBuffer(request);
        if (resultCode != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("CopyQueueExample", false, "UploadVertexBuffer() failed to stream buffer contents to GPU.");
        }
    }

} // namespace AtomSampleViewer
