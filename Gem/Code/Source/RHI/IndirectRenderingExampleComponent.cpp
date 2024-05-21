/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/IndirectRenderingExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/MultiDeviceIndirectBufferWriter.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Random.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace IndirectRendering
    {
        const char* SampleName = "IndirectRenderingExample";
        const uint32_t ThreadGroupSize = 128;
        const char* IndirectBufferAttachmentId = "IndirectBufferAttachmentId";
        const char* CulledIndirectBufferAttachmentId = "CulledIndirectBufferAttachmentId";
        const char* CountBufferAttachmentId  = "CountBufferAttachmentId";
        const char* DepthBufferAttachmentId = "DepthBufferAttachmentId";
        const AZ::Vector2 VelocityRange(0.1f, 0.3f);
    }

    float GetRandomFloat(float min, float max)
    {
        float scale = aznumeric_cast<float>(rand()) / aznumeric_cast<float>(RAND_MAX);
        float range = max - min;
        return scale * range + min;
    }

    void IndirectRenderingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<IndirectRenderingExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    IndirectRenderingExampleComponent::IndirectRenderingExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void IndirectRenderingExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        UpdateInstancesData(deltaTime);
        if (m_updateIndirectDispatchArguments)
        {
            UpdateIndirectDispatchArguments();
            m_updateIndirectDispatchArguments = false;
        }

        if (m_imguiSidebar.Begin())
        {
            DrawSampleSettings();
        }
    }

    void IndirectRenderingExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;
        if (m_windowContext->GetSwapChain())
        {
            RHI::FrameGraphAttachmentInterface builder = frameGraphBuilder.GetAttachmentDatabase();
            // Create the depth buffer for rendering.
            const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                AZ::RHI::ImageBindFlags::DepthStencil,
                static_cast<uint32_t>(m_windowContext->GetViewport().m_maxX - m_windowContext->GetViewport().m_minX),
                static_cast<uint32_t>(m_windowContext->GetViewport().m_maxY - m_windowContext->GetViewport().m_minY),
                AZ::RHI::Format::D32_FLOAT);
            const AZ::RHI::TransientImageDescriptor transientImageDescriptor(RHI::AttachmentId{ IndirectRendering::DepthBufferAttachmentId }, imageDescriptor);
            builder.CreateTransientImage(transientImageDescriptor);

            if (m_deviceSupportsCountBuffer)
            {
                // Create the count buffer.
                RHI::TransientBufferDescriptor countBufferDesc;
                countBufferDesc.m_attachmentId = IndirectRendering::CountBufferAttachmentId;
                countBufferDesc.m_bufferDescriptor = RHI::BufferDescriptor(
                    // The count buffer must also have the Indirect BufferBind Flags, even if it doesn't contains indirect commands.
                    RHI::BufferBindFlags::Indirect | RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::CopyWrite,
                    m_resetCounterBuffer->GetDescriptor().m_byteCount);
                builder.CreateTransientBuffer(countBufferDesc);
            }

            // Create the indirect buffer that will contains the culled commands.
            RHI::TransientBufferDescriptor culledCommandsBufferDesc;
            culledCommandsBufferDesc.m_attachmentId = IndirectRendering::CulledIndirectBufferAttachmentId;
            culledCommandsBufferDesc.m_bufferDescriptor = RHI::BufferDescriptor(
                RHI::BufferBindFlags::Indirect | RHI::BufferBindFlags::ShaderReadWrite,
                m_indirectDrawBufferSignature->GetByteStride() * m_numObjects);
            builder.CreateTransientBuffer(culledCommandsBufferDesc);
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void IndirectRenderingExampleComponent::InitInputAssemblyResources()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        RHI::MultiDeviceStreamBufferView& triangleStreamBufferView = m_streamBufferViews[0];
        RHI::MultiDeviceStreamBufferView& instancesIndicesStreamBufferView = m_streamBufferViews[1];
        RHI::MultiDeviceStreamBufferView& quadStreamBufferView = m_streamBufferViews[2];

        RHI::MultiDeviceIndexBufferView& triangleIndexBufferView = m_indexBufferViews[0];
        RHI::MultiDeviceIndexBufferView& quadIndexBufferView = m_indexBufferViews[1];

        // We use an index to identify an object at draw time.
        // On platforms that support setting inline constant through indirect commands we use an inline constant to set
        // the object index. On the other platforms we use a vertex buffer stream that has a per instance step.
        // The instance offset is specified as an argument in the indirect buffer. We can't just simply use SV_InstanceID
        // in the shader because the value of that variable is not affected by the instance offset.
        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        // Object index
        layoutBuilder.AddBuffer(RHI::StreamStepFunction::PerInstance)->Channel("BLENDINDICES0", RHI::Format::R32_UINT);
        m_inputStreamLayout = layoutBuilder.End();

        m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        {
            BufferData bufferData;

            const float triangleWidth = 1.0f;
            SetVertexPosition(bufferData.m_trianglePositions.data(), 0, 0, sqrt(pow(triangleWidth * 2.0f, 2.0f) - pow(triangleWidth, 2.0f)) - triangleWidth, 0.0);
            SetVertexPosition(bufferData.m_trianglePositions.data(), 1, -triangleWidth, -triangleWidth, 0.0);
            SetVertexPosition(bufferData.m_trianglePositions.data(), 2, triangleWidth, -triangleWidth, 0.0);

            SetVertexIndexIncreasing(bufferData.m_triangleIndices.data(), bufferData.m_triangleIndices.size());

            SetFullScreenRect(bufferData.m_quadPositions.data(), nullptr, bufferData.m_quadIndices.data());

            for (uint32_t i = 0; i < bufferData.m_instanceIndices.size(); ++i)
            {
                bufferData.m_instanceIndices[i] = i;
            }

            m_inputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_inputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            m_inputAssemblyBufferPool->InitBuffer(request);

            // If the platform supports setting vertex and index buffers through indirect commands,
            // we create separate Vertex Buffer and Index Buffer views for the triangle and quad.
            // If not supported, we create one Vertex Buffer and Index Buffer view that contains both
            // primitives and we adjust the offset as an argument in the indirect buffer.
            switch (m_mode)
            {
            case SequenceType::IARootConstantsDraw:
            {
                triangleStreamBufferView =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_trianglePositions),
                    sizeof(BufferData::m_trianglePositions),
                    sizeof(VertexPosition)
                };

                triangleIndexBufferView =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_triangleIndices),
                    sizeof(BufferData::m_triangleIndices),
                    RHI::IndexFormat::Uint16
                };

                quadStreamBufferView =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_quadPositions),
                    sizeof(BufferData::m_quadPositions),
                    sizeof(VertexPosition)
                };

                quadIndexBufferView =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_quadIndices),
                    sizeof(BufferData::m_quadIndices),
                    RHI::IndexFormat::Uint16
                };
                break;
            }
            case SequenceType::DrawOnly:
            {
                m_streamBufferViews[0] =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_trianglePositions),
                    sizeof(BufferData::m_trianglePositions) + sizeof(BufferData::m_quadPositions),
                    sizeof(VertexPosition)
                };

                m_indexBufferViews[0] =
                {
                    *m_inputAssemblyBuffer,
                    offsetof(BufferData, m_triangleIndices), // Need to offset the index buffer to the proper location
                    sizeof(BufferData::m_triangleIndices) + sizeof(BufferData::m_quadIndices),
                    RHI::IndexFormat::Uint16
                };
                break;
            }
            default:
                AZ_Assert(false, "Invalid Sequence type");
                return;
            }

            instancesIndicesStreamBufferView =
            {
                *m_inputAssemblyBuffer,
                offsetof(BufferData, m_instanceIndices),
                sizeof(BufferData::m_instanceIndices),
                sizeof(uint32_t)
            };

            RHI::ValidateStreamBufferViews(m_inputStreamLayout, AZStd::span<const RHI::MultiDeviceStreamBufferView>(m_streamBufferViews.data(), 2));
        }
    }


    void IndirectRenderingExampleComponent::InitShaderResources()
    {
        {  
            auto shader = m_indirectDrawShader;
            if (shader == nullptr)
            {
                return;
            }

            AZ::RHI::PipelineStateDescriptorForDraw drawPipelineStateDescriptor;
            drawPipelineStateDescriptor.m_inputStreamLayout = m_inputStreamLayout;

            shader->GetVariant(m_indirectDrawShaderVariantStableId).ConfigurePipelineState(drawPipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat)
                ->DepthStencilAttachment(RHI::Format::D32_FLOAT);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(drawPipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            drawPipelineStateDescriptor.m_renderStates.m_depthStencilState = AZ::RHI::DepthStencilState::CreateDepth();

            m_drawPipelineState = shader->AcquirePipelineState(drawPipelineStateDescriptor);
            if (!m_drawPipelineState)
            {
                AZ_Error(IndirectRendering::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", IndirectDrawShaderFilePath);
                return;
            }

            m_sceneShaderResourceGroup = CreateShaderResourceGroup(shader, "IndirectSceneSrg", IndirectRendering::SampleName);

            // Not needed because this example uses static variants (preloaded before this function is called).
            // But left here for educational purposes:
            // Fallback in case we don't have the exact match.
            if (m_sceneShaderResourceGroup->HasShaderVariantKeyFallbackEntry())
            {
                m_sceneShaderResourceGroup->SetShaderVariantKeyFallbackValue(m_indirectDrawShaderOptionGroup.GetShaderVariantId().m_key);
            }
        }

        {
            auto shader = m_indirectDispatchShader;
            if (!shader)
            {
                return;
            }

            AZ::RHI::PipelineStateDescriptorForDispatch computePipelineStateDescriptor;
            
            shader->GetVariant(m_indirectDispatchShaderVariantStableId).ConfigurePipelineState(computePipelineStateDescriptor);

            m_cullPipelineState = shader->AcquirePipelineState(computePipelineStateDescriptor);
            if (!m_cullPipelineState)
            {
                AZ_Error(IndirectRendering::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", IndirectDispatchShaderFilePath);
                return;
            }

            m_cullShaderResourceGroup = CreateShaderResourceGroup(shader, "CullSrg", IndirectRendering::SampleName);

            const Name countBufferId{ "m_outNumCommands" };
            const Name cullOffsetId{ "m_cullOffset" };
            const Name numCommandsId{ "m_inNumCommands" };
            const Name maxCommandsId{ "m_maxDrawIndirectCount" };

            FindShaderInputIndex(&m_cullingCountBufferIndex, m_cullShaderResourceGroup, countBufferId, IndirectRendering::SampleName);
            FindShaderInputIndex(&m_cullingOffsetIndex, m_cullShaderResourceGroup, cullOffsetId, IndirectRendering::SampleName);
            FindShaderInputIndex(&m_cullingNumCommandsIndex, m_cullShaderResourceGroup, numCommandsId, IndirectRendering::SampleName);
            FindShaderInputIndex(&m_cullingMaxCommandsIndex, m_cullShaderResourceGroup, maxCommandsId, IndirectRendering::SampleName);

            const Name inputCommandsId{ "m_inputCommands" };
            const Name outputCommandsId{ "m_outputCommands" };
            const char* srgNames[] = { "IndirectDrawCommandsSrg", "IndirectIAInlineConstCommandsSrg" };
            for (uint32_t i = 0; i < NumSequencesType; ++i)
            {
                m_indirectCommandsShaderResourceGroups[i] = CreateShaderResourceGroup(shader, srgNames[i], IndirectRendering::SampleName);
                FindShaderInputIndex(&m_cullingInputIndirectBufferIndices[i], m_indirectCommandsShaderResourceGroups[i], inputCommandsId, IndirectRendering::SampleName);
                FindShaderInputIndex(&m_cullingOutputIndirectBufferIndices[i], m_indirectCommandsShaderResourceGroups[i], outputCommandsId, IndirectRendering::SampleName);
            }
        }
    }

    void IndirectRenderingExampleComponent::InitIndirectRenderingResources()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        RHI::ResultCode result;

        m_shaderBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderRead | RHI::BufferBindFlags::Indirect;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_shaderBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        // Create the layout depending on which commands are supported by the device.
        m_indirectDrawBufferLayout = RHI::IndirectBufferLayout();
        if (m_mode == SequenceType::IARootConstantsDraw)
        {
            m_indirectDrawBufferLayout.AddIndirectCommand(RHI::IndirectCommandDescriptor(RHI::IndirectCommandType::RootConstants));
            m_indirectDrawBufferLayout.AddIndirectCommand(RHI::IndirectCommandDescriptor(RHI::IndirectBufferViewArguments{ 0 }));
            m_indirectDrawBufferLayout.AddIndirectCommand(RHI::IndirectCommandDescriptor(RHI::IndirectCommandType::IndexBufferView));
        }

        m_indirectDrawBufferLayout.AddIndirectCommand(RHI::IndirectCommandDescriptor(RHI::IndirectCommandType::DrawIndexed));
        if (!m_indirectDrawBufferLayout.Finalize())
        {
            AZ_Assert(false, "Fail to finalize Indirect Layout");
            return;
        }

        // Create the signature and pass the pipeline state since we may have
        // an inline constants command.
        m_indirectDrawBufferSignature = aznew RHI::MultiDeviceIndirectBufferSignature;
        RHI::MultiDeviceIndirectBufferSignatureDescriptor signatureDescriptor;
        signatureDescriptor.m_layout = m_indirectDrawBufferLayout;
        signatureDescriptor.m_pipelineState = m_drawPipelineState.get();
        result = m_indirectDrawBufferSignature->Init(RHI::MultiDevice::AllDevices, signatureDescriptor);

        if (result != RHI::ResultCode::Success)
        {
            AZ_Assert(false, "Fail to initialize Indirect Buffer Signature");
            return;
        }

        m_sourceIndirectBuffer = aznew RHI::MultiDeviceBuffer();

        uint32_t commandsStride = m_indirectDrawBufferSignature->GetByteStride();
        RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_sourceIndirectBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor(
            bufferPoolDesc.m_bindFlags,
            commandsStride * s_maxNumberOfObjects);
        m_shaderBufferPool->InitBuffer(request);

        // Create a writer to populate the buffer with the commands.
        RHI::Ptr<RHI::MultiDeviceIndirectBufferWriter> indirectBufferWriter = aznew RHI::MultiDeviceIndirectBufferWriter;
        result = indirectBufferWriter->Init(*m_sourceIndirectBuffer, 0, commandsStride, s_maxNumberOfObjects, *m_indirectDrawBufferSignature);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Assert(false, "Fail to initialize Indirect Buffer Writer");
            return;
        }

        RHI::MultiDeviceStreamBufferView& triangleStreamBufferView = m_streamBufferViews[0];
        RHI::MultiDeviceStreamBufferView& quadStreamBufferView = m_streamBufferViews[2];

        RHI::MultiDeviceIndexBufferView& triangleIndexBufferView = m_indexBufferViews[0];
        RHI::MultiDeviceIndexBufferView& quadIndexBufferView = m_indexBufferViews[1];

        // Write the commands using the IndirectBufferWriter
        // We alternate between drawing a triangle and a quad.
        for (uint32_t i = 0; i < s_maxNumberOfObjects; ++i)
        {
            if (i % 2)
            {
                if (m_mode == SequenceType::IARootConstantsDraw)
                {
                    indirectBufferWriter->SetRootConstants(reinterpret_cast<uint8_t*>(&i), sizeof(i))
                        ->SetVertexView(0, triangleStreamBufferView)
                        ->SetIndexView(triangleIndexBufferView);
                }
                indirectBufferWriter->DrawIndexed(
                    RHI::DrawIndexed(
                        1,
                        i,
                        0,
                        3,
                        0));
            }
            else
            {
                RHI::DrawIndexed arguments(
                    1,
                    i,
                    0,
                    6,
                    0);

                switch (m_mode)
                {
                case SequenceType::IARootConstantsDraw:
                    indirectBufferWriter->SetRootConstants(reinterpret_cast<uint8_t*>(&i), sizeof(i))
                        ->SetVertexView(0, quadStreamBufferView)
                        ->SetIndexView(quadIndexBufferView);
                    break;
                case SequenceType::DrawOnly:
                    // Since we are using one vertex buffer view and one index buffer view for both type of primitives
                    // we need to adjust the vertex and index offset so they point to the proper location.
                    arguments.m_vertexOffset = decltype(BufferData::m_trianglePositions)::array_size;
                    arguments.m_indexOffset = decltype(BufferData::m_triangleIndices)::array_size;
                    break;
                default:
                    AZ_Assert(false, "Invalid sequence type");
                    return;
                }

                indirectBufferWriter->DrawIndexed(arguments);
            }

            indirectBufferWriter->NextSequence();
        }

        indirectBufferWriter->Shutdown();

        auto viewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, s_maxNumberOfObjects, commandsStride);
        m_sourceIndirectBufferView = m_sourceIndirectBuffer->BuildBufferView(viewDescriptor);
                  
        if(!m_sourceIndirectBufferView.get())
        {
            AZ_Assert(false, "Fail to initialize Indirect Buffer View");
            return;
        }

        // Create the buffer that will contain the compute dispatch arguments.
        m_indirectDispatchBuffer = aznew RHI::MultiDeviceBuffer();
        {
            m_indirectDispatchBufferLayout = RHI::IndirectBufferLayout();
            m_indirectDispatchBufferLayout.AddIndirectCommand(RHI::IndirectCommandDescriptor(RHI::IndirectCommandType::Dispatch));
            if (!m_indirectDispatchBufferLayout.Finalize())
            {
                AZ_Assert(false, "Fail to finalize Indirect Layout");
                return;
            }

            m_indirectDispatchBufferSignature = aznew RHI::MultiDeviceIndirectBufferSignature;
            signatureDescriptor = {};
            signatureDescriptor.m_layout = m_indirectDispatchBufferLayout;
            result = m_indirectDispatchBufferSignature->Init(RHI::MultiDevice::AllDevices, signatureDescriptor);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Assert(false, "Fail to initialize Indirect Buffer Signature");
                return;
            }

            uint32_t indirectDispatchStride = m_indirectDispatchBufferSignature->GetByteStride();
            request = {};
            request.m_buffer = m_indirectDispatchBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor(
                bufferPoolDesc.m_bindFlags,
                indirectDispatchStride);
            m_shaderBufferPool->InitBuffer(request);

            m_indirectDispatchBufferView =
            {
                *m_indirectDispatchBuffer,
                *m_indirectDispatchBufferSignature,
                0,
                indirectDispatchStride,
                indirectDispatchStride
            };

            m_indirectDispatchWriter = aznew RHI::MultiDeviceIndirectBufferWriter;
            result = m_indirectDispatchWriter->Init(*m_indirectDispatchBuffer, 0, indirectDispatchStride, 1, *m_indirectDispatchBufferSignature);
            if (result != RHI::ResultCode::Success)
            {
                AZ_Assert(false, "Fail to initialize Indirect Buffer Writer");
                return;
            }

            UpdateIndirectDispatchArguments();
        }
    }

    void IndirectRenderingExampleComponent::InitInstancesDataResources()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        uint32_t maxIndirectDrawCount = device->GetLimits().m_maxIndirectDrawCount;

        // Populate the data for each instance using some random values.
        m_instancesData.resize(s_maxNumberOfObjects);
        for (InstanceData& data : m_instancesData)
        {
            float scale = GetRandomFloat(0.01, 0.1f);
            data.m_offset = AZ::Vector4(GetRandomFloat(-4.0f, -2.0f), GetRandomFloat(-1.f, 1.f), GetRandomFloat(0.f, 1.f), 0.f);
            data.m_scale = AZ::Vector4(scale, scale, 1.f, 0.f);
            data.m_color = AZ::Color(GetRandomFloat(0.5f, 1.0f), GetRandomFloat(0.5f, 1.0f), GetRandomFloat(0.5f, 1.0f), 1.0f);
            data.m_velocity.Set(GetRandomFloat(IndirectRendering::VelocityRange.GetX(), IndirectRendering::VelocityRange.GetY()));
        }

        m_instancesBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::ShaderRead;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
        m_instancesBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        m_instancesDataBuffer = aznew RHI::MultiDeviceBuffer();

        RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_instancesDataBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{
            RHI::BufferBindFlags::ShaderRead,
            sizeof(InstanceData) * s_maxNumberOfObjects };
        request.m_initialData = m_instancesData.data();
        m_instancesBufferPool->InitBuffer(request);

        auto descriptor = RHI::BufferViewDescriptor::CreateStructured(0, static_cast<uint32_t>(m_instancesData.size()), sizeof(InstanceData));
        m_instancesDataBufferView = m_instancesDataBuffer->BuildBufferView(descriptor);
                  
        if(!m_instancesDataBufferView.get())
        {
            AZ_Assert(false, "Fail to initialize Instances Data Buffer View");
            return;
        }

        // Create the count buffer if the platform supports it.
        // The count buffer will contain the actual number of primitives to draw after
        // the compute shader has culled the commands.
        if (m_deviceSupportsCountBuffer)
        {
            m_copyBufferPool = aznew RHI::MultiDeviceBufferPool();

            bufferPoolDesc = {};
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::CopyRead;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Host;
            m_copyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            m_resetCounterBuffer = aznew RHI::MultiDeviceBuffer();

            AZStd::vector<uint32_t> initData;
            initData.assign(static_cast<size_t>(std::ceil(float(s_maxNumberOfObjects) / maxIndirectDrawCount)), 0);

            request = {};
            request.m_buffer = m_resetCounterBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor
            {
                RHI::BufferBindFlags::CopyRead,
                sizeof(uint32_t) * initData.size()
            };
            request.m_initialData = initData.data();
            m_copyBufferPool->InitBuffer(request);
        }

        {
            const Name instancesDataId{ "m_instancesData" };
            const Name matrixId{ "m_matrix" };

            FindShaderInputIndex(&m_sceneInstancesDataBufferIndex, m_sceneShaderResourceGroup, instancesDataId, IndirectRendering::SampleName);
            FindShaderInputIndex(&m_sceneMatrixInputIndex, m_sceneShaderResourceGroup, matrixId, IndirectRendering::SampleName);
            float screenAspect = GetViewportWidth() / GetViewportHeight();
            m_sceneShaderResourceGroup->SetBufferView(m_sceneInstancesDataBufferIndex, m_instancesDataBufferView.get());
            m_sceneShaderResourceGroup->SetConstant(m_sceneMatrixInputIndex, AZ::Matrix4x4::CreateScale(AZ::Vector3(1.f/ screenAspect, 1.f, 1.f)));
            m_sceneShaderResourceGroup->Compile();
        }

        {
            uint32_t sequenceTypeIndex = static_cast<uint32_t>(m_mode);
            m_indirectCommandsShaderResourceGroups[sequenceTypeIndex]->SetBufferView(m_cullingInputIndirectBufferIndices[sequenceTypeIndex], m_sourceIndirectBufferView.get());
        }
    }

    void IndirectRenderingExampleComponent::CreateResetCounterBufferScope()
    {
        // This scope resets the count buffer value to 0 every frame.
        // Since we increment it in the shader using InterlockedAdd we need it
        // to start with 0.
        // To reset it we just copy a buffer with the proper values into the count buffer.
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                RHI::BufferScopeAttachmentDescriptor countBufferAttachment;
                countBufferAttachment.m_attachmentId = IndirectRendering::CountBufferAttachmentId;
                countBufferAttachment.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                countBufferAttachment.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(
                    0,
                    static_cast<uint32_t>(m_resetCounterBuffer->GetDescriptor().m_byteCount / sizeof(uint32_t)),
                    sizeof(uint32_t));
                frameGraph.UseCopyAttachment(countBufferAttachment, RHI::ScopeAttachmentAccess::Write);
            }
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* countBufferView = context.GetBufferView(RHI::AttachmentId{ IndirectRendering::CountBufferAttachmentId });
            m_copyDescriptor.m_mdSourceBuffer = m_resetCounterBuffer.get();
            m_copyDescriptor.m_sourceOffset = 0;
            m_copyDescriptor.m_mdDestinationBuffer = countBufferView->GetBuffer();
            m_copyDescriptor.m_destinationOffset = 0;
            m_copyDescriptor.m_size = static_cast<uint32_t>(m_resetCounterBuffer->GetDescriptor().m_byteCount);
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::SingleDeviceCopyItem copyItem(m_copyDescriptor.GetDeviceCopyBufferDescriptor(context.GetDeviceIndex()));
            context.GetCommandList()->Submit(copyItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "IndirectResetCounterScope" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void IndirectRenderingExampleComponent::CreateCullingScope()
    {
        // This scopes culls the primitives that are outside of a designated area.
        // In order to do this it copies the commands from an input buffer into
        // and output one. If count buffers are supported only valid commands are copied.
        // Otherwise it copies all commands but it updates the vertex count to 0 so no
        // triangles are rendered.
        // The dispatch call for this scope is done in an indirect manner. The indirect buffer for this dispatch
        // is populated on CPU.
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        uint32_t maxIndirectDrawCount = device->GetLimits().m_maxIndirectDrawCount;

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            uint32_t commandsStride = m_indirectDrawBufferSignature->GetByteStride();

            RHI::BufferScopeAttachmentDescriptor culledBufferAttachment;
            culledBufferAttachment.m_attachmentId = IndirectRendering::CulledIndirectBufferAttachmentId;
            culledBufferAttachment.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
            culledBufferAttachment.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(0, m_numObjects, commandsStride);
            frameGraph.UseShaderAttachment(culledBufferAttachment, RHI::ScopeAttachmentAccess::ReadWrite);

            if (m_deviceSupportsCountBuffer)
            {
                // The count buffer that we will be writing the final count of operations.
                RHI::BufferScopeAttachmentDescriptor countBufferAttachment;
                countBufferAttachment.m_attachmentId = IndirectRendering::CountBufferAttachmentId;
                countBufferAttachment.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                countBufferAttachment.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(
                    0,
                    static_cast<uint32_t>(m_resetCounterBuffer->GetDescriptor().m_byteCount / sizeof(uint32_t)),
                    sizeof(uint32_t));
                frameGraph.UseShaderAttachment(countBufferAttachment, RHI::ScopeAttachmentAccess::ReadWrite);
            }
        };

        const auto compileFunction = [this, maxIndirectDrawCount](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {

            const auto* culledBufferView = context.GetBufferView(RHI::AttachmentId{ IndirectRendering::CulledIndirectBufferAttachmentId });

            uint32_t sequenceTypeIndex = static_cast<uint32_t>(m_mode);
            auto& indirectCommandsSRG = m_indirectCommandsShaderResourceGroups[sequenceTypeIndex];
            indirectCommandsSRG->SetBufferView(m_cullingOutputIndirectBufferIndices[sequenceTypeIndex], culledBufferView);
            indirectCommandsSRG->Compile();

            if (m_deviceSupportsCountBuffer)
            {
                const auto* countBufferView = context.GetBufferView(RHI::AttachmentId{ IndirectRendering::CountBufferAttachmentId });

                m_cullShaderResourceGroup->SetBufferView(m_cullingCountBufferIndex, countBufferView);
                m_drawIndirect.m_countBuffer = countBufferView->GetBuffer();
                m_drawIndirect.m_countBufferByteOffset = 0;
            }

            // Update the cull area
            float cullScale = 0.75f;
            AZ::Vector2 cullPlane = AZ::Vector2(-m_cullOffset, m_cullOffset) * AZ::Vector2(cullScale) + AZ::Vector2(cullScale - 1.0f);
            m_cullShaderResourceGroup->SetConstant(m_cullingOffsetIndex, cullPlane);
            m_cullShaderResourceGroup->SetConstant(m_cullingNumCommandsIndex, m_numObjects);
            m_cullShaderResourceGroup->SetConstant(m_cullingMaxCommandsIndex, AZStd::min(m_numObjects, maxIndirectDrawCount));
            m_cullShaderResourceGroup->Compile();

            uint32_t stride = m_indirectDrawBufferSignature->GetByteStride();
            m_indirectDrawBufferView =
            {
                *(culledBufferView->GetBuffer()),
                *m_indirectDrawBufferSignature,
                0,
                stride * m_numObjects,
                stride
            };
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            RHI::SingleDeviceDispatchItem dispatchItem;
            uint32_t numSrgs = 0;
            dispatchItem.m_shaderResourceGroups[numSrgs++] = m_cullShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroups[numSrgs++] = m_sceneShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();

            for (const auto& srg : m_indirectCommandsShaderResourceGroups)
            {
                dispatchItem.m_shaderResourceGroups[numSrgs++] = srg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
            }

            // Submit the dispatch in an indirect manner.
            // Not really needed but it tests the indirect dispatch code.
            RHI::DispatchIndirect dispatchArgs(1, m_indirectDispatchBufferView.GetDeviceIndirectBufferView(context.GetDeviceIndex()), 0);

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_cullPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = static_cast<uint8_t>(numSrgs);

            commandList->Submit(dispatchItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "IndirecDispatchScope" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }


    void IndirectRenderingExampleComponent::CreateDrawingScope()
    {
        // Scope responsible for drawing the primitives in an indirect manner
        // using an indirect buffer that was populated by a compute shader.
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        uint32_t maxIndirectDrawCount = device->GetLimits().m_maxIndirectDrawCount;

        const auto prepareFunction = [this, maxIndirectDrawCount](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                // Binds the swap chain as a color attachment.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            {
                // Binds the transient depth attachment
                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = IndirectRendering::DepthBufferAttachmentId;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = AZ::RHI::Format::D32_FLOAT;
                dsDesc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateDepth(1.f);
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::ReadWrite);
            }

            if (m_deviceSupportsCountBuffer)
            {
                // Count buffer.
                RHI::BufferScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = IndirectRendering::CountBufferAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                descriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(
                    0,
                    static_cast<uint32_t>(m_resetCounterBuffer->GetDescriptor().m_byteCount / sizeof(uint32_t)),
                    sizeof(uint32_t));
                frameGraph.UseAttachment(descriptor, RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentUsage::Indirect);
            }

            {
                // Indirect Buffer with the culled commands.
                RHI::BufferScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = IndirectRendering::CulledIndirectBufferAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                descriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(
                    0,
                    m_numObjects,
                    m_indirectDrawBufferSignature->GetByteStride());
                frameGraph.UseAttachment(descriptor, RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentUsage::Indirect);
            }

            frameGraph.SetEstimatedItemCount(uint32_t(std::ceil(m_numObjects/ float(maxIndirectDrawCount))));
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this, maxIndirectDrawCount](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_sceneShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            // In case multi indirect drawing is not supported
            // we need to emit multiple indirect draw calls.
            m_drawIndirect.m_countBufferByteOffset = 0;
            for (uint32_t i = 0;
                i < m_numObjects;
                i += maxIndirectDrawCount, m_drawIndirect.m_countBufferByteOffset += sizeof(uint32_t))
            {
                // Offset the indirect buffer depending on the number of indirect draw calls that we can do.
                m_drawIndirect.m_maxSequenceCount = AZStd::min(m_numObjects - i, maxIndirectDrawCount);
                m_drawIndirect.m_indirectBufferByteOffset = i * m_indirectDrawBufferView.GetByteStride();
                m_drawIndirect.m_indirectBufferView = &m_indirectDrawBufferView;

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_arguments = AZ::RHI::MultiDeviceDrawArguments(m_drawIndirect).GetDeviceDrawArguments(context.GetDeviceIndex());
                drawItem.m_pipelineState = m_drawPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                auto deviceIndexBufferView{m_indexBufferViews[0].GetDeviceIndexBufferView(context.GetDeviceIndex())};
                drawItem.m_indexBufferView = &deviceIndexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = 2;
                AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                // Submit the indirect draw item.
                commandList->Submit(drawItem);
            }
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                RHI::ScopeId{ "IndirectDrawScope" },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void IndirectRenderingExampleComponent::Activate()
    {
        using namespace AZ;

        m_numObjects = s_maxNumberOfObjects / 2;

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        const auto& deviceFeatures = device->GetFeatures();
        // Select the commands in the layout depending on the device capabilities.
        switch (deviceFeatures.m_indirectCommandTier)
        {
        case RHI::IndirectCommandTiers::Tier1:
            m_mode = SequenceType::DrawOnly;
            break;
        case RHI::IndirectCommandTiers::Tier2:
            m_mode = SequenceType::IARootConstantsDraw;
            break;
        default:
            AZ_Assert(false, "Indirect Rendering is not supported on this platform.");
            return;
        }

        m_deviceSupportsCountBuffer = deviceFeatures.m_indirectDrawCountBufferSupported;

        m_assetLoadManager = AZStd::make_unique<AZ::AssetCollectionAsyncLoader>();
        m_doneLoadingShaders = false;
        m_indirectDrawShaderVariantStableId = AZ::RPI::RootShaderVariantStableId;
        m_indirectDispatchShaderVariantStableId = AZ::RPI::RootShaderVariantStableId;

        // List of all assets this example needs.
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {IndirectDrawShaderFilePath, azrtti_typeid<RPI::ShaderAsset>()},
            {IndirectDispatchShaderFilePath, azrtti_typeid<RPI::ShaderAsset>()},
        };

        // Configure the imgui progress list widget.
        auto onUserCancelledAction = [&]()
        {
            //AZ_TracePrintf(AsyncCompute::sampleName, "Cancelled by user.\n");
            m_assetLoadManager->Cancel();
            SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::Reset);
        };

        m_imguiProgressList.OpenPopup("Waiting For Assets...", "Assets pending for processing:", {}, onUserCancelledAction, true /*automaticallyCloseOnAction*/, "Cancel");

        AZStd::for_each(assetList.begin(), assetList.end(),
            [&](const AssetCollectionAsyncLoader::AssetToLoadInfo& item) { m_imguiProgressList.AddItem(item.m_assetPath); });
        m_imguiProgressList.AddItem(IndirectDrawVariantLabel);
        m_imguiProgressList.AddItem(IndirectDispatchVariantLabel);

        // Kickoff asynchronous asset loading, the activation will continue once all assets are available.
        m_assetLoadManager->LoadAssetsAsync(assetList, [&](AZStd::string_view assetName, [[maybe_unused]] bool success, size_t pendingAssetCount)
            {
                AZ_Error(LogName, success, "Error loading asset %s, a crash will occur when OnAllAssetsReadyActivate() is called!", assetName.data());
                m_imguiProgressList.RemoveItem(assetName);
                if (!pendingAssetCount && !m_doneLoadingShaders)
                {
                    m_doneLoadingShaders = true;
                    InitRequestsForShaderVariants();
                }
            });

    }

    void IndirectRenderingExampleComponent::OnAllAssetsReadyActivate()
    {
        InitInputAssemblyResources();
        InitShaderResources();
        InitIndirectRenderingResources();
        InitInstancesDataResources();

        // We use 3 scopes.
        // The first one is for reseting the count buffer to 0.
        // The second one is the compute scope in charge of culling.
        // The last one is the graphic scope in charge of rendering the culled primitives.
        if (m_deviceSupportsCountBuffer)
        {
            CreateResetCounterBufferScope();
        }
        CreateCullingScope();
        CreateDrawingScope();

        m_imguiSidebar.Activate();
        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AzFramework::WindowNotificationBus::Handler::BusConnect(m_windowContext->GetWindowHandle());
    }

    void IndirectRenderingExampleComponent::Deactivate()
    {
        m_inputAssemblyBufferPool = nullptr;
        m_shaderBufferPool = nullptr;
        m_instancesBufferPool = nullptr;
        m_copyBufferPool = nullptr;

        m_inputAssemblyBuffer = nullptr;
        m_sourceIndirectBuffer = nullptr;
        m_instancesDataBuffer = nullptr;
        m_resetCounterBuffer = nullptr;

        m_drawPipelineState = nullptr;
        m_cullPipelineState = nullptr;

        m_sceneShaderResourceGroup = nullptr;
        m_cullShaderResourceGroup = nullptr;
        m_indirectCommandsShaderResourceGroups.fill(nullptr);

        m_sourceIndirectBufferView = nullptr;
        m_instancesDataBufferView = nullptr;

        m_indirectDispatchWriter = nullptr;

        m_indirectDrawBufferSignature = nullptr;
        m_indirectDispatchBufferSignature = nullptr;

        m_instancesData.clear();

        m_imguiSidebar.Deactivate();
        AzFramework::WindowNotificationBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        AZ::RPI::ShaderReloadNotificationBus::MultiHandler::BusDisconnect();

        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void IndirectRenderingExampleComponent::DrawSampleSettings()
    {
        ImGui::Spacing();
        ScriptableImGui::SliderFloat("Cull Offset", &m_cullOffset, 0.f, 1.f);
        if (ScriptableImGui::SliderInt("Num Objects", reinterpret_cast<int*>(&m_numObjects), 1, s_maxNumberOfObjects))
        {
            m_updateIndirectDispatchArguments = true;
        }
        m_imguiSidebar.End();
    }

    void IndirectRenderingExampleComponent::UpdateInstancesData(float deltaTime)
    {
        using namespace AZ;
        AZ::SimpleLcgRandom random;
        const float offsetBounds = 2.5f;
        for (uint32_t i = 0; i < m_numObjects; ++i)
        {
            m_instancesData[i].m_offset.SetX(m_instancesData[i].m_offset.GetX() + m_instancesData[i].m_velocity.GetX() * deltaTime);
            if (m_instancesData[i].m_offset.GetX() > offsetBounds)
            {
                m_instancesData[i].m_offset.SetX(-offsetBounds);
                m_instancesData[i].m_velocity.Set(GetRandomFloat(IndirectRendering::VelocityRange.GetX(), IndirectRendering::VelocityRange.GetY()));
            }
        }

        RHI::MultiDeviceBufferMapRequest request(*m_instancesDataBuffer, 0, sizeof(InstanceData) * m_numObjects);
        RHI::MultiDeviceBufferMapResponse response;

        m_instancesBufferPool->MapBuffer(request, response);
        if (!response.m_data.empty())
        {
            for(auto& [_, responseData] : response.m_data)
            {
                ::memcpy(responseData, m_instancesData.data(), request.m_byteCount);
            }
            m_instancesBufferPool->UnmapBuffer(*m_instancesDataBuffer);
        }
    }

    void IndirectRenderingExampleComponent::OnWindowResized(uint32_t width, uint32_t height)
    {
        if (m_sceneShaderResourceGroup)
        {
            float aspectRatio = width / float(height);
            m_sceneShaderResourceGroup->SetConstant(m_sceneMatrixInputIndex, AZ::Matrix4x4::CreateScale(AZ::Vector3(1.f / aspectRatio, 1.f, 1.f)));
            m_sceneShaderResourceGroup->Compile();
        }
    }

    void IndirectRenderingExampleComponent::UpdateIndirectDispatchArguments()
    {
        m_indirectDispatchWriter->Seek(0);

        RHI::DispatchDirect args;
        args.m_threadsPerGroupX = IndirectRendering::ThreadGroupSize;
        args.m_totalNumberOfThreadsX = aznumeric_cast<uint16_t>(m_numObjects);
        m_indirectDispatchWriter->Dispatch(args);

        m_indirectDispatchWriter->Flush();
    }

    void IndirectRenderingExampleComponent::InitRequestsForShaderVariants()
    {
        {
            m_indirectDrawShader = LoadShader(*m_assetLoadManager.get(), IndirectDrawShaderFilePath, IndirectRendering::SampleName);
            AZ::RPI::ShaderReloadNotificationBus::MultiHandler::BusConnect(m_indirectDrawShader->GetAsset().GetId());
            // We use a shader option to tell the shader if the object index must be read from the vertex input or an inline constant.
            m_indirectDrawShaderOptionGroup = m_indirectDrawShader->CreateShaderOptionGroup();
            m_indirectDrawShaderOptionGroup.SetValue(AZ::Name("o_useRootConstants"), m_mode == SequenceType::IARootConstantsDraw ? AZ::Name("true") : AZ::Name("false"));
            //Kickoff the asynchronous loading of the variant tree.
            auto shaderVariant = m_indirectDrawShader->GetVariant(m_indirectDrawShaderOptionGroup.GetShaderVariantId());
            if (!shaderVariant.IsRootVariant())
            {
                m_indirectDrawShaderVariantStableId = shaderVariant.GetStableId();
                m_imguiProgressList.RemoveItem(IndirectDrawVariantLabel);
                AZ::RPI::ShaderReloadNotificationBus::MultiHandler::BusDisconnect(m_indirectDrawShader->GetAsset().GetId());
            }
        }

        {
            m_indirectDispatchShader = LoadShader(*m_assetLoadManager.get(), IndirectDispatchShaderFilePath, IndirectRendering::SampleName);
            AZ::RPI::ShaderReloadNotificationBus::MultiHandler::BusConnect(m_indirectDispatchShader->GetAsset().GetId());
            // We use a shader option to tell the shader which commands are included in the buffer and
            // another option to tell the shader if count buffers are supported.
            m_indirectDispatchShaderOptionGroup = m_indirectDispatchShader->CreateShaderOptionGroup();
            const char* optionValues[] = { "SequenceType::Draw", "SequenceType::IAInlineConstDraw" };
            m_indirectDispatchShaderOptionGroup.SetValue(AZ::Name("o_sequenceType"), AZ::Name(optionValues[static_cast<uint32_t>(m_mode)]));
            m_indirectDispatchShaderOptionGroup.SetValue(AZ::Name("o_countBufferSupported"), m_deviceSupportsCountBuffer ? AZ::Name("true") : AZ::Name("false"));
            //Kickoff the asynchronous loading of the variant tree.
            auto shaderVariant = m_indirectDispatchShader->GetVariant(m_indirectDispatchShaderOptionGroup.GetShaderVariantId());
            if (!shaderVariant.IsRootVariant())
            {
                m_indirectDispatchShaderVariantStableId = shaderVariant.GetStableId();
                m_imguiProgressList.RemoveItem(IndirectDispatchVariantLabel);
                AZ::RPI::ShaderReloadNotificationBus::MultiHandler::BusDisconnect(m_indirectDispatchShader->GetAsset().GetId());
            }
        }

        if ((m_indirectDrawShaderVariantStableId != AZ::RPI::RootShaderVariantStableId) &&
            (m_indirectDispatchShaderVariantStableId != AZ::RPI::RootShaderVariantStableId))
        {
            AZ_TracePrintf(LogName, "Got all variants already from InitRequestsForShaderVariantAssets()\n");
            OnAllAssetsReadyActivate();
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // ShaderReloadNotificationBus overrides

    void IndirectRenderingExampleComponent::OnShaderVariantReinitialized(const AZ::RPI::ShaderVariant& shaderVariant)
    {
        //AZ_TracePrintf(LogName, "Got variant %s\n", shaderVariantAsset.GetHint().c_str());
        const AZ::Data::AssetId* shaderAssetId = AZ::RPI::ShaderReloadNotificationBus::GetCurrentBusId();
        if (*shaderAssetId == m_indirectDrawShader->GetAsset().GetId())
        {
            m_indirectDrawShaderVariantStableId = shaderVariant.GetStableId();
            m_imguiProgressList.RemoveItem(IndirectDrawVariantLabel);
        }
        else if (*shaderAssetId == m_indirectDispatchShader->GetAsset().GetId())
        {
            m_indirectDispatchShaderVariantStableId = shaderVariant.GetStableId();
            m_imguiProgressList.RemoveItem(IndirectDispatchVariantLabel);
        }

        if ((m_indirectDrawShaderVariantStableId != AZ::RPI::RootShaderVariantStableId) &&
            (m_indirectDispatchShaderVariantStableId != AZ::RPI::RootShaderVariantStableId))
        {
            AZ_TracePrintf(LogName, "Got all variants\n");
            OnAllAssetsReadyActivate();
        }
    }
    ///////////////////////////////////////////////////////////////////////

} // namespace AtomSampleViewer
