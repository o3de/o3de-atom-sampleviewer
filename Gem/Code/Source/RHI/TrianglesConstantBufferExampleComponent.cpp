/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/TrianglesConstantBufferExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/RHI/ScopeProducerFunction.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    const char* TrianglesConstantBufferExampleComponent::s_trianglesConstantBufferExampleName = "TrianglesConstantBufferExample";
    const uint32_t TrianglesConstantBufferExampleComponent::s_numberOfTrianglesTotal = 30;

    void TrianglesConstantBufferExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TrianglesConstantBufferExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    TrianglesConstantBufferExampleComponent::TrianglesConstantBufferExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void TrianglesConstantBufferExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_time += deltaTime;
    }

    void TrianglesConstantBufferExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        using namespace AZ;

        InstanceInfo trianglesData[s_numberOfTrianglesTotal];

        // Distribute the triangles evenly along the screen
        const uint32_t numColumns = static_cast<uint32_t>(ceil(sqrt(s_numberOfTrianglesTotal)));
        const uint32_t lastTriangleIndex = s_numberOfTrianglesTotal - 1u;
        const uint32_t numRows = (lastTriangleIndex / numColumns) + 1u; // Last triangle's row index plus one is the actual number of rows

        for (uint32_t triangleIdx = 0u; triangleIdx < s_numberOfTrianglesTotal; ++triangleIdx)
        {
            // Location in range [0,1]
            const float offsetX = 1.0f / static_cast<float>(numColumns);
            const float offsetY = 1.0f / static_cast<float>(numRows);
            float x = (triangleIdx % numColumns) * offsetX + 0.5f * offsetX;
            float y = (triangleIdx / numColumns) * offsetY + 0.5f * offsetY;

            // Location in clip Space [0,1] -> [-1,1]. Triangles located top to bottom, left to right.
            x = 2.0f * x - 1.0f;
            y = -(2.0f * y - 1.0f); // Finally inverse y so it starts from top to bottom

            float time = m_time;
            time *= float(triangleIdx + 1u) * 0.2f; // Faster the bigger the index

            // Move the triangle around.
            AZ::Vector3 translation(
                x + sinf(time) * 0.05f,
                y + cosf(time) * 0.05f,
                0.0f);

            //[GFX_TODO] ATOM-5582 - InstanceInfo should use Atom specific Matrix types
            AZ::Matrix4x4 translationMat = AZ::Matrix4x4::CreateTranslation(translation);
            translationMat.StoreToRowMajorFloat16(&trianglesData[triangleIdx].m_matrix[0]);
            
            AZ::Color colorMultiplier = fabsf(sinf(time)) * AZ::Color::CreateOne();
            colorMultiplier.StoreToFloat4(&trianglesData[triangleIdx].m_colorMultiplier[0]);
        }

        // All triangles data will be uploaded in one go to the constant buffer once per frame.
        UploadDataToConstantBuffer(trianglesData, static_cast<uint32_t>(sizeof(InstanceInfo)), s_numberOfTrianglesTotal);
        
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void TrianglesConstantBufferExampleComponent::UploadDataToConstantBuffer(InstanceInfo* data, uint32_t elementSize, uint32_t elementCount)
    {
        AZ::RHI::BufferMapRequest mapRequest;
        mapRequest.m_buffer = m_constantBuffer.get();
        mapRequest.m_byteCount = elementSize * elementCount;
        AZ::RHI::BufferMapResponse mapResponse;
        AZ::RHI::ResultCode resultCode = m_constantBufferPool->MapBuffer(mapRequest, mapResponse);
        if (resultCode == AZ::RHI::ResultCode::Success)
        {
            for(const auto& [_, bufferData] : mapResponse.m_data)
            {
                memcpy(bufferData, data, sizeof(InstanceInfo) * elementCount);
            }
            m_constantBufferPool->UnmapBuffer(*mapRequest.m_buffer);
        }
    }

    void TrianglesConstantBufferExampleComponent::Activate()
    {
        using namespace AZ;

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        // Creates Input Assembly buffer and Streams/Index Views
        {
            m_inputAssemblyBufferPool = aznew RHI::BufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

            IABufferData bufferData;

            SetVertexPosition(bufferData.m_positions.data(), 0,  0.0f,  0.1f, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 1, -0.1f, -0.1f, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 2,  0.1f, -0.1f, 0.0);

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
                offsetof(IABufferData, m_positions),
                sizeof(IABufferData::m_positions),
                sizeof(VertexPosition)
            };

            m_streamBufferViews[1] =
            {
                *m_inputAssemblyBuffer,
                offsetof(IABufferData, m_colors),
                sizeof(IABufferData::m_colors),
                sizeof(VertexColor)
            };

            m_indexBufferView =
            {
                *m_inputAssemblyBuffer,
                offsetof(IABufferData, m_indices),
                sizeof(IABufferData::m_indices),
                RHI::IndexFormat::Uint16
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("COLOR", RHI::Format::R32G32B32A32_FLOAT);
            pipelineStateDescriptor.m_inputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(pipelineStateDescriptor.m_inputStreamLayout, m_streamBufferViews);
        }

        // Create the buffer pool where both buffers get allocated from
        {
            m_constantBufferPool = aznew RHI::BufferPool();
            RHI::BufferPoolDescriptor constantBufferPoolDesc;
            constantBufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::Constant;
            constantBufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            constantBufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
            [[maybe_unused]] RHI::ResultCode result = m_constantBufferPool->Init(RHI::MultiDevice::AllDevices, constantBufferPoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized constant buffer pool");
        }

        CreateConstantBufferView();

        // Load the Shader and obtain the Pipeline state and its SRG
        {
            const char* triangeShaderFilePath = "Shaders/RHI/TrianglesConstantBuffer.azshader";
            auto shader = LoadShader(triangeShaderFilePath, s_trianglesConstantBufferExampleName);
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
            AZ_Assert(m_pipelineState, "Failed to acquire default pipeline state for shader '%s'", triangeShaderFilePath);

            m_shaderResourceGroup = CreateShaderResourceGroup(shader, "TriangleSrg", s_trianglesConstantBufferExampleName);
        }

        // Prepare SRG instance
        {
            // Find the constant buffer index inside the SRG
            const Name trianglesBufferId{ "m_trianglesCB" };
            RHI::ShaderInputBufferIndex trianglesBufferIndex;
            FindShaderInputIndex(&trianglesBufferIndex, m_shaderResourceGroup, trianglesBufferId, s_trianglesConstantBufferExampleName);

            [[maybe_unused]] bool set = m_shaderResourceGroup->SetBufferView(trianglesBufferIndex, m_constantBufferView.get(), 0);
            AZ_Assert(set, "Failed to set the buffer view");

            // All SRG data has been set already, it only had the buffer view to be set, we can compile now.
            // Later then only thing to do is to update the Buffer content itself, but the SRG won't need to be recompiled.
            m_shaderResourceGroup->Compile();
        }

        // Creates a scope for rendering the triangle.
        {
            struct ScopeData
            {
            };

            const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                // Binds the swap chain as a color attachment. Clears it to white.
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
                drawIndexed.m_instanceCount = s_numberOfTrianglesTotal;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
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

                // Submit the triangle draw item.
                commandList->Submit(drawItem);
            };

            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    RHI::ScopeId{"TrianglesConstantBuffer"},
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));
        }

        TickBus::Handler::BusConnect();
        RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void TrianglesConstantBufferExampleComponent::CreateConstantBufferView()
    {
        using namespace AZ;
        const uint32_t constantBufferSize = sizeof(InstanceInfo) * s_numberOfTrianglesTotal;

        m_constantBuffer = aznew RHI::Buffer();
        RHI::BufferInitRequest request;
        request.m_buffer = m_constantBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::Constant, constantBufferSize };
        [[maybe_unused]] RHI::ResultCode result = m_constantBufferPool->InitBuffer(request);
        AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize constant buffer");

        RHI::BufferViewDescriptor bufferDesc = RHI::BufferViewDescriptor::CreateStructured(0, 1u, constantBufferSize);
        m_constantBufferView = m_constantBuffer->BuildBufferView(bufferDesc);
    }

    void TrianglesConstantBufferExampleComponent::Deactivate()
    {
        using namespace AZ;

        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_constantBuffer = nullptr;
        m_constantBufferView = nullptr;

        m_constantBuffer = nullptr;
        m_constantBufferPool = nullptr;

        m_pipelineState = nullptr;
        m_shaderResourceGroup = nullptr;

        m_scopeProducers.clear();
        m_windowContext = nullptr;

        TickBus::Handler::BusConnect();
        RHI::RHISystemNotificationBus::Handler::BusDisconnect();
    }
} // namespace AtomSampleViewer
