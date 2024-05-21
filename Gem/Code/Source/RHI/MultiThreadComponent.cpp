/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <RHI/MultiThreadComponent.h>

#include <AzCore/Math/MatrixUtils.h>

#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <AzCore/Math/Random.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>


namespace AtomSampleViewer
{
    // static const variables.
    const AZ::Vector3 MultiThreadComponent::m_up = AZ::Vector3(0.0f, 1.0f, 0.0f);

    void MultiThreadComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiThreadComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    MultiThreadComponent::MultiThreadComponent()
    {
        m_depthStencilID = AZ::RHI::AttachmentId{ "DepthStencilID" };

        uint32_t index = 0;

        // Create positions for each cube
        for (uint32_t j = 0; j < s_cubesPerLine*s_cubeSpacing; j+= s_cubeSpacing)
        {
            for (uint32_t i = 0; i < s_cubesPerLine*s_cubeSpacing; i+= s_cubeSpacing)
            {
                m_cubeTransforms[index] = AZ::Matrix4x4::CreateTranslation(AZ::Vector3(static_cast<float>(i), static_cast<float>(j), 0.0f));
                ++index;
            }
        }

        m_supportRHISamplePipeline = true;
    }

    void MultiThreadComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        m_time += 0.005f;
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void MultiThreadComponent::Activate()
    {
        // This is done here instead of doing it in the constructor, 
        // since m_windowContext might not be yet initialized at construction time. 
        float fieldOfView = AZ::Constants::Pi / 4.0f;
        float screenAspect = GetViewportWidth() / GetViewportHeight();

        float heighOfCubePlane = static_cast<float>(s_cubesPerLine * s_cubeSpacing);
        float distanceFromCubePlane = 1.0f * (1 / tanf(fieldOfView/2)) * heighOfCubePlane/2;

        float centerOfScreen = heighOfCubePlane/2;
        AZ::Vector3 m_worldPosition = AZ::Vector3(centerOfScreen, centerOfScreen, distanceFromCubePlane);
        m_lookAt = AZ::Vector3(centerOfScreen, centerOfScreen, 0.0f);
        MakePerspectiveFovMatrixRH(m_viewProjMatrix, fieldOfView, screenAspect, m_zNear, m_zFar);
        m_viewProjMatrix = m_viewProjMatrix * CreateViewMatrix(m_worldPosition, m_up, m_lookAt);

        CreateInputAssemblyBuffer();
        CreatePipeline();
        CreateScope();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void MultiThreadComponent::Deactivate()
    {
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_bufferPool = nullptr;
        m_inputAssemblyBuffer = nullptr;
        m_shaderResourceGroups.fill(nullptr);
        m_pipelineState = nullptr;
        m_scopeProducers.clear();
    }

    MultiThreadComponent::SingleCubeBufferData MultiThreadComponent::CreateSingleCubeBufferData(const AZ::Vector4 color)
    {
        // Create vertices, colors and normals for a cube and a plane
        SingleCubeBufferData bufferData;
        {
            AZStd::vector<AZ::Vector3> vertices =
            {
                //Front Face
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),     AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(1.0, -1.0, 1.0),
                //Back Face                                                                       
                AZ::Vector3(1.0, 1.0, -1.0),        AZ::Vector3(-1.0, 1.0, -1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(1.0, -1.0, -1.0),
                //Left Face                                                                      
                AZ::Vector3(-1.0, 1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(-1.0, 1.0, -1.0),
                //Right Face                                                                    
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(1.0, -1.0, 1.0),     AZ::Vector3(1.0, -1.0, -1.0),    AZ::Vector3(1.0, 1.0, -1.0),
                //Top Face                                                                  
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),     AZ::Vector3(-1.0, 1.0, -1.0),    AZ::Vector3(1.0, 1.0, -1.0),
                //Bottom Face                                                                    
                AZ::Vector3(1.0, -1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(1.0, -1.0, -1.0),
            };

            for (int i = 0; i < s_geometryVertexCount; ++i)
            {
                SetVertexPosition(bufferData.m_positions.data(), i, vertices[i]);
                SetVertexColor(bufferData.m_colors.data(), i, color);
            }

            bufferData.m_indices =
            {
                {
                    //Back
                    2, 0, 1,
                    0, 2, 3,
                    //Front
                    4, 6, 5,
                    6, 4, 7,
                    //Left
                    8, 10, 9,
                    10, 8, 11,
                    //Right
                    14, 12, 13,
                    15, 12, 14,
                    //Top
                    16, 18, 17,
                    18, 16, 19,
                    //Bottom
                    22, 20, 21,
                    23, 20, 22,
                }
            };
        }
        return bufferData;
    }

    void MultiThreadComponent::CreateInputAssemblyBuffer()
    {       
        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        AZ::RHI::ResultCode result = AZ::RHI::ResultCode::Success;

        m_bufferPool = aznew AZ::RHI::BufferPool();
        AZ::RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        result = m_bufferPool->Init(AZ::RHI::MultiDevice::AllDevices, bufferPoolDesc);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("MultiThreadComponent", false, "Failed to initialize buffer pool with error code %d", result);
            return;
        }

        SingleCubeBufferData bufferData = CreateSingleCubeBufferData(AZ::Vector4(1.0f, 0.0f, 0.0f, 0.0f));

        m_inputAssemblyBuffer = aznew AZ::RHI::Buffer();
        AZ::RHI::BufferInitRequest request;

        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, sizeof(SingleCubeBufferData) };
        request.m_initialData = &bufferData;
        result = m_bufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("MultiThreadComponent", false, "Failed to initialize buffer with error code %d", result);
            return;
        }

