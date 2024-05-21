/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/CommandList.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/createdestroy.h>

#include <RHI/MSAAExampleComponent.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    namespace
    {
        const char* TriangeShaderFilePath = "Shaders/RHI/triangle.azshader";
        const char* CustomResolveShaderFilePath = "Shaders/RHI/MSAAResolve.azshader";
        const char* SampleName = "MSAAExample";
    }

    void MSAAExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MSAAExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MSAAExampleComponent::MSAAExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void MSAAExampleComponent::Activate()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew RHI::BufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        AZStd::vector<RHI::SamplePosition> emptySamplePositions;
        AZStd::vector<RHI::SamplePosition> customSamplePositions = { RHI::SamplePosition(3, 3), RHI::SamplePosition(11, 3), RHI::SamplePosition(3, 11), RHI::SamplePosition(11, 11) };
        m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA2X)] = { 2, true, emptySamplePositions, RHI::AttachmentStoreAction::DontCare, RHI::AttachmentId("MSAA2X"), true };
        m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA4X)] = { 4, true, emptySamplePositions, RHI::AttachmentStoreAction::DontCare, RHI::AttachmentId("MSAA4X"), true };
        m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA4X_Custom_Positions)] = { 4, true, customSamplePositions, RHI::AttachmentStoreAction::DontCare, RHI::AttachmentId("MSAA4X_Custom_Positions"), true };
        m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA4X_Custom_Resolve)] = { 4, false, emptySamplePositions, RHI::AttachmentStoreAction::Store, RHI::AttachmentId("MSAA4X_Custom_Resolve"), true };
        m_sampleProperties[static_cast<uint32_t>(MSAAType::NoAA)] = { 1, false, emptySamplePositions, RHI::AttachmentStoreAction::Store, m_outputAttachmentId, false };

        CreateTriangleResources();
        CreateQuadResources();

        for (uint32_t i = 0; i < s_numMSAAExamples; ++i)
        {
            auto msaaType = static_cast<MSAAType>(i);
            CreateScopeResources(msaaType);
            CreateScopeProducer(msaaType);
        }
         
        // Extra scopes for resolving the MSAA texture using a custom shader
        CreateCustomMSAAResolveResources();
        CreateCustomMSAAResolveScope();

        OnMSAATypeChanged(m_currentType);

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();
    }

    void MSAAExampleComponent::Deactivate()
    {
        m_triangleInputAssemblyBuffer = nullptr;
        m_quadInputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_pipelineStates.fill(nullptr);
        m_customResolveMSAAPipelineState = nullptr;
        m_triangleShaderResourceGroup = nullptr;
        m_customMSAAResolveShaderResourceGroup = nullptr;
        
        AzFramework::InputChannelEventListener::Disconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        AZStd::for_each(m_MSAAScopeProducers.begin(), m_MSAAScopeProducers.end(), [](auto& list) { list.clear(); });
        m_scopeProducers.clear();
    }

    bool MSAAExampleComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel & inputChannel)
    {
        const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
        switch (inputChannel.GetState())
        {
        case AzFramework::InputChannel::State::Ended:
        {
            if (inputChannelId == AzFramework::InputDeviceMouse::Button::Left)
            {
                OnMSAATypeChanged(static_cast<MSAAType>((static_cast<uint32_t>(m_currentType) + 1) % s_numMSAAExamples));
            }
        }
        default:
        {
            break;
        }
        }
        return false;
    }

    void MSAAExampleComponent::CreateTriangleResources()
    {
        using namespace AZ;
        TriangleBufferData bufferData;

        SetVertexPosition(bufferData.m_positions.data(), 0, 0.0, 0.5, 0.0);
        SetVertexPosition(bufferData.m_positions.data(), 1, -0.5, -0.5, 0.0);
        SetVertexPosition(bufferData.m_positions.data(), 2, 0.5, -0.5, 0.0);

        SetVertexColor(bufferData.m_colors.data(), 0, 1.0, 0.0, 0.0, 1.0);
        SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 1.0, 0.0, 1.0);
        SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);

        SetVertexIndexIncreasing(bufferData.m_indices.data(), bufferData.m_indices.size());

        m_triangleInputAssemblyBuffer = aznew RHI::Buffer();

        RHI::BufferInitRequest request;
        request.m_buffer = m_triangleInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        m_inputAssemblyBufferPool->InitBuffer(request);

        m_triangleStreamBufferViews[0] =
        {
            *m_triangleInputAssemblyBuffer,
            offsetof(TriangleBufferData, m_positions),
            sizeof(TriangleBufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_triangleStreamBufferViews[1] =
        {
            *m_triangleInputAssemblyBuffer,
            offsetof(TriangleBufferData, m_colors),
            sizeof(TriangleBufferData::m_colors),
            sizeof(VertexColor)
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
        m_triangleInputStreamLayout = layoutBuilder.End();

        RHI::ValidateStreamBufferViews(m_triangleInputStreamLayout, m_triangleStreamBufferViews);

        m_triangleShader = LoadShader(TriangeShaderFilePath, SampleName);
        if (!m_triangleShader)
        {
            return;
        }

        m_triangleShaderResourceGroup = CreateShaderResourceGroup(m_triangleShader, "TriangleInstanceSrg", SampleName);

        const Name objectMatrixConstantId{ "m_objectMatrix" };
        FindShaderInputIndex(&m_objectMatrixConstantIndex, m_triangleShaderResourceGroup, objectMatrixConstantId, SampleName);

        [[maybe_unused]] bool success = m_triangleShaderResourceGroup->SetConstant(m_objectMatrixConstantIndex, AZ::Matrix4x4::CreateIdentity());
        AZ_Warning("MSAAExampleComponent", success, "Failed to set SRG Constant m_objectMatrix");
        m_triangleShaderResourceGroup->Compile();
    }

    void MSAAExampleComponent::CreateQuadResources()
    {
        using namespace AZ;
        QuadBufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_quadInputAssemblyBuffer = aznew RHI::Buffer();

        RHI::ResultCode result = RHI::ResultCode::Success;
        RHI::BufferInitRequest request;

        request.m_buffer = m_quadInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        result = m_inputAssemblyBufferPool->InitBuffer(request);

        if (result != RHI::ResultCode::Success)
        {
            AZ_Error(SampleName, false, "Failed to initialize position buffer with error code %d", result);
            return;
        }

        m_quadStreamBufferViews[0] = {
            *m_quadInputAssemblyBuffer,
            offsetof(QuadBufferData, m_positions),
            sizeof(QuadBufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_quadStreamBufferViews[1] = {
            *m_quadInputAssemblyBuffer,
            offsetof(QuadBufferData, m_uvs),
            sizeof(QuadBufferData::m_uvs),
            sizeof(VertexUV)
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_quadInputStreamLayout = layoutBuilder.End();

        RHI::ValidateStreamBufferViews(m_quadInputStreamLayout, m_quadStreamBufferViews);

        m_customMSAAResolveShader = LoadShader(CustomResolveShaderFilePath, SampleName);
        if (!m_customMSAAResolveShader)
        {
            return;
        }

        m_customMSAAResolveShaderResourceGroup = CreateShaderResourceGroup(m_customMSAAResolveShader, "TextureInstanceSrg", SampleName);
        const Name albedoMapId{ "m_albedoMap" };
        FindShaderInputIndex(&m_customMSAAResolveTextureInputIndex, m_customMSAAResolveShaderResourceGroup, albedoMapId, SampleName);
    }

    void MSAAExampleComponent::OnMSAATypeChanged(MSAAType type)
    {
        auto msaaTypeIndex = static_cast<uint32_t>(type);
        m_scopeProducers.clear();
        m_scopeProducers.insert(m_scopeProducers.end(), m_MSAAScopeProducers[msaaTypeIndex].begin(), m_MSAAScopeProducers[msaaTypeIndex].end());
        m_currentType = type;
    }

    void MSAAExampleComponent::CreateScopeResources(MSAAType type)
    {
        using namespace AZ;

        const auto& properties = m_sampleProperties[static_cast<uint32_t>(type)];

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
        pipelineStateDescriptor.m_inputStreamLayout = m_triangleInputStreamLayout;

        auto shaderVariant = m_triangleShader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
        shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat, properties.m_resolve);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        pipelineStateDescriptor.m_renderStates.m_multisampleState.m_samples = static_cast<uint16_t>(properties.m_samplesCount);
        auto& customPositionList = pipelineStateDescriptor.m_renderStates.m_multisampleState.m_customPositions;
        AZStd::copy(properties.m_customPositions.begin(), properties.m_customPositions.end(), customPositionList.begin());
        pipelineStateDescriptor.m_renderStates.m_multisampleState.m_customPositionsCount = static_cast<uint32_t>(properties.m_customPositions.size());

        auto& pipeline = m_pipelineStates[static_cast<uint32_t>(type)];
        pipeline = m_triangleShader->AcquirePipelineState(pipelineStateDescriptor);
        if (!pipeline)
        {
            AZ_Error(SampleName, false, "Failed to acquire default pipeline state for shader '%s'", TriangeShaderFilePath);
            return;
        }
    }

    void MSAAExampleComponent::CreateScopeProducer(MSAAType type)
    {
        using namespace AZ;
        uint32_t msaaTypeIndex = static_cast<uint32_t>(type);

        // Creates a scope for rendering the triangle.
        const auto prepareFunction = [this, msaaTypeIndex](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {            
            {
                // Binds the MSAA color attachment.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_sampleProperties[msaaTypeIndex].m_attachmentId;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(1.0f, 1.0, 1.0, 0.0);
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_storeAction = m_sampleProperties[msaaTypeIndex].m_storeAction;

                if (m_sampleProperties[msaaTypeIndex].m_isTransient)
                {
                    auto imageDesc = RHI::ImageDescriptor::Create2D(
                        RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                        m_outputWidth,
                        m_outputHeight,
                        m_outputFormat);
                    imageDesc.m_multisampleState = RHI::MultisampleState(static_cast<uint16_t>(m_sampleProperties[msaaTypeIndex].m_samplesCount), 0);
                    if(m_sampleProperties[msaaTypeIndex].m_customPositions.size()>0)
                    {
                        imageDesc.m_multisampleState.m_customPositionsCount = static_cast<uint32_t>(m_sampleProperties[msaaTypeIndex].m_customPositions.size());
                        auto& customPositionList = imageDesc.m_multisampleState.m_customPositions;
                        AZStd::copy(m_sampleProperties[msaaTypeIndex].m_customPositions.begin(), m_sampleProperties[msaaTypeIndex].m_customPositions.end(), customPositionList.begin());
                    }

                    frameGraph.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(descriptor.m_attachmentId, imageDesc));
                }

                frameGraph.UseColorAttachment(descriptor);
            }

            // Binds the resolve color attachment.
            if (m_sampleProperties[msaaTypeIndex].m_resolve)
            {
                RHI::ResolveScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                descriptor.m_resolveAttachmentId = m_sampleProperties[msaaTypeIndex].m_attachmentId;
                frameGraph.UseResolveAttachment(descriptor);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        using namespace AZ;
        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this, msaaTypeIndex](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::DeviceIndexBufferView indexBufferView = { *m_triangleInputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                                                                 offsetof(TriangleBufferData, m_indices),
                                                                 sizeof(TriangleBufferData::m_indices), RHI::IndexFormat::Uint16 };

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 3;
            drawIndexed.m_instanceCount = 1;

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                m_triangleShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
            };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_pipelineStates[msaaTypeIndex]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            drawItem.m_indexBufferView = &indexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_triangleStreamBufferViews.size());
            AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                m_triangleStreamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                m_triangleStreamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
            };
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            // Submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        auto name = AZStd::string::format("MSAAType_%d", msaaTypeIndex);
        const AZ::RHI::ScopeId TriangleScope(name);

        m_MSAAScopeProducers[msaaTypeIndex].emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                TriangleScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void MSAAExampleComponent::CreateCustomMSAAResolveResources()
    {
        using namespace AZ;
        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
        pipelineStateDescriptor.m_inputStreamLayout = m_quadInputStreamLayout;

        if (!m_customMSAAResolveShader)
        {
            return;
        }
        auto shaderVariant = m_customMSAAResolveShader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);
        shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_customResolveMSAAPipelineState = m_customMSAAResolveShader->AcquirePipelineState(pipelineStateDescriptor);
        if (!m_customResolveMSAAPipelineState)
        {
            AZ_Error(SampleName, false, "Failed to acquire default pipeline state for shader '%s'", CustomResolveShaderFilePath);
            return;
        }
    }

    void MSAAExampleComponent::CreateCustomMSAAResolveScope()
    {
        using namespace AZ;
        const RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] const ScopeData& scopeData)
        {         
            // Bind MSAA as a texture attachment to sample
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA4X_Custom_Resolve)].m_attachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseShaderAttachment(descriptor, RHI::ScopeAttachmentAccess::Read);
            }

            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(1.0f, 1.0, 1.0, 0.0);
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseColorAttachment(descriptor);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this](const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageView = context.GetImageView(m_sampleProperties[static_cast<uint32_t>(MSAAType::MSAA4X_Custom_Resolve)].m_attachmentId);
            m_customMSAAResolveShaderResourceGroup->SetImageView(m_customMSAAResolveTextureInputIndex, imageView);
            m_customMSAAResolveShaderResourceGroup->Compile();
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::DeviceIndexBufferView indexBufferView = { *m_quadInputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                                                                 offsetof(QuadBufferData, m_indices), sizeof(QuadBufferData::m_indices),
                                                                 RHI::IndexFormat::Uint16 };

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_customMSAAResolveShaderResourceGroup
                                                                                 ->GetRHIShaderResourceGroup()
                                                                                 ->GetDeviceShaderResourceGroup(context.GetDeviceIndex())
                                                                                 .get() };

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_customResolveMSAAPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            drawItem.m_indexBufferView = &indexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_quadStreamBufferViews.size());
            AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                m_quadStreamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                m_quadStreamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
            };
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            // Submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_MSAAScopeProducers[static_cast<uint32_t>(MSAAType::MSAA4X_Custom_Resolve)].emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId("Resolve"),
                ScopeData{  },
                prepareFunction,
                compileFunction,
                executeFunction));
    }
} // namespace AtomSampleViewer
