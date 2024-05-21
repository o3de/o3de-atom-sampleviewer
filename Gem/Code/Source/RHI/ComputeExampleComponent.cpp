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

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/ComputeExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    const char* ComputeExampleComponent::s_computeExampleName = "ComputeExample";

    namespace ShaderInputs
    {
        static const char* const ShaderInputDimension{ "dimension" };
        static const char* const ShaderInputSeed{ "seed" };
    }

    void ComputeExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ComputeExampleComponent, AZ::Component>()->Version(0);
        }
    }

    ComputeExampleComponent::ComputeExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void ComputeExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        m_dispatchSRGs[0]->SetConstant(m_dispatchSeedConstantIndex, AZ::Vector2(cosf(m_time), sin(m_time)));
        m_dispatchSRGs[0]->Compile();
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void ComputeExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        m_time += deltaTime;
    }

    void ComputeExampleComponent::Activate()
    {
        CreateInputAssemblyBuffersAndViews();
        CreateComputeBuffer();

        LoadComputeShader();
        LoadRasterShader();

        CreateComputeScope();
        CreateRasterScope();

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void ComputeExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_dispatchPipelineState = nullptr;
        m_drawPipelineState = nullptr;
        m_dispatchSRGs.fill(nullptr);
        m_drawSRGs.fill(nullptr);

        m_computeBufferPool = nullptr;
        m_computeBuffer = nullptr;
        m_computeBufferView = nullptr;

        m_scopeProducers.clear();
        m_windowContext = nullptr;

        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
    }

    void ComputeExampleComponent::CreateInputAssemblyBuffersAndViews()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        BufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_inputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();

        RHI::MultiDeviceBufferInitRequest request;
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

    void ComputeExampleComponent::LoadComputeShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/ComputeDispatch.azshader";

        const auto shader = LoadShader(shaderFilePath, s_computeExampleName);
        if (shader == nullptr)
            return;

        RHI::PipelineStateDescriptorForDispatch pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

        const auto& numThreads = shader->GetAsset()->GetAttribute(RHI::ShaderStage::Compute, Name("numthreads"));
        if (numThreads)
        {
            const RHI::ShaderStageAttributeArguments& args = *numThreads;
            m_numThreadsX = args[0].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[0]) : m_numThreadsX;
            m_numThreadsY = args[1].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[1]) : m_numThreadsY;
            m_numThreadsZ = args[2].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[2]) : m_numThreadsZ;
        }
        else
        {
            AZ_Error(s_computeExampleName, false, "Did not find expected numthreads attribute");
        }

        m_dispatchPipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_dispatchPipelineState)
        {
            AZ_Error(s_computeExampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_dispatchSRGs[0] = CreateShaderResourceGroup(shader, "ConstantSrg", s_computeExampleName);
        m_dispatchSRGs[1] = CreateShaderResourceGroup(shader, "BufferSrg", s_computeExampleName);

        FindShaderInputIndex(&m_dispatchDimensionConstantIndex, m_dispatchSRGs[0], AZ::Name{ShaderInputs::ShaderInputDimension}, s_computeExampleName);
        FindShaderInputIndex(&m_dispatchSeedConstantIndex, m_dispatchSRGs[0], AZ::Name{ShaderInputs::ShaderInputSeed}, s_computeExampleName);

        // This SRG will be compiled during the OnFramePrepare
        m_dispatchSRGs[0]->SetConstant(m_dispatchDimensionConstantIndex, AZ::Vector2(static_cast<float>(m_bufferWidth), static_cast<float>(m_bufferHeight)));
    }

    void ComputeExampleComponent::LoadRasterShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/ComputeDraw.azshader";

        auto shader = LoadShader(shaderFilePath, s_computeExampleName);
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

        m_drawPipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_drawPipelineState)
        {
            AZ_Error(s_computeExampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_drawSRGs[0] = CreateShaderResourceGroup(shader, "ConstantSrg", s_computeExampleName);
        m_drawSRGs[1] = CreateShaderResourceGroup(shader, "BufferSrg", s_computeExampleName);

        FindShaderInputIndex(&m_drawDimensionConstantIndex, m_drawSRGs[0], AZ::Name{ShaderInputs::ShaderInputDimension}, s_computeExampleName);

        m_drawSRGs[0]->SetConstant(m_drawDimensionConstantIndex, AZ::Vector2(static_cast<float>(m_bufferWidth), static_cast<float>(m_bufferHeight)));
        m_drawSRGs[0]->Compile();
    }

    void ComputeExampleComponent::CreateComputeBuffer()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        [[maybe_unused]] RHI::ResultCode result = RHI::ResultCode::Success;
        m_computeBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        bufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;

        result = m_computeBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized compute buffer pool");

        m_computeBuffer = aznew RHI::MultiDeviceBuffer();
        uint32_t bufferSize = m_bufferWidth * m_bufferHeight * RHI::GetFormatSize(RHI::Format::R32G32B32A32_FLOAT);

        RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_computeBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::ShaderReadWrite, bufferSize };
        result = m_computeBufferPool->InitBuffer(request);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized compute buffer");

        
        m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, m_bufferWidth * m_bufferHeight, RHI::GetFormatSize(RHI::Format::R32G32B32A32_FLOAT));
        m_computeBufferView = m_computeBuffer->BuildBufferView(m_bufferViewDescriptor);
                  
        if(!m_computeBufferView.get())
        {
            AZ_Assert(false, "Failed to initialized compute buffer view");
        }
        AZ_Assert(m_computeBufferView->GetDeviceBufferView(RHI::MultiDevice::DefaultDeviceIndex)->IsFullView(), "compute Buffer View initialization failed to cover in full the Compute Buffer");

    }

    void ComputeExampleComponent::CreateComputeScope()
    {
        using namespace AZ;

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // attach compute buffer
            {
                [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportBuffer(m_bufferAttachmentId, m_computeBuffer);
                AZ_Error(s_computeExampleName, result == RHI::ResultCode::Success, "Failed to import compute buffer with error %d", result);

                RHI::BufferScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_bufferAttachmentId;
                desc.m_bufferViewDescriptor = m_bufferViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);

                const Name computeBufferId{ "m_computeBuffer" };
                RHI::ShaderInputBufferIndex computeBufferIndex = m_dispatchSRGs[1]->FindShaderInputBufferIndex(computeBufferId);
                AZ_Error(s_computeExampleName, computeBufferIndex.IsValid(), "Failed to find shader input buffer %s.", computeBufferId.GetCStr());
                m_dispatchSRGs[1]->SetBufferView(computeBufferIndex, m_computeBufferView.get());
                m_dispatchSRGs[1]->Compile();
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZStd::array <const RHI::SingleDeviceShaderResourceGroup*, 8> shaderResourceGroups;
            shaderResourceGroups[0] = m_dispatchSRGs[0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
            shaderResourceGroups[1] = m_dispatchSRGs[1]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
            
            RHI::SingleDeviceDispatchItem dispatchItem;
            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_numThreadsX);
            dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_numThreadsY);
            dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_numThreadsZ);
            dispatchArgs.m_totalNumberOfThreadsX = m_bufferWidth;
            dispatchArgs.m_totalNumberOfThreadsY = m_bufferHeight;
            dispatchArgs.m_totalNumberOfThreadsZ = 1;

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_dispatchPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 2;
            dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

            commandList->Submit(dispatchItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{"Compute"},
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void ComputeExampleComponent::CreateRasterScope()
    {
        using namespace AZ;

        struct ScopeData
        {
            RPI::WindowContext* m_windowContext;
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment. Clears it to white.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(1.0f, 1.0, 1.0, 0.0);
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseColorAttachment(descriptor);
            }

            // attach compute buffer
            {
                RHI::BufferScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_bufferAttachmentId;
                desc.m_bufferViewDescriptor = m_bufferViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateVector4Float(0.0f, 0.0f, 0.0f, 0.0f);

                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::ReadWrite);

                const Name computeBufferId{ "m_computeBuffer" };
                RHI::ShaderInputBufferIndex computeBufferIndex = m_drawSRGs[1]->FindShaderInputBufferIndex(computeBufferId);
                AZ_Error(s_computeExampleName, computeBufferIndex.IsValid(), "Failed to find shader input buffer %s.", computeBufferId.GetCStr());
                m_drawSRGs[1]->SetBufferView(computeBufferIndex, m_computeBufferView.get());
                m_drawSRGs[1]->Compile();
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
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_drawSRGs[0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get(), m_drawSRGs[1]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_drawPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
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
                RHI::ScopeId{"Raster"},
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
}