        m_streamBufferViews[0] =
        {
            *m_inputAssemblyBuffer,
            offsetof(SingleCubeBufferData, m_positions),
            sizeof(SingleCubeBufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_streamBufferViews[1] =
        {
            *m_inputAssemblyBuffer,
            offsetof(SingleCubeBufferData, m_colors),
            sizeof(SingleCubeBufferData::m_colors),
            sizeof(VertexColor)
        };

        m_indexBufferView =
        {
            *m_inputAssemblyBuffer,
            offsetof(SingleCubeBufferData, m_indices),
            sizeof(SingleCubeBufferData::m_indices),
            AZ::RHI::IndexFormat::Uint16
        };

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.SetTopology(AZ::RHI::PrimitiveTopology::TriangleList);
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("COLOR", AZ::RHI::Format::R32G32B32A32_FLOAT);
        m_streamLayoutDescriptor.Clear();
        m_streamLayoutDescriptor = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_streamLayoutDescriptor, m_streamBufferViews);
    }

    void MultiThreadComponent::CreatePipeline()
    {
        const char* shaderFilePath = "Shaders/RHI/MultiThread.azshader";
        const char* sampleName = "MultiThreadComponent";

        auto shader = LoadShader(shaderFilePath, sampleName);
        if (shader == nullptr)
            return;

        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        AZ::RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_streamLayoutDescriptor;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 1;
        pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_func = AZ::RHI::ComparisonFunc::LessEqual;

        AZ::RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat)
            ->DepthStencilAttachment(device->GetNearestSupportedFormat(AZ::RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil));
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineState)
        {
            AZ_Error("MultiThreadComponent", false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        auto perInstanceSrgLayout = shader->FindShaderResourceGroupLayout(AZ::Name{ "MultiThreadInstanceSrg" });
        if (!perInstanceSrgLayout)
        {
            AZ_Error("MultiThreadComponent", false, "Failed to get shader resource group layout");
            return;
        }

        for (int i = 0; i < s_numberOfCubes; ++i)
        {
            m_shaderResourceGroups[i] = CreateShaderResourceGroup(shader, "MultiThreadInstanceSrg", sampleName);

            FindShaderInputIndex(&m_shaderIndexWorldMat, m_shaderResourceGroups[i], AZ::Name{"m_worldMatrix"}, "MultiThreadComponent");
            FindShaderInputIndex(&m_shaderIndexViewProj, m_shaderResourceGroups[i], AZ::Name{"m_viewProjMatrix"}, "MultiThreadComponent");
        }
    }

    void MultiThreadComponent::CreateScope()
    {
        const auto prepareFunction = [this](AZ::RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment. Clears it to black.
            {
                AZ::RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // Create & Binds DepthStencil image
            {
                const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
                const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                    AZ::RHI::ImageBindFlags::DepthStencil,
                    m_outputWidth,
                    m_outputHeight,
                    device->GetNearestSupportedFormat(AZ::RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil));
                const AZ::RHI::TransientImageDescriptor transientImageDescriptor(m_depthStencilID, imageDescriptor);

                frameGraph.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);

                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_depthStencilID;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = device->GetNearestSupportedFormat(AZ::RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
                dsDesc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateDepthStencil(1.0f, 0);
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::Write);
            }

            // We will submit s_numberOfCubes draw items.
            frameGraph.SetEstimatedItemCount(s_numberOfCubes);
        };

        const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::Matrix4x4 rotation = AZ::Matrix4x4::CreateRotationY(m_time);

            for(int i = 0; i<s_numberOfCubes; ++i)
            {
                AZ::Matrix4x4 transform = m_cubeTransforms[i] * rotation;
                m_shaderResourceGroups[i]->SetConstant(m_shaderIndexWorldMat, transform);
                m_shaderResourceGroups[i]->SetConstant(m_shaderIndexViewProj, m_viewProjMatrix);
                m_shaderResourceGroups[i]->Compile();
            }
        };

        const auto executeFunction = [this](const AZ::RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZ::RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = s_geometryIndexCount;
            drawIndexed.m_instanceCount = 1;

            if (context.GetCommandListIndex() == context.GetCommandListCount() - 1)
            {
#if defined(AZ_DEBUG_BUILD)
                AZ_Printf("MultiThread", "Draw Calls: %d \n", s_numberOfCubes);
                AZ_Printf("MultiThread", "Num CommandLists: %d \n", context.GetCommandListCount());
#endif
            }
            
            for (uint32_t i = context.GetSubmitRange().m_startIndex; i < context.GetSubmitRange().m_endIndex; ++i)
            {
                const AZ::RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                    m_shaderResourceGroups[i]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                };

                AZ::RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                drawItem.m_indexBufferView = &deviceIndexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(AZ::RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                AZStd::array<AZ::RHI::DeviceStreamBufferView, 2> deviceStreamBufferViews{
                    m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()),
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())
                };
                drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

                commandList->Submit(drawItem, i);
            }
        };

        m_scopeProducers.emplace_back(
            aznew AZ::RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId{"MultiThreadMain"},
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }
}// namespace AtomSampleViewer
