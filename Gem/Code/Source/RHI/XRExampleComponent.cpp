/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/XRExampleComponent.h>
#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
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
        if (!AZ::RPI::RPISystemInterface::Get()->GetXRSystem())
        {
            return;
        }

        m_depthStencilID = AZ::RHI::AttachmentId{ AZStd::string::format("DepthStencilID_%llu", GetId()) };
        CreateCubeInputAssemblyBuffer();
        CreateCubePipeline();
        CreateScope();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
        AZ::RPI::XRSpaceNotificationBus::Handler::BusConnect();
    }

    void XRExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_time += deltaTime;
    }

    void XRExampleComponent::OnXRSpaceLocationsChanged(
        const AZ::Transform& baseSpaceToHeadTm, const AZ::Transform& headToLeftEyeTm, const AZ::Transform& headToRightEyeTm)
    {
        AZ::RPI::XRRenderingInterface* xrSystem = AZ::RPI::RPISystemInterface::Get()->GetXRSystem();
        if (!xrSystem || !xrSystem->ShouldRender())
        {
            return;
        }

        m_baseSpaceToHeadTm = baseSpaceToHeadTm;
        const AZ::Transform eyeWorldZupTm = (m_viewIndex == 0)
            ? baseSpaceToHeadTm * headToLeftEyeTm // This is true for both: main pipeline and left eye pipeline.
            : baseSpaceToHeadTm * headToRightEyeTm; // Right eye pipeline.

        // When rendering, the camera Up vector should be Y+, and the forward vector is Z-
        static const AZ::Transform zUpToYUp = AZ::Transform::CreateRotationX(AZ::Constants::HalfPi);
        const auto eyeWorldYupTm = eyeWorldZupTm * zUpToYUp;

        AZ::RPI::FovData fovData;
        [[maybe_unused]] AZ::RHI::ResultCode resultCode = xrSystem->GetViewFov(m_viewIndex, fovData);

        static const float clip_near = 0.05f;
        static const float clip_far = 100.0f;
        bool reverseDepth = false;
        const auto projectionMat44 = xrSystem->CreateStereoscopicProjection(
            fovData.m_angleLeft, fovData.m_angleRight, fovData.m_angleDown, fovData.m_angleUp, clip_near, clip_far, reverseDepth);

        AZ::Matrix4x4 viewMat = AZ::Matrix4x4::CreateFromTransform(eyeWorldYupTm.GetInverse());
        m_viewProjMatrix = projectionMat44 * viewMat;
    }

    void XRExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        AZ::RPI::XRRenderingInterface* xrSystem = AZ::RPI::RPISystemInterface::Get()->GetXRSystem();
        if (xrSystem && xrSystem->ShouldRender())
        {
            const AZ::Matrix4x4 initialScaleMat = AZ::Matrix4x4::CreateScale(AZ::Vector3(0.1f, 0.1f, 0.1f));

            AZ::RPI::PoseData frontViewPoseData;
            //Model matrix for the cube related to the front view
            [[maybe_unused]] AZ::RHI::ResultCode resultCode = xrSystem->GetViewFrontPose(frontViewPoseData);
            m_modelMatrices[0] =
                AZ::Matrix4x4::CreateFromQuaternionAndTranslation(frontViewPoseData.m_orientation, frontViewPoseData.m_position) *
                initialScaleMat;

            AZ::Transform controllerLeftTm;
            resultCode = xrSystem->GetControllerTransform(0, controllerLeftTm);
            m_modelMatrices[1] = AZ::Matrix4x4::CreateFromTransform(m_baseSpaceToHeadTm * controllerLeftTm) * initialScaleMat;

            AZ::Transform controllerRightTm;
            resultCode = xrSystem->GetControllerTransform(1, controllerRightTm);
            m_modelMatrices[2] = AZ::Matrix4x4::CreateFromTransform(m_baseSpaceToHeadTm * controllerRightTm) * initialScaleMat;
        }      
        
        for (int i = 0; i < NumberOfCubes; ++i)
        {
            m_shaderResourceGroups[i]->SetConstant(m_shaderIndexWorldMat, m_modelMatrices[i]);
            m_shaderResourceGroups[i]->SetConstant(m_shaderIndexViewProj, m_viewProjMatrix);
            m_shaderResourceGroups[i]->Compile();
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    XRExampleComponent::SingleCubeBufferData XRExampleComponent::CreateSingleCubeBufferData()
    {
        const AZStd::fixed_vector<AZ::Color, GeometryVertexCount> vertexColor =
        {
            //Top Face
            AZ::Colors::DarkBlue,   AZ::Colors::DarkBlue,   AZ::Colors::DarkBlue,   AZ::Colors::DarkBlue,
            //bottom Face                                                                       
            AZ::Colors::Blue,       AZ::Colors::Blue,       AZ::Colors::Blue,       AZ::Colors::Blue,
            //Left Face                                                                      
            AZ::Colors::DarkGreen,  AZ::Colors::DarkGreen,  AZ::Colors::DarkGreen,  AZ::Colors::DarkGreen,
            //Right Face                                                                    
            AZ::Colors::Green,      AZ::Colors::Green,      AZ::Colors::Green,      AZ::Colors::Green,
            //Front Face                                                                  
            AZ::Colors::DarkRed,    AZ::Colors::DarkRed,    AZ::Colors::DarkRed,    AZ::Colors::DarkRed,
            //Back Face                                                                    
            AZ::Colors::Red,        AZ::Colors::Red,        AZ::Colors::Red,        AZ::Colors::Red,
        };

        // Create vertices, colors and normals for a cube and a plane
        SingleCubeBufferData bufferData;
        {
            
            const AZStd::fixed_vector<AZ::Vector3, GeometryVertexCount> vertices =
            {
                //Top Face
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),     AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(1.0, -1.0, 1.0),
                //Bottom Face                                                                       
                AZ::Vector3(1.0, 1.0, -1.0),        AZ::Vector3(-1.0, 1.0, -1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(1.0, -1.0, -1.0),
                //Left Face                                                                      
                AZ::Vector3(-1.0, 1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(-1.0, 1.0, -1.0),
                //Right Face                                                                    
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(1.0, -1.0, 1.0),     AZ::Vector3(1.0, -1.0, -1.0),    AZ::Vector3(1.0, 1.0, -1.0),
                //Front Face                                                                  
                AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),     AZ::Vector3(-1.0, 1.0, -1.0),    AZ::Vector3(1.0, 1.0, -1.0),
                //Back Face                                                                    
                AZ::Vector3(1.0, -1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),    AZ::Vector3(-1.0, -1.0, -1.0),   AZ::Vector3(1.0, -1.0, -1.0),
            };

            for (int i = 0; i < GeometryVertexCount; ++i)
            {
                SetVertexPosition(bufferData.m_positions.data(), i, vertices[i]);
                SetVertexColor(bufferData.m_colors.data(), i, vertexColor[i].GetAsVector4());
            }

            bufferData.m_indices =
            {
                {
                    //Top
                    2, 0, 1,
                    0, 2, 3,
                    //Bottom
                    4, 6, 5,
                    6, 4, 7,
                    //Left
                    8, 10, 9,
                    10, 8, 11,
                    //Right
                    14, 12, 13,
                    15, 12, 14,
                    //Front
                    16, 18, 17,
                    18, 16, 19,
                    //Back
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

        m_bufferPool = aznew AZ::RHI::BufferPool();
        AZ::RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        result = m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error("XRExampleComponent", false, "Failed to initialize buffer pool with error code %d", result);
            return;
        }

        SingleCubeBufferData bufferData = CreateSingleCubeBufferData();

        m_inputAssemblyBuffer = aznew AZ::RHI::Buffer();
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
            AZ_Error("XRExampleComponent", false, "Failed to acquire default pipeline state for shader '%s'", shaderFilePath);
            return;
        }

        auto perInstanceSrgLayout = shader->FindShaderResourceGroupLayout(AZ::Name{ "OpenXrSrg" });
        if (!perInstanceSrgLayout)
        {
            AZ_Error("XRExampleComponent", false, "Failed to get shader resource group layout");
            return;
        }

        for (int i = 0; i < NumberOfCubes; ++i)
        {
            m_shaderResourceGroups[i] = CreateShaderResourceGroup(shader, "OpenXrSrg", sampleName);
        }

        // Using the first SRG to get the correct index as all the SRGs will have the same indices.
        FindShaderInputIndex(&m_shaderIndexWorldMat, m_shaderResourceGroups[0], AZ::Name{ "m_worldMatrix" }, "XRExampleComponent");
        FindShaderInputIndex(&m_shaderIndexViewProj, m_shaderResourceGroups[0], AZ::Name{ "m_viewProjMatrix" }, "XRExampleComponent");
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
                dsDesc.m_loadStoreAction.m_loadActionStencil = AZ::RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::Write);
            }

            // We will submit NumberOfCubes draw items.
            frameGraph.SetEstimatedItemCount(NumberOfCubes);
        };

        AZ::RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const AZ::RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            AZ::RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            AZ::RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = GeometryIndexCount;
            drawIndexed.m_instanceCount = 1;

            // Dividing NumberOfCubes by context.GetCommandListCount() to balance to number 
            // of draw call equally between each thread.
            uint32_t numberOfCubesPerCommandList = NumberOfCubes / context.GetCommandListCount();
            uint32_t indexStart = context.GetCommandListIndex() * numberOfCubesPerCommandList;
            uint32_t indexEnd = indexStart + numberOfCubesPerCommandList;

            if (context.GetCommandListIndex() == context.GetCommandListCount() - 1)
            {
                indexEnd = NumberOfCubes;
            }

            for (uint32_t i = indexStart; i < indexEnd; ++i)
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

                commandList->Submit(drawItem);
            }
        };

        m_scopeProducers.emplace_back(
            aznew AZ::RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId{ AZStd::string::format("XRSample_Id_%llu", GetId()) },
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void XRExampleComponent::Deactivate()
    {
        if (!AZ::RPI::RPISystemInterface::Get()->GetXRSystem())
        {
            return;
        }

        AZ::RPI::XRSpaceNotificationBus::Handler::BusDisconnect();

        m_inputAssemblyBuffer = nullptr;
        m_bufferPool = nullptr;
        m_pipelineState = nullptr;
        m_shaderResourceGroups.fill(nullptr);

        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }
} 
