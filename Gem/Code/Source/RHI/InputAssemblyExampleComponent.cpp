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

#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/InputAssemblyExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    namespace InputAssembly
    {
        const char* SampleName = "InputaAssemblyExample";
        const char* const ShaderInputTime{ "m_time" };
        const char* const ShaderInpuIABuffer{ "m_IABuffer" };
        const char* const ShaderInputMatrix{ "m_matrix" };
        const char* const ShaderInputColor{ "m_color" };
        const char* InputAssemblyBufferAttachmentId = "InputAssemblyBufferAttachmentId";
        const char* ImportedInputAssemblyBufferAttachmentId = "ImportedInputAssemblyBufferAttachmentId";
    }

    void InputAssemblyExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<InputAssemblyExampleComponent, AZ::Component>()->Version(0);
        }
    }

    InputAssemblyExampleComponent::InputAssemblyExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void InputAssemblyExampleComponent::FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;
        if (m_windowContext->GetSwapChain())
        {
            // Create Input Assembly buffer
            {
                RHI::TransientBufferDescriptor bufferDesc;
                bufferDesc.m_attachmentId = InputAssembly::InputAssemblyBufferAttachmentId;
                bufferDesc.m_bufferDescriptor = RHI::BufferDescriptor(
                    RHI::BufferBindFlags::InputAssembly | RHI::BufferBindFlags::ShaderReadWrite,
                    sizeof(BufferData));
                frameGraphBuilder.GetAttachmentDatabase().CreateTransientBuffer(bufferDesc);
            }

            {
                frameGraphBuilder.GetAttachmentDatabase().ImportBuffer(AZ::Name{ InputAssembly::ImportedInputAssemblyBufferAttachmentId }, m_inputAssemblyBuffer);
            }

            float aspectRatio = static_cast<float>(m_outputWidth / m_outputHeight);
            AZ::Vector2 scale(AZStd::min(1.0f / aspectRatio, 1.0f), AZStd::min(aspectRatio, 1.0f));
            {
                AZ::Matrix4x4 scaleTranslate =
                    AZ::Matrix4x4::CreateTranslation(AZ::Vector3(0.4f, 0.4f, 0)) *
                    AZ::Matrix4x4::CreateScale(AZ::Vector3(scale.GetX() * 0.6f, scale.GetY() * 0.6f, 1.0f));
                m_drawSRG[0]->SetConstant(m_drawMatrixIndex, scaleTranslate);
                m_drawSRG[0]->SetConstant(m_drawColorIndex, AZ::Vector4(1.0, 0, 0, 1.0f));
                m_drawSRG[0]->Compile();
            }
            {
                AZ::Matrix4x4 scaleTranslate =
                    AZ::Matrix4x4::CreateTranslation(AZ::Vector3(-0.4f, -0.4f, 0)) *
                    AZ::Matrix4x4::CreateScale(AZ::Vector3(scale.GetX() * 0.4f, scale.GetY() * 0.4f, 1.0f));
                m_drawSRG[1]->SetConstant(m_drawMatrixIndex, scaleTranslate);
                m_drawSRG[1]->SetConstant(m_drawColorIndex, AZ::Vector4(0.0, 1, 0, 1.0f));
                m_drawSRG[1]->Compile();
            }
        }
    }

    void InputAssemblyExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        m_time += deltaTime;
    }

    void InputAssemblyExampleComponent::Activate()
    {
        CreateInputAssemblyLayout();
        CreateBuffers();

        LoadComputeShader();
        LoadRasterShader();

        CreateComputeScope();
        CreateRasterScope();

        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void InputAssemblyExampleComponent::Deactivate()
    {
        m_dispatchPipelineState = nullptr;
        m_drawPipelineState = nullptr;
        m_dispatchSRG[0] = nullptr;
        m_dispatchSRG[1] = nullptr;
        m_drawSRG[0] = nullptr;
        m_drawSRG[1] = nullptr;

        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_scopeProducers.clear();
        m_windowContext = nullptr;

        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
    }

    void InputAssemblyExampleComponent::CreateInputAssemblyLayout()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32A32_FLOAT);
        layoutBuilder.SetTopology(RHI::PrimitiveTopology::TriangleStrip);
        m_inputStreamLayout = layoutBuilder.End();
    }

    void InputAssemblyExampleComponent::CreateBuffers()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew RHI::BufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly | RHI::BufferBindFlags::ShaderReadWrite;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        m_inputAssemblyBuffer = aznew RHI::Buffer();

        RHI::BufferInitRequest request;
        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly | RHI::BufferBindFlags::ShaderReadWrite, sizeof(BufferData) };
        request.m_initialData = nullptr;
        m_inputAssemblyBufferPool->InitBuffer(request);
    }

    void InputAssemblyExampleComponent::LoadComputeShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/InputAssemblyCompute.azshader";

        const auto shader = LoadShader(shaderFilePath, InputAssembly::SampleName);
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
            AZ_Error(InputAssembly::SampleName, false, "Did not find expected numthreads attribute");
        }

        m_dispatchPipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_dispatchPipelineState)
        {
            AZ_Error(InputAssembly::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_dispatchSRG[0] = CreateShaderResourceGroup(shader, "DispatchSRG", InputAssembly::SampleName);
        m_dispatchSRG[1] = CreateShaderResourceGroup(shader, "DispatchSRG", InputAssembly::SampleName);
        FindShaderInputIndex(&m_dispatchTimeConstantIndex, m_dispatchSRG[0], AZ::Name{ InputAssembly::ShaderInputTime }, InputAssembly::SampleName);
        FindShaderInputIndex(&m_dispatchIABufferIndex, m_dispatchSRG[0], AZ::Name{ InputAssembly::ShaderInpuIABuffer }, InputAssembly::SampleName);
    }

    void InputAssemblyExampleComponent::LoadRasterShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/InputAssemblyDraw.azshader";

        auto shader = LoadShader(shaderFilePath, InputAssembly::SampleName);
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
            AZ_Error(InputAssembly::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        m_drawSRG[0] = CreateShaderResourceGroup(shader, "DrawSRG", InputAssembly::SampleName);
        m_drawSRG[1] = CreateShaderResourceGroup(shader, "DrawSRG", InputAssembly::SampleName);
        FindShaderInputIndex(&m_drawMatrixIndex, m_drawSRG[0], AZ::Name{ InputAssembly::ShaderInputMatrix }, InputAssembly::SampleName);
        FindShaderInputIndex(&m_drawColorIndex, m_drawSRG[0], AZ::Name{ InputAssembly::ShaderInputColor }, InputAssembly::SampleName);
    }    

    void InputAssemblyExampleComponent::CreateComputeScope()
    {
        using namespace AZ;

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        const auto prepareFunction = [](RHI::FrameGraphInterface frameGraph, ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            // Declare usage of the vertex buffer as UAV
            {
                RHI::BufferScopeAttachmentDescriptor attachmentDescriptor;
                attachmentDescriptor.m_attachmentId = InputAssembly::InputAssemblyBufferAttachmentId;
                attachmentDescriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, BufferData::array_size, sizeof(BufferData::value_type));
                attachmentDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseShaderAttachment(attachmentDescriptor, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            {
                RHI::BufferScopeAttachmentDescriptor attachmentDescriptor;
                attachmentDescriptor.m_attachmentId = InputAssembly::ImportedInputAssemblyBufferAttachmentId;
                attachmentDescriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, BufferData::array_size, sizeof(BufferData::value_type));
                attachmentDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseShaderAttachment(attachmentDescriptor, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            frameGraph.SetEstimatedItemCount(2);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, const ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            {
                const auto* inputAssemblyBufferView = context.GetBufferView(RHI::AttachmentId{ InputAssembly::InputAssemblyBufferAttachmentId });
                m_dispatchSRG[0]->SetBufferView(m_dispatchIABufferIndex, inputAssemblyBufferView);
                m_dispatchSRG[0]->SetConstant(m_dispatchTimeConstantIndex, m_time);
                m_dispatchSRG[0]->Compile();
            }

            {
                const auto* inputAssemblyBufferView = context.GetBufferView(RHI::AttachmentId{ InputAssembly::ImportedInputAssemblyBufferAttachmentId });
                m_dispatchSRG[1]->SetBufferView(m_dispatchIABufferIndex, inputAssemblyBufferView);
                m_dispatchSRG[1]->SetConstant(m_dispatchTimeConstantIndex, m_time);
                m_dispatchSRG[1]->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, const ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            RHI::CommandList* commandList = context.GetCommandList();

            RHI::DeviceDispatchItem dispatchItem;
            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_numThreadsX);
            dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_numThreadsY);
            dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_numThreadsZ);
            dispatchArgs.m_totalNumberOfThreadsX = 1;
            dispatchArgs.m_totalNumberOfThreadsY = 1;
            dispatchArgs.m_totalNumberOfThreadsZ = 1;

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_dispatchPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 1;

            for (uint32_t index = context.GetSubmitRange().m_startIndex; index < context.GetSubmitRange().m_endIndex; ++index)
            {
                dispatchItem.m_shaderResourceGroups[0] = m_dispatchSRG[index]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
                commandList->Submit(dispatchItem, index);
            }
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{"IACompute"},
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void InputAssemblyExampleComponent::CreateRasterScope()
    {
        using namespace AZ;

        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Declare the usage of the vertex buffer as Input Assembly. This is needed because we modify the vertex buffer in the GPU
            // and it needs synchronization.
            {
                RHI::BufferScopeAttachmentDescriptor attachmentDescriptor;
                attachmentDescriptor.m_attachmentId = InputAssembly::InputAssemblyBufferAttachmentId;
                attachmentDescriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, BufferData::array_size, sizeof(BufferData::value_type));
                attachmentDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                attachmentDescriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                frameGraph.UseInputAssemblyAttachment(attachmentDescriptor);
            }

            {
                RHI::BufferScopeAttachmentDescriptor attachmentDescriptor;
                attachmentDescriptor.m_attachmentId = InputAssembly::ImportedInputAssemblyBufferAttachmentId;
                attachmentDescriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, BufferData::array_size, sizeof(BufferData::value_type));
                attachmentDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                attachmentDescriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                frameGraph.UseInputAssemblyAttachment(attachmentDescriptor);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(2);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, const ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            {
                const auto* inputAssemblyBufferView = context.GetBufferView(RHI::AttachmentId{ InputAssembly::InputAssemblyBufferAttachmentId });
                if (inputAssemblyBufferView)
                {
                    m_streamBufferView[0] = {*(inputAssemblyBufferView->GetBuffer()), 0, sizeof(BufferData), sizeof(BufferData::value_type)};

                    RHI::ValidateStreamBufferViews(m_inputStreamLayout, AZStd::span<const RHI::StreamBufferView>(&m_streamBufferView[0], 1));
                }
            }

            {
                const auto* inputAssemblyBufferView = context.GetBufferView(RHI::AttachmentId{ InputAssembly::ImportedInputAssemblyBufferAttachmentId });
                if (inputAssemblyBufferView)
                {
                    m_streamBufferView[1] = {*(inputAssemblyBufferView->GetBuffer()), 0, sizeof(BufferData), sizeof(BufferData::value_type)};

                    RHI::ValidateStreamBufferViews(m_inputStreamLayout, AZStd::span<const RHI::StreamBufferView>(&m_streamBufferView[1], 1));
                }
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, const ScopeData& scopeData)
        {
            AZ_UNUSED(scopeData);
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawLinear drawLinear;
            drawLinear.m_vertexCount = BufferData::array_size;

            RHI::DeviceDrawItem drawItem;
            drawItem.m_arguments = drawLinear;
            drawItem.m_pipelineState = m_drawPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            drawItem.m_indexBufferView = nullptr;
            drawItem.m_streamBufferViewCount = 1;
            drawItem.m_shaderResourceGroupCount = 1;

            for (uint32_t index = context.GetSubmitRange().m_startIndex; index < context.GetSubmitRange().m_endIndex; ++index)
            {
                auto deviceStreamBufferView{m_streamBufferView[index].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = &deviceStreamBufferView;

                RHI::DeviceShaderResourceGroup* rhiSRGS[] = {
                    m_drawSRG[index]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };
                drawItem.m_shaderResourceGroups = rhiSRGS;

                commandList->Submit(drawItem, index);
            }
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{"IARaster"},
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
}
