/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/ScopeProducerFunction.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <Atom/RPI.Reflect/Image/AttachmentImageAssetCreator.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/MRTExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    MRTExampleComponent::MRTExampleComponent()
    {
        m_time = 0.0f;
        m_attachmentID[0] = AZ::RHI::AttachmentId("redAttachemntID");
        m_attachmentID[1] = AZ::RHI::AttachmentId("greenAttachemntID");
        m_attachmentID[2] = AZ::RHI::AttachmentId("blueAttachemntID");
        m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

        m_supportRHISamplePipeline = true;
    }

    void MRTExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MRTExampleComponent, AZ::Component>()
                ->Version(0);
        }
    }

    void MRTExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        m_time += 0.002f;
        float r = cosf(m_time);
        float g = sinf(m_time);
        float b = cosf(m_time + AZ::Constants::Pi);
        m_shaderResourceGroups[0]->SetConstant(m_shaderInputConstantIndices[0], AZ::Vector4(r, 0 ,0, 1));
        m_shaderResourceGroups[0]->SetConstant(m_shaderInputConstantIndices[1], AZ::Vector2(g, 0));
        m_shaderResourceGroups[0]->SetConstant(m_shaderInputConstantIndices[2], b);
        m_shaderResourceGroups[0]->Compile();

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void MRTExampleComponent::Activate()
    {
        // Init buffers & views
        CreateInputAssemblyBuffersAndViews();
        // Init render targets
        InitRenderTargets();
        // init pipeline state
        ReadRenderTargetShader();
        ReadScreenShader();

        CreateRenderTargetScope();
        CreateScreenScope();

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MRTExampleComponent::Deactivate()
    {
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_bufferPool = nullptr;
        m_inputAssemblyBuffer = nullptr;
        m_renderTargetImageDescriptors.fill({});
        m_shaderResourceGroups.fill(nullptr);
        m_pipelineStates.fill(nullptr);
        m_scopeProducers.clear();
    }

    void MRTExampleComponent::ReadRenderTargetShader()
    {
        using namespace AZ;
        const char* MRTTargetShaderFilePath = "Shaders/RHI/MRTTarget.azshader";
        const char* sampleName = "MRTExample";

        auto shader = LoadShader(MRTTargetShaderFilePath, sampleName);
        if (shader == nullptr)
            return;

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(RHI::Format::R8G8B8A8_UNORM_SRGB)
            ->RenderTargetAttachment(RHI::Format::R16G16_FLOAT)
            ->RenderTargetAttachment(RHI::Format::R32_FLOAT);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineStates[0] = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineStates[0])
        {
            AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", MRTTargetShaderFilePath);
            return;
        }

        m_shaderResourceGroups[0] = CreateShaderResourceGroup(shader, "MrtTargetSrg", sampleName);

        FindShaderInputIndex(&m_shaderInputConstantIndices[0], m_shaderResourceGroups[0], AZ::Name{"rValue"}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[1], m_shaderResourceGroups[0], AZ::Name{"gValue"}, sampleName);
        FindShaderInputIndex(&m_shaderInputConstantIndices[2], m_shaderResourceGroups[0], AZ::Name{"bValue"}, sampleName);
    }

    void MRTExampleComponent::ReadScreenShader()
    {
        using namespace AZ;
        const char* MRTScreenShaderFilePath = "Shaders/RHI/MRTScreen.azshader";
        const char* sampleName = "MRTExample";

        auto shader = LoadShader(MRTScreenShaderFilePath, sampleName);
        if (shader == nullptr)
            return;

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineStates[1] = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineStates[1])
        {
            AZ_Error(sampleName, false, "Failed to acquire default pipeline state for shader '%s'", MRTScreenShaderFilePath);
            return;
        }

        m_shaderResourceGroups[1] = CreateShaderResourceGroup(shader, "MrtScreenInstanceSrg", sampleName);

        FindShaderInputIndex(&m_shaderInputImageIndices[0], m_shaderResourceGroups[1], AZ::Name{"rMap"}, sampleName);
        FindShaderInputIndex(&m_shaderInputImageIndices[1], m_shaderResourceGroups[1], AZ::Name{"gMap"}, sampleName);
        FindShaderInputIndex(&m_shaderInputImageIndices[2], m_shaderResourceGroups[1], AZ::Name{"bMap"}, sampleName);
    }

    void MRTExampleComponent::CreateInputAssemblyBuffersAndViews()
    {
        using namespace AZ;
        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        m_bufferPool = aznew RHI::BufferPool();
        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        BufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_inputAssemblyBuffer = aznew RHI::Buffer();
        RHI::ResultCode result = RHI::ResultCode::Success;
        RHI::BufferInitRequest request;

        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        result = m_bufferPool->InitBuffer(request);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Error("MRTExample", false, "Failed to initialize buffer with error code %d", result);
            return;
        }

        m_streamBufferViews[0] =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_positions),
            sizeof(BufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_streamBufferViews[1] =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_uvs),
            sizeof(BufferData::m_uvs),
            sizeof(VertexUV)
        };

        m_indexBufferView =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_indices),
            sizeof(BufferData::m_indices),
            RHI::IndexFormat::Uint16
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_inputStreamLayout = layoutBuilder.End();

        RHI::ValidateStreamBufferViews(m_inputStreamLayout, m_streamBufferViews);
    }

    void MRTExampleComponent::InitRenderTargets()
    {
        using namespace AZ;

        RHI::ImageDescriptor imageDesc;

        imageDesc = RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite, 8, 8, RHI::Format::R8G8B8A8_UNORM_SRGB);
        m_renderTargetImageDescriptors[0] = RHI::TransientImageDescriptor(m_attachmentID[0], imageDesc);

        imageDesc = RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite, 8, 8, RHI::Format::R16G16_FLOAT);
        m_renderTargetImageDescriptors[1] = RHI::TransientImageDescriptor(m_attachmentID[1], imageDesc);

        imageDesc = RHI::ImageDescriptor::Create2D(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite, 8, 8, RHI::Format::R32_FLOAT);
        m_renderTargetImageDescriptors[2] = RHI::TransientImageDescriptor(m_attachmentID[2], imageDesc);
    }

    void MRTExampleComponent::CreateRenderTargetScope()
    {
        using namespace AZ;

        const auto prepareFunctionRenderTarget = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Create & Binds RenderTarget images
            {
                frameGraph.GetAttachmentDatabase().CreateTransientImage(m_renderTargetImageDescriptors[0]);
                RHI::ImageScopeAttachmentDescriptor descR;
                descR.m_attachmentId = m_attachmentID[0];
                descR.m_loadStoreAction.m_clearValue = m_clearValue;
                descR.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;

                frameGraph.GetAttachmentDatabase().CreateTransientImage(m_renderTargetImageDescriptors[1]);
                RHI::ImageScopeAttachmentDescriptor descG;
                descG.m_attachmentId = m_attachmentID[1];
                descG.m_loadStoreAction.m_clearValue = m_clearValue;
                descG.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;

                frameGraph.GetAttachmentDatabase().CreateTransientImage(m_renderTargetImageDescriptors[2]);
                RHI::ImageScopeAttachmentDescriptor descB;
                descB.m_attachmentId = m_attachmentID[2];
                descB.m_loadStoreAction.m_clearValue = m_clearValue;
                descB.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;

                const AZStd::array<RHI::ImageScopeAttachmentDescriptor, 3> imageScopeArray{{descR, descG, descB}};
                frameGraph.UseColorAttachments(imageScopeArray);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunctionRenderTarget;
        const auto executeFunctionRenderTarget = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            const auto& imageSize = m_renderTargetImageDescriptors[0].m_imageDescriptor.m_size;
            RHI::Viewport viewport(0, static_cast<float>(imageSize.m_width), 0, static_cast<float>(imageSize.m_height));
            RHI::Scissor scissor(0, 0, static_cast<int32_t>(imageSize.m_width), static_cast<int32_t>(imageSize.m_height));
            commandList->SetViewports(&viewport, 1);
            commandList->SetScissors(&scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_shaderResourceGroups[0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineStates[0]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
            };
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunctionRenderTarget),
            decltype(compileFunctionRenderTarget),
            decltype(executeFunctionRenderTarget)>(
                RHI::ScopeId{"MRTTarget"},
                ScopeData{},
                prepareFunctionRenderTarget,
                compileFunctionRenderTarget,
                executeFunctionRenderTarget));
    }

    void MRTExampleComponent::CreateScreenScope()
    {
        using namespace AZ;

        const auto prepareFunctionScreen = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Bind shader attachments which are rendered by RenderTargetScope.
            {
                frameGraph.UseShaderAttachment(RHI::ImageScopeAttachmentDescriptor(m_attachmentID[0]), RHI::ScopeAttachmentAccess::Read);
                frameGraph.UseShaderAttachment(RHI::ImageScopeAttachmentDescriptor(m_attachmentID[1]), RHI::ScopeAttachmentAccess::Read);
                frameGraph.UseShaderAttachment(RHI::ImageScopeAttachmentDescriptor(m_attachmentID[2]), RHI::ScopeAttachmentAccess::Read);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunctionScreen = [this](const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageViewR = context.GetImageView(m_attachmentID[0]);
            const auto* imageViewG = context.GetImageView(m_attachmentID[1]);
            const auto* imageViewB = context.GetImageView(m_attachmentID[2]);

            m_shaderResourceGroups[1]->SetImageView(m_shaderInputImageIndices[0], imageViewR);
            m_shaderResourceGroups[1]->SetImageView(m_shaderInputImageIndices[1], imageViewG);
            m_shaderResourceGroups[1]->SetImageView(m_shaderInputImageIndices[2], imageViewB);
            m_shaderResourceGroups[1]->Compile();
        };

        const auto executeFunctionScreen = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_shaderResourceGroups[1]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineStates[1]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
            };
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunctionScreen),
            decltype(compileFunctionScreen),
            decltype(executeFunctionScreen)>(
                RHI::ScopeId{"MRTScreen"},
                ScopeData{},
                prepareFunctionScreen,
                compileFunctionScreen,
                executeFunctionScreen));
    }
}
