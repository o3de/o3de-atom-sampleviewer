/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <RHI/DualSourceBlendingComponent.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    const char* DualSourceBlendingComponent::s_dualSourceBlendingName = "DualSourceBlending";

    void DualSourceBlendingComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DualSourceBlendingComponent, AZ::Component>()->Version(0);
        }
    }

    DualSourceBlendingComponent::DualSourceBlendingComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void DualSourceBlendingComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        float blendFactor = (sinf(m_time) + 1) * 0.5f;
        m_shaderResourceGroup->SetConstant(m_blendFactorIndex, blendFactor);
        m_shaderResourceGroup->Compile();
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void DualSourceBlendingComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        m_time += deltaTime;
    }

    void DualSourceBlendingComponent::Activate()
    {
        CreateInputAssemblyBuffersAndViews();
        LoadRasterShader();
        CreateRasterScope();

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void DualSourceBlendingComponent::Deactivate()
    {
        m_inputAssemblyBufferPool = nullptr;
        m_inputAssemblyBuffer = nullptr;

        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;

        m_scopeProducers.clear();
        m_windowContext = nullptr;
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void DualSourceBlendingComponent::CreateInputAssemblyBuffersAndViews()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        m_inputAssemblyBufferPool = aznew RHI::BufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        BufferData bufferData;
        SetVertexPosition(bufferData.m_positions.data(), 0, -1.0f, -1.0f, 0.0f);
        SetVertexPosition(bufferData.m_positions.data(), 1,  1.0f, -1.0f, 0.0f);
        SetVertexPosition(bufferData.m_positions.data(), 2,  0.0f, 1.0f, 0.0f);

        SetVertexColor(bufferData.m_colors.data(), 0, 1.0, 0.0, 0.0, 1.0);
        SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 1.0, 0.0, 1.0);
        SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);

        SetVertexIndexIncreasing(bufferData.m_indices.data(), bufferData.m_indices.size());
        m_inputAssemblyBuffer = aznew RHI::Buffer();

        RHI::BufferInitRequest request;
        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        m_inputAssemblyBufferPool->InitBuffer(request);

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
            offsetof(BufferData, m_colors),
            sizeof(BufferData::m_colors),
            sizeof(VertexColor)
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
        layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
        m_inputStreamLayout = layoutBuilder.End();

        RHI::ValidateStreamBufferViews(m_inputStreamLayout, m_streamBufferViews);
    }

    void DualSourceBlendingComponent::LoadRasterShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/dualsourceblending.azshader";

        auto shader = LoadShader(shaderFilePath, s_dualSourceBlendingName);
        if (shader == nullptr)
        {
            return;
        }

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        pipelineDesc.m_renderStates.m_blendState.m_targets[0].m_enable = 1;
        pipelineDesc.m_renderStates.m_blendState.m_targets[0].m_blendSource = RHI::BlendFactor::ColorSource1;
        pipelineDesc.m_renderStates.m_blendState.m_targets[0].m_blendDest = RHI::BlendFactor::ColorSource1Inverse;
        pipelineDesc.m_renderStates.m_blendState.m_targets[0].m_blendOp = RHI::BlendOp::Add;

        m_pipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineState)
        {
            AZ_Error(s_dualSourceBlendingName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_shaderResourceGroup = CreateShaderResourceGroup(shader, "DualSourceBlendingSrg", s_dualSourceBlendingName);
        FindShaderInputIndex(&m_blendFactorIndex, m_shaderResourceGroup, AZ::Name{"m_blendFactor"}, s_dualSourceBlendingName);
    }

    void DualSourceBlendingComponent::CreateRasterScope()
    {
        using namespace AZ;
        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] const ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 3;
            drawIndexed.m_instanceCount = 1;

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
            };
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;

            // Submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId(s_dualSourceBlendingName),
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
}
