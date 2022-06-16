/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/XRExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    //Colors for different faces of the cube
    const AZ::Vector4 Red = AZ::Vector4(1, 0, 0, 0);
    const AZ::Vector4 DarkRed = AZ::Vector4(0.25f, 0, 0, 0);
    const AZ::Vector4 Green = AZ::Vector4(0, 1, 0, 0);
    const AZ::Vector4 DarkGreen = AZ::Vector4(0, 0.25f, 0, 0);
    const AZ::Vector4 Blue = AZ::Vector4(0, 0, 1, 0);
    const AZ::Vector4 DarkBlue = AZ::Vector4(0, 0, 0.25f, 0);

    void XRExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<XRExampleComponent, AZ::Component>()->Version(0);
        }
    }

    XRExampleComponent::XRExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void XRExampleComponent::Activate()
    {
        using namespace AZ;

        CreateCubeInputAssemblyBuffer();
        CreateCubePipeline();
        CreateScope();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void XRExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_time += deltaTime;
    }

    void XRExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        AZ::Matrix4x4 projection = AZ::Matrix4x4::CreateIdentity();
        AZ::Matrix4x4 vpMat = AZ::Matrix4x4::CreateIdentity();

        AZ::RPI::XRRenderingInterface* xrSystem = AZ::RPI::RPISystemInterface::Get()->GetXRSystem();
        if (xrSystem && xrSystem->ShouldRender())
        {
            AZ::RPI::FovData fovData = xrSystem->GetViewFov(m_viewIndex);
            AZ::RPI::PoseData poseData = xrSystem->GetViewPose(m_viewIndex);
                
            static const float clip_near = 0.05f;
            static const float clip_far = 100.0f;
            projection = xrSystem->CreateProjectionOffset(fovData.m_angleLeft, fovData.m_angleRight, 
                                                          fovData.m_angleDown, fovData.m_angleUp, 
                                                          clip_near, clip_far);

            AZ::Matrix4x4 viewMat = AZ::Matrix4x4::CreateFromQuaternionAndTranslation(poseData.orientation, poseData.position);;
            viewMat.InvertTransform();
            m_viewProjMatrix = projection * viewMat;
 
            AZ::Matrix4x4 initialScaleMat = AZ::Matrix4x4::CreateScale(AZ::Vector3(0.1f, 0.1f, 0.1f));

            //Model matrix for the cube related to the front view
            AZ::RPI::PoseData frontViewPoseData = xrSystem->GetViewFrontPose();
            m_modelMatrices[0] = AZ::Matrix4x4::CreateFromQuaternionAndTranslation(frontViewPoseData.orientation, frontViewPoseData.position) * initialScaleMat;
                      
            //Model matrix for the cube related to the left controller
            AZ::RPI::PoseData controllerLeftPose = xrSystem->GetControllerPose(0);
            AZ::Matrix4x4 leftScaleMat = initialScaleMat * AZ::Matrix4x4::CreateScale(AZ::Vector3(xrSystem->GetControllerScale(0), xrSystem->GetControllerScale(0), xrSystem->GetControllerScale(0)));
            m_modelMatrices[1] = AZ::Matrix4x4::CreateFromQuaternionAndTranslation(controllerLeftPose.orientation, controllerLeftPose.position) * leftScaleMat;

            //Model matrix for the cube related to the right controller
            AZ::Matrix4x4 rightScaleMat = initialScaleMat * AZ::Matrix4x4::CreateScale(AZ::Vector3(xrSystem->GetControllerScale(1), xrSystem->GetControllerScale(1), xrSystem->GetControllerScale(1)));
            AZ::RPI::PoseData controllerRightPose = xrSystem->GetControllerPose(1);
            m_modelMatrices[2] = AZ::Matrix4x4::CreateFromQuaternionAndTranslation(controllerRightPose.orientation, controllerRightPose.position) * rightScaleMat;
        }      
        
        for (int i = 0; i < s_numberOfCubes; ++i)
        {
            m_shaderResourceGroups[i]->SetConstant(m_shaderIndexWorldMat, m_modelMatrices[i]);
            m_shaderResourceGroups[i]->SetConstant(m_shaderIndexViewProj, m_viewProjMatrix);
            m_shaderResourceGroups[i]->Compile();
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    XRExampleComponent::SingleCubeBufferData XRExampleComponent::CreateSingleCubeBufferData([[maybe_unused]] const AZ::Vector4 color)
    {
        AZStd::vector<AZ::Vector4> vertexColor =
        {
            //Front Face
            DarkBlue, DarkBlue, DarkBlue, DarkBlue,
            //Back Face                                                                       
            Blue, Blue, Blue, Blue,
            //Left Face                                                                      
            DarkGreen, DarkGreen, DarkGreen, DarkGreen,
            //Right Face                                                                    
            Green, Green, Green, Green,
            //Top Face                                                                  
            DarkRed, DarkRed, DarkRed, DarkRed,
            //Bottom Face                                                                    
            Red, Red, Red, Red,
        };

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
                SetVertexColor(bufferData.m_colors.data(), i, vertexColor[i]);
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

    void XRExampleComponent::CreateCubeInputAssemblyBuffer()
    {
        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        AZ::RHI::ResultCode result = AZ::RHI::ResultCode::Success;

        m_bufferPool = AZ::RHI::Factory::Get().CreateBufferPool();
        AZ::RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        result = m_bufferPool->Init(*device, bufferPoolDesc);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("XRExampleComponent", false, "Failed to initialize buffer pool with error code %d", result);
            return;
        }

        SingleCubeBufferData bufferData = CreateSingleCubeBufferData(AZ::Vector4(1.0f, 0.0f, 0.0f, 0.0f));

        m_inputAssemblyBuffer = AZ::RHI::Factory::Get().CreateBuffer();
        AZ::RHI::BufferInitRequest request;

        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, sizeof(SingleCubeBufferData) };
        request.m_initialData = &bufferData;
        result = m_bufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("XRExampleComponent", false, "Failed to initialize buffer with error code %d", result);
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

    void XRExampleComponent::CreateCubePipeline()
    {
        const char* shaderFilePath = "Shaders/RHI/OpenXrSample.azshader";
        const char* sampleName = "XRExampleComponent";

        auto shader = LoadShader(shaderFilePath, sampleName);
        if (shader == nullptr)
        { 
            return;
        }

        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        AZ::RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_streamLayoutDescriptor;
        
        AZ::RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat);
            
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_pipelineState = shader->AcquirePipelineState(pipelineDesc);
        if (!m_pipelineState)
        {
            AZ_Error("XRExampleComponent", false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        auto perInstanceSrgLayout = shader->FindShaderResourceGroupLayout(AZ::Name{ "OpenXrSrg" });
        if (!perInstanceSrgLayout)
        {
            AZ_Error("XRExampleComponent", false, "Failed to get shader resource group layout");
            return;
        }

        for (int i = 0; i < s_numberOfCubes; ++i)
        {
            m_shaderResourceGroups[i] = CreateShaderResourceGroup(shader, "OpenXrSrg", sampleName);

            FindShaderInputIndex(&m_shaderIndexWorldMat, m_shaderResourceGroups[i], AZ::Name{ "m_worldMatrix" }, "XRExampleComponent");
            FindShaderInputIndex(&m_shaderIndexViewProj, m_shaderResourceGroups[i], AZ::Name{ "m_viewProjMatrix" }, "XRExampleComponent");
        }
    }

    void XRExampleComponent::CreateScope()
    {
        // Creates a scope for rendering the triangle.
        struct ScopeData
        {

        };
        const auto prepareFunction = [this](AZ::RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment. Clears it to black.
            {
                AZ::RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            // We will submit s_numberOfCubes draw items.
            frameGraph.SetEstimatedItemCount(s_numberOfCubes);
        };

        AZ::RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const AZ::RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZ::RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = s_geometryIndexCount;
            drawIndexed.m_instanceCount = 1;

            // Dividing s_numberOfCubes by context.GetCommandListCount() to balance to number 
            // of draw call equally between each thread.
            uint32_t numberOfCubesPerCommandList = s_numberOfCubes / context.GetCommandListCount();
            uint32_t indexStart = context.GetCommandListIndex() * numberOfCubesPerCommandList;
            uint32_t indexEnd = indexStart + numberOfCubesPerCommandList;

            if (context.GetCommandListIndex() == context.GetCommandListCount() - 1)
            {
                indexEnd = s_numberOfCubes;
            }

            for (uint32_t i = indexStart; i < indexEnd; ++i)
            {
                const AZ::RHI::ShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[i]->GetRHIShaderResourceGroup() };

                AZ::RHI::DrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_pipelineState.get();
                drawItem.m_indexBufferView = &m_indexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(AZ::RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
                drawItem.m_streamBufferViews = m_streamBufferViews.data();

                commandList->Submit(drawItem);
            }
        };

        m_scopeProducers.emplace_back(
            aznew AZ::RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId{ AZStd::string::format("XRSample_Id_ % u", GetId()) },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void XRExampleComponent::Deactivate()
    {
        m_inputAssemblyBuffer = nullptr;
        m_bufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroups.fill(nullptr);

        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }
} 
