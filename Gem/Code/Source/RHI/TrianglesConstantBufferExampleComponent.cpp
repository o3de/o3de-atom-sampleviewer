/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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

#include <TriangleConstantBufferExampleComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    const char* TrianglesConstantBufferExampleComponent::s_trianglesConstantBufferExampleName = "TrianglesConstantBufferExample";
    // The number of triangles that are represented with a single constant buffer, that contains multiple buffer views;
    // each view containing a partial view of the constant buffer
    const uint32_t TrianglesConstantBufferExampleComponent::s_numberOfTrianglesSingleCB = ATOMSAMPLEVIEWER_TRAIT_TRIANGLE_CONSTANT_BUFFER_SAMPLE_SINGLE_CONSTANT_BUFFER_SIZE;
    // The number of triangles that are represented with multiple constant buffers;
    // each having their own constant buffer and view containing the whole buffer
    const uint32_t TrianglesConstantBufferExampleComponent::s_numberOfTrianglesMultipleCB = ATOMSAMPLEVIEWER_TRAIT_TRIANGLE_CONSTANT_BUFFER_SAMPLE_MULTIPLE_CONSTANT_BUFFER_SIZE;
    // The total number of views and triangles that will be rendered for this sample
    const uint32_t TrianglesConstantBufferExampleComponent::s_numberOfTrianglesTotal = s_numberOfTrianglesSingleCB + s_numberOfTrianglesMultipleCB;

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

        // Calculate the alignment
        const uint32_t alignment = RHI::AlignUp(static_cast<uint32_t>(sizeof(InstanceInfo)), m_constantBufferAlighment);

        // All triangles data will be uploaded in one go to the constant buffer once per frame.
        UploadDataToSingleConstantBuffer(trianglesData, alignment, s_numberOfTrianglesSingleCB);
        UploadDataToMultipleConstantBuffers(trianglesData + s_numberOfTrianglesSingleCB, alignment);
        
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void TrianglesConstantBufferExampleComponent::UploadDataToSingleConstantBuffer(InstanceInfo* data, uint32_t elementSize, uint32_t elementCount)
    {
        AZ::RHI::BufferMapRequest mapRequest;
        mapRequest.m_buffer = m_constantBuffer.get();
        mapRequest.m_byteCount = elementSize;

        // NOTE: Due to the Constant Buffer alignment not being handled internally by the RHI, updating the Constant Buffer
        // that is bound to an alignment needs to be handled by the user.
        // Update the instance data of the triangles that are mapped to a single Constant Buffer
        for (uint32_t triangleIdx = 0u; triangleIdx < elementCount; ++triangleIdx)
        {
            mapRequest.m_byteOffset = triangleIdx * elementSize;

            AZ::RHI::BufferMapResponse mapResponse;
            AZ::RHI::ResultCode resultCode = m_constantBufferPool->MapBuffer(mapRequest, mapResponse);
            if (mapResponse.m_data)
            {
                memcpy(mapResponse.m_data, data + triangleIdx, sizeof(InstanceInfo));
                m_constantBufferPool->UnmapBuffer(*mapRequest.m_buffer);
            }
        }
    }

    void TrianglesConstantBufferExampleComponent::UploadDataToMultipleConstantBuffers(InstanceInfo* data, uint32_t elementSize)
    {
        // Update the instance data of the triangles that are mapped to their individual Constant Buffer
        for (uint32_t triangleIdx = 0u; triangleIdx < (uint32_t)m_constantBufferArray.size(); ++triangleIdx)
        {
            AZ::RHI::BufferMapRequest mapRequest;
            mapRequest.m_buffer = m_constantBufferArray[triangleIdx].get();
            mapRequest.m_byteCount = elementSize;
            mapRequest.m_byteOffset = 0u;

            AZ::RHI::BufferMapResponse mapResponse;
            AZ::RHI::ResultCode resultCode = m_constantBufferPool->MapBuffer(mapRequest, mapResponse);

            if(mapResponse.m_data)
            {
                memcpy(mapResponse.m_data, data + triangleIdx, sizeof(InstanceInfo));
                m_constantBufferPool->UnmapBuffer(*mapRequest.m_buffer);
            }
        }
    }

    void TrianglesConstantBufferExampleComponent::Activate()
    {
        using namespace AZ;

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        // Cache the alignment
        m_constantBufferAlighment = device->GetLimits().m_minConstantBufferViewOffset;

        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

        // Creates Input Assembly buffer and Streams/Index Views
        {
            m_inputAssemblyBufferPool = RHI::Factory::Get().CreateBufferPool();

            RHI::BufferPoolDescriptor bufferPoolDesc;
            bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
            bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            m_inputAssemblyBufferPool->Init(*device, bufferPoolDesc);

            IABufferData bufferData;

            SetVertexPosition(bufferData.m_positions.data(), 0,  0.0f,  0.1f, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 1, -0.1f, -0.1f, 0.0);
            SetVertexPosition(bufferData.m_positions.data(), 2,  0.1f, -0.1f, 0.0);

            SetVertexColor(bufferData.m_colors.data(), 0, 1.0, 0.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 1, 0.0, 1.0, 0.0, 1.0);
            SetVertexColor(bufferData.m_colors.data(), 2, 0.0, 0.0, 1.0, 1.0);

            SetVertexIndexIncreasing(bufferData.m_indices.data(), bufferData.m_indices.size());

            m_inputAssemblyBuffer = RHI::Factory::Get().CreateBuffer();

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
            m_constantBufferPool = RHI::Factory::Get().CreateBufferPool();
            RHI::BufferPoolDescriptor constantBufferPoolDesc;
            constantBufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::Constant;
            constantBufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
            constantBufferPoolDesc.m_hostMemoryAccess = RHI::HostMemoryAccess::Write;
            RHI::ResultCode result = m_constantBufferPool->Init(*device, constantBufferPoolDesc);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized constant buffer pool");
        }

        // Calculate the total constant buffer size depending on the device alignment limit
        const uint32_t alignedBufferSize = RHI::AlignUp(static_cast<uint32_t>(sizeof(InstanceInfo)), m_constantBufferAlighment);

        // NOTE: Alignment needs to be taken into account when creating a Constant Buffer which consists of Buffer Views
        // that have an offset that isn't 0
        // Create single Constant Buffer with multiple Buffer Views
        {
            const uint32_t constantBufferSize = alignedBufferSize * s_numberOfTrianglesSingleCB;

            m_constantBuffer = RHI::Factory::Get().CreateBuffer();
            RHI::BufferInitRequest request;
            request.m_buffer = m_constantBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::Constant, constantBufferSize };
            RHI::ResultCode result = m_constantBufferPool->InitBuffer(request);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialize constant buffer");

            for (uint32_t triangleIdx = 0u; triangleIdx < s_numberOfTrianglesSingleCB; ++triangleIdx)
            {
                RHI::BufferViewDescriptor bufferDesc = RHI::BufferViewDescriptor::CreateStructured(triangleIdx, 1u, alignedBufferSize);
                AZ::RHI::Ptr<AZ::RHI::BufferView> constantBufferView = m_constantBuffer->GetBufferView(bufferDesc);
                          
                if(!constantBufferView.get())
                {
                    AZ_Assert(false, "Failed to initialized constant buffer view");
                }
                m_constantBufferViewArray.push_back(constantBufferView);
            }
        }

        // Create multiple Constant Buffers, each containing one Buffer View
        {
            RHI::ResultCode result;
            m_constantBufferArray.reserve(s_numberOfTrianglesMultipleCB);

            for (uint32_t triangleIdx = 0u; triangleIdx < s_numberOfTrianglesMultipleCB; ++triangleIdx)
            {
                AZ::RHI::Ptr<AZ::RHI::Buffer> constantBuffer = RHI::Factory::Get().CreateBuffer();
                RHI::BufferInitRequest request;
                request.m_buffer = constantBuffer.get();
                request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::Constant, alignedBufferSize};
                result = m_constantBufferPool->InitBuffer(request);
                AZ_Assert(result == RHI::ResultCode::Success, "Failed to initialized constant buffer");

                RHI::BufferViewDescriptor bufferDesc = RHI::BufferViewDescriptor::CreateStructured(0u, 1u, alignedBufferSize);
                AZ::RHI::Ptr<AZ::RHI::BufferView> constantBufferView = constantBuffer->GetBufferView(bufferDesc);
                          
                if(!constantBufferView.get())
                {
                    AZ_Assert(false, "Failed to initialized constant buffer view");
                }
                AZ_Assert(constantBufferView->IsFullView(), "Constant Buffer View initialization failed to cover in full the Constant Buffer");

                m_constantBufferViewArray.push_back(constantBufferView);
                m_constantBufferArray.push_back(constantBuffer);
            }
        }

        // Load the Shader and obtain the Pipeline state and its SRG
        {
            const char* triangeShaderFilePath = ATOMSAMPLEVIEWER_TRAIT_TRIANGLE_CONSTANT_BUFFER_SAMPLE_SHADER_NAME;
            auto shader = LoadShader(triangeShaderFilePath, s_trianglesConstantBufferExampleName);
            if (shader == nullptr)
                return;

            auto shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
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

            uint32_t viewIdx = 0u;
            for (const AZ::RHI::Ptr<AZ::RHI::BufferView>& bufferView : m_constantBufferViewArray)
            {
                bool set = m_shaderResourceGroup->SetBufferView(trianglesBufferIndex, bufferView.get(), viewIdx);
                AZ_Assert(set, "Failed to set the buffer view");
                viewIdx++;
            }

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

                const RHI::ShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroup->GetRHIShaderResourceGroup() };

                RHI::DrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState.get();
                drawItem.m_indexBufferView = &m_indexBufferView;
                drawItem.m_shaderResourceGroupCount = RHI::ArraySize(shaderResourceGroups);
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint32_t>(m_streamBufferViews.size());
                drawItem.m_streamBufferViews = m_streamBufferViews.data();

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

    void TrianglesConstantBufferExampleComponent::Deactivate()
    {
        using namespace AZ;

        m_inputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;

        m_constantBuffer = nullptr;
        m_constantBufferArray.clear();
        m_constantBufferViewArray.clear();

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
