/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/Image.h>
#include <Atom/RHI/ImagePool.h>
#include <Atom/RHI/ScopeProducerFunction.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>

#include <AzCore/Math/MatrixUtils.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/ViewProviderBus.h>

#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>

#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/AsyncComputeExampleComponent.h>
#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace AsyncCompute
    {
        static const char* sampleName = "AsyncComputeComponent";
        static constexpr uint32_t s_shadowMapSize = 1024;
        static constexpr uint32_t s_luminanceMapSize = 1024;
    }

    void AsyncComputeExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AsyncComputeExampleComponent, AZ::Component>()->Version(0);
        }
    }

    AsyncComputeExampleComponent::AsyncComputeExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void AsyncComputeExampleComponent::FrameBeginInternal(RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        if (!m_fullyActivated)
        {
            return;
        }

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        // Import non transient images
        for (uint32_t i = 0; i < m_sceneImages.size(); ++i)
        {
            frameGraphBuilder.GetAttachmentDatabase().ImportImage(m_sceneIds[i], m_sceneImages[i]);
        }

        // Generate transient images
        {
            const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                RHI::ImageBindFlags::DepthStencil | RHI::ImageBindFlags::ShaderRead,
                AsyncCompute::s_shadowMapSize,
                AsyncCompute::s_shadowMapSize,
                RHI::Format::D32_FLOAT);

            frameGraphBuilder.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(m_shadowAttachmentId, imageDescriptor));
        }

        {
            RHI::Format depthStencilFormat = device->GetNearestSupportedFormat(RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
            const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                RHI::ImageBindFlags::DepthStencil, m_outputWidth, m_outputHeight, depthStencilFormat);
            const RHI::TransientImageDescriptor transientImageDescriptor(m_forwardDepthStencilId, imageDescriptor);

            frameGraphBuilder.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);
        }

        {
            const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead,
                AsyncCompute::s_luminanceMapSize,
                AsyncCompute::s_luminanceMapSize,
                RHI::Format::R32_FLOAT);

            frameGraphBuilder.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(m_luminanceMapAttachmentId, imageDescriptor));
        }

        // Swap scene image index
        AZStd::swap(m_currentSceneImageIndex, m_previousSceneImageIndex); 
    }

    void AsyncComputeExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);

        if (!m_fullyActivated)
        {
            return;
        }

        if (m_imguiSidebar.Begin())
        {
            ScriptableImGui::Checkbox("Enable/Disable Async Compute", &m_asyncComputeEnabled);
            m_imguiSidebar.End();
        }
    }

    void AsyncComputeExampleComponent::ResetCamera()
    {
        const float pitch = -AZ::Constants::QuarterPi / 2.0f;
        const float distance = 35.0f;
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetCenter, AZ::Vector3(0.f));
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, distance);
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetMaxDistance, 50.0f);
        AZ::Debug::ArcBallControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::ArcBallControllerRequestBus::Events::SetPitch, pitch);

        // Set the camera Transform so we don't have a jump on the first frame.
        AZ::Quaternion orientation = AZ::Quaternion::CreateRotationX(pitch);
        AZ::Vector3 position = orientation.TransformVector(AZ::Vector3(0, -distance, 0));
        AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformBus::Events::SetWorldTM, AZ::Transform::CreateFromQuaternionAndTranslation(orientation, position));
    }

    void AsyncComputeExampleComponent::CreateSceneRenderTargets()
    {
        m_imagePool = aznew RHI::ImagePool();
        RHI::ImagePoolDescriptor imagePoolDesc;
        imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite;
        m_imagePool->Init(RHI::MultiDevice::AllDevices, imagePoolDesc);

        for (auto& image : m_sceneImages)
        {
            image = aznew RHI::Image();
            RHI::ImageInitRequest initImageRequest;
            RHI::ClearValue clearValue = RHI::ClearValue::CreateVector4Float(0, 0, 0, 0);
            initImageRequest.m_image = image.get();
            initImageRequest.m_descriptor = RHI::ImageDescriptor::Create2D(
                RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderReadWrite,
                m_outputWidth,
                m_outputHeight,
                RHI::Format::R16G16B16A16_FLOAT);
            initImageRequest.m_optimizedClearValue = &clearValue;
            m_imagePool->InitImage(initImageRequest);
        }        
    }

    void AsyncComputeExampleComponent::CreateQuad()
    {
        m_quadBufferPool = aznew RHI::BufferPool();
        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_quadBufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<VertexNormal, 4> m_normals;
            AZStd::array<uint16_t, 6> m_indices;
        };

        BufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());
        bufferData.m_normals.fill({ { 0, 0, 1.0f } });
        for (auto& uv : bufferData.m_uvs)
        {
            uv.m_uv[1] = 1.0f - uv.m_uv[1];
        }

        m_quadInputAssemblyBuffer = aznew RHI::Buffer();
        RHI::ResultCode result = RHI::ResultCode::Success;
        RHI::BufferInitRequest request;

        request.m_buffer = m_quadInputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        result = m_quadBufferPool->InitBuffer(request);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Error("AsyncComputeComponent", false, "Failed to initialize buffer with error code %d", result);
            return;
        }

        AZ::RHI::StreamBufferView positionsBufferView =
        {
            *m_quadInputAssemblyBuffer,
            offsetof(BufferData, m_positions),
            sizeof(BufferData::m_positions),
            sizeof(VertexPosition)
        };

        AZ::RHI::StreamBufferView normalsBufferView =
        {
            *m_quadInputAssemblyBuffer,
            offsetof(BufferData, m_normals),
            sizeof(BufferData::m_normals),
            sizeof(VertexNormal)
        };

        AZ::RHI::StreamBufferView uvsBufferView =
        {
            *m_quadInputAssemblyBuffer,
            offsetof(BufferData, m_uvs),
            sizeof(BufferData::m_uvs),
            sizeof(VertexUV)
        };

        m_quadStreamBufferViews[ShadowScope].push_back(positionsBufferView);
        m_quadStreamBufferViews[ForwardScope].push_back(positionsBufferView);
        m_quadStreamBufferViews[ForwardScope].push_back(normalsBufferView);
        m_quadStreamBufferViews[CopyTextureScope].push_back(positionsBufferView);
        m_quadStreamBufferViews[CopyTextureScope].push_back(uvsBufferView);
        m_quadStreamBufferViews[LuminanceMapScope] = m_quadStreamBufferViews[CopyTextureScope];

        m_quadIndexBufferView =
        {
            *m_quadInputAssemblyBuffer,
            offsetof(BufferData, m_indices),
            sizeof(BufferData::m_indices),
            RHI::IndexFormat::Uint16
        };
    }

    void AsyncComputeExampleComponent::LoadShaders()
    {
        AZStd::vector<const char*> shaders = {
            "Shaders/RHI/CopyQueue.azshader",                    // Vertex + Fragment
            "Shaders/RHI/MultipleViewsDepth.azshader",           // Vertex
            "Shaders/RHI/AsyncComputeShadow.azshader",           // Vertex + Fragment
            "Shaders/RHI/AsyncComputeLuminanceMap.azshader",     // Vertex + Fragment
            "Shaders/RHI/AsyncComputeLuminanceReduce.azshader",  // Compute
            "Shaders/RHI/AsyncComputeTonemapping.azshader"};     // Compute

        for (size_t i = 0; i < shaders.size(); ++i)
        {
            auto shader = LoadShader(*m_assetLoadManager.get(), shaders[i], AsyncCompute::sampleName);
            if (shader == nullptr)
                return;

            m_shaders[i] = shader;

            if (shader->GetPipelineStateType() == AZ::RHI::PipelineStateType::Dispatch)
            {
                const auto& numThreads = shader->GetAsset()->GetAttribute(RHI::ShaderStage::Compute, Name("numthreads"));
                if (numThreads)
                {
                    const RHI::ShaderStageAttributeArguments& args = *numThreads;
                    m_numThreads[i].m_X = args[0].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[0]) : m_numThreads[i].m_X;
                    m_numThreads[i].m_Y = args[1].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[1]) : m_numThreads[i].m_Y;
                    m_numThreads[i].m_Z = args[2].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[2]) : m_numThreads[i].m_Z;
                }
                else
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Did not find expected numthreads attribute");
                }
            }
        }       
    }

    void AsyncComputeExampleComponent::LoadModel()
    {
        // Load the asset
        auto modelAsset = m_assetLoadManager->GetAsset<AZ::RPI::ModelAsset>("objects/shaderball_simple.fbx.azmodel");
        AZ_Assert(modelAsset.IsReady(), "The model asset is supposed to be ready.");

        m_model = AZ::RPI::Model::FindOrCreate(modelAsset);
        AZ_Error(AsyncCompute::sampleName, m_model, "Failed to load model");  
    }

    void AsyncComputeExampleComponent::CreatePipelines()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        {
            // Shadow Scope Pipelines
            const auto& shader = m_shaders[ShadowScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 1;
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_func = RHI::ComparisonFunc::LessEqual;

            attachmentsBuilder.AddSubpass()
                ->DepthStencilAttachment(RHI::Format::D32_FLOAT);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            {
                // Terrain Pipeline
                RHI::InputStreamLayoutBuilder layoutBuilder;
                layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
                pipelineDesc.m_inputStreamLayout = layoutBuilder.End();

                if (!RHI::ValidateStreamBufferViews(
                        pipelineDesc.m_inputStreamLayout, 
                        m_quadStreamBufferViews[ShadowScope]))
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Invalid stream buffer views for terrain");
                    return;
                }

                m_terrainPipelineStates[ShadowScope] = shader->AcquirePipelineState(pipelineDesc);
                if (!m_terrainPipelineStates[ShadowScope])
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for shadow shader");
                    return;
                }
            }

            {
                // Model Pipeline
                Data::Instance<AZ::RPI::ModelLod> modelLod = m_model->GetLods()[0];
                modelLod->GetStreamsForMesh(
                    pipelineDesc.m_inputStreamLayout,
                    m_modelStreamBufferViews[ShadowScope],
                    nullptr,
                    shader->GetInputContract(),
                    0);

                m_modelPipelineStates[ShadowScope] = shader->AcquirePipelineState(pipelineDesc);
                if (!m_modelPipelineStates[ShadowScope])
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for shader");
                    return;
                }
            }
        }

        {
            // Forward Scope Pipelines
            const auto& shader = m_shaders[ForwardScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);

            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState = RHI::DepthStencilState::CreateReverseDepth();

            attachmentsBuilder.Reset();
            RHI::Format depthStencilFormat = device->GetNearestSupportedFormat(RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(RHI::Format::R16G16B16A16_FLOAT)
                ->DepthStencilAttachment(depthStencilFormat);

            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            {
                // Terrain Pipeline
                RHI::InputStreamLayoutBuilder layoutBuilder;
                layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
                layoutBuilder.AddBuffer()->Channel("NORMAL", RHI::Format::R32G32B32_FLOAT);
                pipelineDesc.m_inputStreamLayout = layoutBuilder.End();

                if (!RHI::ValidateStreamBufferViews(
                    pipelineDesc.m_inputStreamLayout, 
                    m_quadStreamBufferViews[ForwardScope]))
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Invalid stream buffer views for terrain");
                    return;
                }

                m_terrainPipelineStates[ForwardScope] = shader->AcquirePipelineState(pipelineDesc);
                if (!m_terrainPipelineStates[ForwardScope])
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for shadow shader");
                    return;
                }
            }

            {
                // Model Pipeline
                Data::Instance<AZ::RPI::ModelLod> modelLod = m_model->GetLods()[0];
                modelLod->GetStreamsForMesh(
                    pipelineDesc.m_inputStreamLayout,
                    m_modelStreamBufferViews[ForwardScope],
                    nullptr,
                    shader->GetInputContract(),
                    0);

                m_modelPipelineStates[ForwardScope] = shader->AcquirePipelineState(pipelineDesc);
                if (!m_modelPipelineStates[ForwardScope])
                {
                    AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for shader");
                    return;
                }
            }           
        }

        {
            // Copy texture Pipeline
            const auto& shader = m_shaders[CopyTextureScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 0;

            attachmentsBuilder.Reset();
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            RHI::TargetBlendState& blendstate = pipelineDesc.m_renderStates.m_blendState.m_targets[0];
            blendstate.m_enable = true;
            blendstate.m_blendDest = RHI::BlendFactor::AlphaSourceInverse;
            blendstate.m_blendSource = RHI::BlendFactor::AlphaSource;
            blendstate.m_blendOp = RHI::BlendOp::Add;

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineDesc.m_inputStreamLayout = layoutBuilder.End();

            if (!RHI::ValidateStreamBufferViews(
                pipelineDesc.m_inputStreamLayout,
                m_quadStreamBufferViews[CopyTextureScope]))
            {
                AZ_Error(AsyncCompute::sampleName, false, "Invalid stream buffer views for LuminanceMap");
                return;
            }

            m_copyTexturePipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_copyTexturePipelineState)
            {
                AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for copy texture");
                return;
            }
        }

        {
            // LuminanceMap pipeline
            const auto& shader = m_shaders[LuminanceMapScope];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);
            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState.m_depth.m_enable = 0;

            attachmentsBuilder.Reset();
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(RHI::Format::R32_FLOAT);
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
            pipelineDesc.m_inputStreamLayout = layoutBuilder.End();

            if (!RHI::ValidateStreamBufferViews(
                pipelineDesc.m_inputStreamLayout,
                m_quadStreamBufferViews[LuminanceMapScope]))
            {
                AZ_Error(AsyncCompute::sampleName, false, "Invalid stream buffer views for LuminanceMap");
                return;
            }

            m_luminancePipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_luminancePipelineState)
            {
                AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for luminance map");
                return;
            }
        }

        {
            // Luminance reduce pipelines
            const auto& shader = m_shaders[LuminanceReduceScope];
            RHI::PipelineStateDescriptorForDispatch pipelineDesc;
            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

            m_luminanceReducePipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_luminanceReducePipelineState)
            {
                AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for luminance reduce");
                return;
            }
        }

        {
            // Tonemapping pipeline
            const auto& shader = m_shaders[TonemappingScope];
            RHI::PipelineStateDescriptorForDispatch pipelineDesc;
            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

            m_tonemappingPipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_tonemappingPipelineState)
            {
                AZ_Error(AsyncCompute::sampleName, false, "Failed to acquire default pipeline state for tonemapping");
                return;
            }
        }
    }

    void AsyncComputeExampleComponent::Activate()
    {
        m_assetLoadManager = AZStd::make_unique<AZ::AssetCollectionAsyncLoader>();
        
        m_forwardDepthStencilId = AZ::Name("ForwardDepthStencilId");
        m_shadowAttachmentId = AZ::Name("ShadowAttachmentId");
        m_luminanceMapAttachmentId = AZ::Name("LuminanceMapAttachmentId");
        m_averageLuminanceAttachmentId = AZ::Name("LuminanceReduce1");

        // List of all assets this example needs.
        AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo> assetList = {
            {"Shaders/RHI/CopyQueue.azshader", azrtti_typeid<RPI::ShaderAsset>()},                    // Vertex + Fragment
            {"Shaders/RHI/MultipleViewsDepth.azshader", azrtti_typeid<RPI::ShaderAsset>()},           // Vertex
            {"Shaders/RHI/AsyncComputeShadow.azshader", azrtti_typeid<RPI::ShaderAsset>()},           // Vertex + Fragment
            {"Shaders/RHI/AsyncComputeLuminanceMap.azshader", azrtti_typeid<RPI::ShaderAsset>()},     // Vertex + Fragment
            {"Shaders/RHI/AsyncComputeLuminanceReduce.azshader", azrtti_typeid<RPI::ShaderAsset>()},  // Compute
            {"Shaders/RHI/AsyncComputeTonemapping.azshader", azrtti_typeid<RPI::ShaderAsset>()},    // Compute
            {"objects/shaderball_simple.fbx.azmodel", azrtti_typeid<AZ::RPI::ModelAsset>()}, // The model
        };


        // Configure the imgui progress list widget.
        auto onUserCancelledAction = [&]()
        {
            AZ_TracePrintf(AsyncCompute::sampleName, "Cancelled by user.\n");
            m_assetLoadManager->Cancel();
            SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::Reset);
        };
        m_imguiProgressList.OpenPopup("Waiting For Assets...", "Assets pending for processing:", {}, onUserCancelledAction, true /*automaticallyCloseOnAction*/, "Cancel");
        AZStd::for_each(assetList.begin(), assetList.end(),
            [&](const AssetCollectionAsyncLoader::AssetToLoadInfo& item) { m_imguiProgressList.AddItem(item.m_assetPath); });


        // Kickoff asynchronous asset loading, the activation will continue once all assets are available.
        m_assetLoadManager->LoadAssetsAsync(assetList, [&](AZStd::string_view assetName, [[maybe_unused]] bool success, size_t pendingAssetCount)
            {
                if (m_fullyActivated)
                {
                    return;
                }
                AZ_Error(AsyncCompute::sampleName, success, "Error loading asset %s, a crash will occur when OnAllAssetsReadyActivate() is called!", assetName.data());
                AZ_TracePrintf(AsyncCompute::sampleName, "Asset %s loaded %s. Wait for %zu more assets before full activation\n", assetName.data(), success ? "successfully" : "UNSUCCESSFULLY", pendingAssetCount);
                m_imguiProgressList.RemoveItem(assetName);
                if (!pendingAssetCount)
                {
                    OnAllAssetsReadyActivate();
                }
            });

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);
    }

    void AsyncComputeExampleComponent::OnAllAssetsReadyActivate()
    {
        AZ_Assert(!m_fullyActivated, "Full Activation should occur only once");

        CreateSceneRenderTargets();
        CreateQuad();
        LoadModel();
        LoadShaders();
        CreatePipelines();
        SetupScene();
        SetArcBallControllerParams();

        CreateLuminanceMapScope();
        CreateShadowScope();
        CreateLuminanceReduceScopes();
        CreateTonemappingScope();
        CreateForwardScope();
        CreateCopyTextureScope();

        m_imguiSidebar.Activate();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        ExampleComponentRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

        m_fullyActivated = true;
    }

    void AsyncComputeExampleComponent::Deactivate()
    {
        m_assetLoadManager->Cancel();
        m_fullyActivated = false;

        m_quadBufferPool = nullptr;
        m_quadInputAssemblyBuffer = nullptr;
        m_quadStreamBufferViews.fill(AZStd::vector<AZ::RHI::StreamBufferView>());
        m_terrainPipelineStates.fill(nullptr);
        m_modelStreamBufferViews.fill(AZ::RPI::ModelLod::StreamBufferViewList());
        m_modelPipelineStates.fill(nullptr);
        m_model = nullptr;
        m_copyTexturePipelineState = nullptr;
        m_luminancePipelineState = nullptr;
        m_luminanceReducePipelineState = nullptr;
        m_tonemappingPipelineState = nullptr;
        m_shaderResourceGroups.fill(AZStd::vector<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>>());
        m_viewShaderResourceGroup = nullptr;
        m_shaders.fill(nullptr);
        m_imagePool = nullptr;
        m_sceneImages.fill(nullptr);

        m_scopeProducers.clear();
        m_windowContext = nullptr;

        m_imguiSidebar.Deactivate();
        ExampleComponentRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
    }

    void AsyncComputeExampleComponent::SetupScene()
    {
        // Shader inputs
        const Name shaderInputImageWorldMatrix{ "m_worldMatrix" };
        const Name shaderInputImageViewProjectionMatrix{ "m_viewProjectionMatrix" };
        const Name shaderInputImageLightViewProjectionMatrix{ "m_lightViewProjectionMatrix" };
        const Name shaderInputImageLightPosition{ "m_lightPosition" };
        const Name shaderInputImageAmbientColor{ "m_ambientColor" };
        const Name shaderInputImageDiffuseColor{ "m_diffuseColor" };
        const Name shaderInputImageDepthMapTexture{ "m_depthMapTexture" };
        const Name textureInputImageTexture{ "m_texture" };
        const Name luminanceReduceInputImageTexture{ "m_inputTexture" };
        const Name luminanceReduceOutputImageTexture{ "m_outputTexture" };
        const Name tonemappingImageTexture{ "m_inOutTexture" };
        const Name tonemappingLuminanceImageTexture{ "m_luminanceTexture" };

        // These are the model matrices. They contain the rotation, translation and scale of the objects that we are going to draw.
        AZStd::vector<AZ::Matrix4x4> objectMatrices = 
        {
            AZ::Matrix4x4::CreateScale(AZ::Vector3(12.0f)),
            AZ::Matrix4x4::CreateRotationZ(AZ::Constants::Pi),
            AZ::Matrix4x4::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi * 3.0f), AZ::Vector3(5.0f, 8.0f, 0.0f)),
            AZ::Matrix4x4::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateRotationZ(-AZ::Constants::QuarterPi * 3.0f), AZ::Vector3(-6.0f, 6.0f, 0.0f)) * AZ::Matrix4x4::CreateScale(AZ::Vector3(1.5f)),
            AZ::Matrix4x4::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateRotationZ(-AZ::Constants::QuarterPi * 3.0f), AZ::Vector3(-7.0f, -4.0f, 0.0f)) * AZ::Matrix4x4::CreateScale(AZ::Vector3(0.6f)),
            AZ::Matrix4x4::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi * 2.0), AZ::Vector3(4.0f, -4.0f, 0.0f)) * AZ::Matrix4x4::CreateScale(AZ::Vector3(1.3f))
        };

        const float zNear = 1.0f, zFar = 100.0f;
        const AZ::Vector3 up = AZ::Vector3(0.0f, 0.0f, 1.0f);
        const AZ::Vector3 lookAt = AZ::Vector3(0.0f, 0.0f, 0.0f);

        auto ambientColor = AZ::Vector4(0.15f, 0.15f, 0.15f, 1.0f);
        auto diffuseColor = AZ::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        // Camera
        float fieldOfView = AZ::Constants::Pi / 4.0f;
        float screenAspect = GetViewportWidth() / GetViewportHeight();
        MakePerspectiveFovMatrixRH(m_projectionMatrix, fieldOfView, screenAspect, zNear, zFar);

        // Light
        float fovY = AZ::Constants::Pi / 2.0f;
        float aspectRatio = 1.0f;
        auto lightPosition = AZ::Vector3(-5.0f, -12.0f, 8.0f);
        AZ::Matrix4x4 lightProjectMatrix;
        MakePerspectiveFovMatrixRH(lightProjectMatrix, fovY, aspectRatio, zNear, zFar);
        AZ::Matrix4x4 worldToLight = lightProjectMatrix * CreateViewMatrix(lightPosition, up, lookAt);

        // Shadow generation SRGs
        m_shaderResourceGroups[ShadowScope].reserve(objectMatrices.size());
        for (const AZ::Matrix4x4& objectMatrix : objectMatrices)
        {
            auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[ShadowScope], "DepthViewSrg", AsyncCompute::sampleName);
            RHI::ShaderInputConstantIndex viewProjectionMatrixInput;
            RHI::ShaderInputConstantIndex modelMatrixInput;
            FindShaderInputIndex(&viewProjectionMatrixInput, shaderResourceGroup, shaderInputImageViewProjectionMatrix, AsyncCompute::sampleName);
            FindShaderInputIndex(&modelMatrixInput, shaderResourceGroup, shaderInputImageWorldMatrix, AsyncCompute::sampleName);

            shaderResourceGroup->SetConstant(viewProjectionMatrixInput, worldToLight);
            shaderResourceGroup->SetConstant(modelMatrixInput, objectMatrix);
            shaderResourceGroup->Compile();
            m_shaderResourceGroups[ShadowScope].push_back(shaderResourceGroup);
        }

        // Forward scope SRGs
        m_shaderResourceGroups[ForwardScope].reserve(objectMatrices.size());
        for (const AZ::Matrix4x4& objectMatrix : objectMatrices)
        {
            auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[ForwardScope], "ShadowSrg", AsyncCompute::sampleName);

            AZStd::array<RHI::ShaderInputConstantIndex, 5> shaderInputIndices;
            FindShaderInputIndex(&shaderInputIndices[0], shaderResourceGroup, shaderInputImageWorldMatrix, AsyncCompute::sampleName);
            FindShaderInputIndex(&shaderInputIndices[1], shaderResourceGroup, shaderInputImageLightViewProjectionMatrix, AsyncCompute::sampleName);
            FindShaderInputIndex(&shaderInputIndices[2], shaderResourceGroup, shaderInputImageLightPosition, AsyncCompute::sampleName);
            FindShaderInputIndex(&shaderInputIndices[3], shaderResourceGroup, shaderInputImageAmbientColor, AsyncCompute::sampleName);
            FindShaderInputIndex(&shaderInputIndices[4], shaderResourceGroup, shaderInputImageDiffuseColor, AsyncCompute::sampleName);
            FindShaderInputIndex(&m_shaderInputImageIndex, shaderResourceGroup, shaderInputImageDepthMapTexture, AsyncCompute::sampleName);

            shaderResourceGroup->SetConstant(shaderInputIndices[0], objectMatrix);
            shaderResourceGroup->SetConstant(shaderInputIndices[1], worldToLight);
            shaderResourceGroup->SetConstant(shaderInputIndices[2], AZ::Vector4::CreateFromVector3AndFloat(lightPosition, 1.0f));
            shaderResourceGroup->SetConstant(shaderInputIndices[3], ambientColor);
            shaderResourceGroup->SetConstant(shaderInputIndices[4], diffuseColor);
            m_shaderResourceGroups[ForwardScope].push_back(shaderResourceGroup);
            // Do not compile now since this SRG is updated every frame with the image input.
        }

        {
            // Copy texture SRG
            auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[CopyTextureScope], "CopyQueueSrg", AsyncCompute::sampleName);
            FindShaderInputIndex(&m_copyTextureShaderInputImageIndex, shaderResourceGroup, textureInputImageTexture, AsyncCompute::sampleName);
            m_shaderResourceGroups[CopyTextureScope].push_back(shaderResourceGroup);
        }

        {
            // Luminance map SRG
            auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[LuminanceMapScope], "TextureInstanceSrg", AsyncCompute::sampleName);
            FindShaderInputIndex(&m_luminanceShaderInputImageIndex, shaderResourceGroup, textureInputImageTexture, AsyncCompute::sampleName);
            m_shaderResourceGroups[LuminanceMapScope].push_back(shaderResourceGroup);
        }

        {
            // Luminance reduce SRGs
            AZ_Assert(m_numThreads[LuminanceReduceScope].m_X == m_numThreads[LuminanceReduceScope].m_Y, "If the shader source changes, this logic should change too.");
            AZ_Assert(m_numThreads[LuminanceReduceScope].m_Z == 1, "If the shader source changes, this logic should change too.");
            const auto luminanceMapThreadGroupSize = m_numThreads[LuminanceReduceScope].m_X;
            for (uint32_t size = AsyncCompute::s_luminanceMapSize; size > 1; size = AZStd::max(size / (luminanceMapThreadGroupSize * 2), 1u))
            {
                auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[LuminanceReduceScope], "TexturesSrg", AsyncCompute::sampleName);
                FindShaderInputIndex(&m_luminanceReduceShaderInputImageIndex, shaderResourceGroup, luminanceReduceInputImageTexture, AsyncCompute::sampleName);
                FindShaderInputIndex(&m_luminanceReduceShaderOutputImageIndex, shaderResourceGroup, luminanceReduceOutputImageTexture, AsyncCompute::sampleName);
                m_shaderResourceGroups[LuminanceReduceScope].push_back(shaderResourceGroup);
            }
        }

        {
            // Tonemapping SRGs
            auto shaderResourceGroup = CreateShaderResourceGroup(m_shaders[TonemappingScope], "TexturesSrg", AsyncCompute::sampleName);
            FindShaderInputIndex(&m_tonemappingShaderImageIndex, shaderResourceGroup, tonemappingImageTexture, AsyncCompute::sampleName);
            FindShaderInputIndex(&m_tonemappingLuminanceImageIndex, shaderResourceGroup, tonemappingLuminanceImageTexture, AsyncCompute::sampleName);
            m_shaderResourceGroups[TonemappingScope].push_back(shaderResourceGroup);
        }
    }

    void AsyncComputeExampleComponent::SetArcBallControllerParams()
    {
        AZ::Debug::CameraControllerRequestBus::Event(m_cameraEntityId, &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());

        AZ::RPI::ViewPtr cameraView;
        // The RPI::View associated to this component can be obtained through the ViewProvider, by using Entity Id.
        AZ::RPI::ViewProviderBus::EventResult(cameraView, m_cameraEntityId, &AZ::RPI::ViewProvider::GetView);
        if (cameraView)
        {
            m_viewShaderResourceGroup = cameraView->GetShaderResourceGroup();
        }
    }

    void AsyncComputeExampleComponent::CreateCopyTextureScope()
    {
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            auto& source = m_sceneIds[m_previousSceneImageIndex];
            auto& destination = m_outputAttachmentId;
            {
                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = destination;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseColorAttachment(descriptor);
                }

                {
                    RHI::ImageScopeAttachmentDescriptor descriptor;
                    descriptor.m_attachmentId = source;
                    descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseShaderAttachment(descriptor, RHI::ScopeAttachmentAccess::Read);
                }
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            auto& source = m_sceneIds[m_previousSceneImageIndex];
            auto& shaderResourceGroup = m_shaderResourceGroups[CopyTextureScope].front();

            const auto* imageView = context.GetImageView(source);
            shaderResourceGroup->SetImageView(m_copyTextureShaderInputImageIndex, imageView);
            shaderResourceGroup->Compile();
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            {
                // Quad
                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[CopyTextureScope]
                                                                                     .front()
                                                                                     ->GetRHIShaderResourceGroup()
                                                                                     ->GetDeviceShaderResourceGroup(
                                                                                         context.GetDeviceIndex())
                                                                                     .get() };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_copyTexturePipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                auto deviceIndexBufferView{m_quadIndexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                drawItem.m_indexBufferView = &deviceIndexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_quadStreamBufferViews[CopyTextureScope].size());
                AZStd::vector<RHI::DeviceStreamBufferView> deviceQuadStreamBufferViews;
                for(const auto& streamBufferView : m_quadStreamBufferViews[CopyTextureScope])
                {
                    deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                }
                drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();
                commandList->Submit(drawItem);
            }
       };

        AZStd::string name = AZStd::string::format("CopyTextureToSwapchain");
        const RHI::ScopeId shadowScope(name);
        m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                shadowScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void AsyncComputeExampleComponent::CreateShadowScope()
    {
        // Generate shadowmap texture.
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Create & Binds DepthStencil image
            {
                RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_shadowAttachmentId;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = RHI::Format::D32_FLOAT;
                dsDesc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateDepthStencil(1.0f, 0);
                dsDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, RHI::ScopeAttachmentAccess::ReadWrite);
            }

            frameGraph.SetEstimatedItemCount(static_cast<uint32_t>(m_shaderResourceGroups[ShadowScope].size()));
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            float shadowMapSizeFloat = static_cast<float>(AsyncCompute::s_shadowMapSize);
            int32_t shadowMapSizeInt = static_cast<int32_t>(AsyncCompute::s_shadowMapSize);
            RHI::Viewport viewport = RHI::Viewport(0, shadowMapSizeFloat, 0, shadowMapSizeFloat);
            RHI::Scissor scissor(0, 0, shadowMapSizeInt, shadowMapSizeInt);
            commandList->SetViewports(&viewport, 1);
            commandList->SetScissors(&scissor, 1);

            for(uint32_t i = context.GetSubmitRange().m_startIndex; i < context.GetSubmitRange().m_endIndex; ++i)
            {
                if (i == 0)
                {
                    // Terrain
                    RHI::DrawIndexed drawIndexed;
                    drawIndexed.m_indexCount = 6;
                    drawIndexed.m_instanceCount = 1;

                    const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[ShadowScope][0]
                                                                                         ->GetRHIShaderResourceGroup()
                                                                                         ->GetDeviceShaderResourceGroup(
                                                                                             context.GetDeviceIndex())
                                                                                         .get() };

                    RHI::DeviceDrawItem drawItem;
                    drawItem.m_arguments = drawIndexed;
                    drawItem.m_pipelineState = m_terrainPipelineStates[ShadowScope]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                    auto deviceIndexBufferView{m_quadIndexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                    drawItem.m_indexBufferView = &deviceIndexBufferView;
                    drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                    drawItem.m_shaderResourceGroups = shaderResourceGroups;
                    drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_quadStreamBufferViews[ShadowScope].size());
                    AZStd::vector<RHI::DeviceStreamBufferView> deviceQuadStreamBufferViews;
                    for(const auto& streamBufferView : m_quadStreamBufferViews[ShadowScope])
                    {
                        deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                    }
                    drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();
                    commandList->Submit(drawItem, i);
                }
                else
                {
                    // Models
                    const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[ShadowScope][i]
                                                                                         ->GetRHIShaderResourceGroup()
                                                                                         ->GetDeviceShaderResourceGroup(
                                                                                             context.GetDeviceIndex())
                                                                                         .get() };
                    for (const auto& mesh : m_model->GetLods()[0]->GetMeshes())
                    {
                        RHI::DeviceDrawItem drawItem;
                        drawItem.m_arguments = mesh.m_drawArguments.GetDeviceDrawArguments(context.GetDeviceIndex());
                        drawItem.m_pipelineState = m_modelPipelineStates[ShadowScope]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                        auto deviceIndexBufferView{mesh.m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                        drawItem.m_indexBufferView = &deviceIndexBufferView;
                        drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                        drawItem.m_shaderResourceGroups = shaderResourceGroups;
                        drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_modelStreamBufferViews[ShadowScope].size());
                        AZStd::vector<RHI::DeviceStreamBufferView> deviceQuadStreamBufferViews;
                        for(const auto& streamBufferView : m_modelStreamBufferViews[ShadowScope])
                        {
                            deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                        }
                        drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();
                        commandList->Submit(drawItem, i);
                    }
                }
            }
        };
        
        const RHI::ScopeId shadowScope("ShadowScope");
        m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                shadowScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void AsyncComputeExampleComponent::CreateForwardScope()
    {
        // Render all objects with shadows.
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the scene image. Clears it to black.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_sceneIds[m_currentSceneImageIndex];
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                descriptor.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateVector4Float(0.f, 0.f, 0.f, 0.f);
                frameGraph.UseColorAttachment(descriptor);
            }

            // Binds depth buffer from depth pass
            {
                frameGraph.UseShaderAttachment(RHI::ImageScopeAttachmentDescriptor(m_shadowAttachmentId), RHI::ScopeAttachmentAccess::Read);
            }

            // Binds DepthStencil image
            {
                RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
                RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = m_forwardDepthStencilId;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = device->GetNearestSupportedFormat(RHI::Format::D24_UNORM_S8_UINT, AZ::RHI::FormatCapabilities::DepthStencil);
                dsDesc.m_loadStoreAction.m_clearValue = RHI::ClearValue::CreateDepthStencil(0, 0);
                dsDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                dsDesc.m_loadStoreAction.m_loadActionStencil = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, RHI::ScopeAttachmentAccess::Write);
            }

            frameGraph.SetEstimatedItemCount(static_cast<uint32_t>(m_shaderResourceGroups[ForwardScope].size()));
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageView = context.GetImageView(m_shadowAttachmentId);

            for (const auto& shaderResourceGroup : m_shaderResourceGroups[ForwardScope])
            {
                shaderResourceGroup->SetImageView(m_shaderInputImageIndex, imageView);
                shaderResourceGroup->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Bind ViewSrg
            commandList->SetShaderResourceGroupForDraw(*m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get());

            // Set persistent viewport and scissor state.
            const auto& imageSize = m_sceneImages[m_currentSceneImageIndex]->GetDescriptor().m_size;
            RHI::Viewport viewport(0, static_cast<float>(imageSize.m_width), 0, static_cast<float>(imageSize.m_height));
            RHI::Scissor scissor(0, 0, static_cast<int32_t>(imageSize.m_width), static_cast<int32_t>(imageSize.m_height));
            commandList->SetViewports(&viewport, 1);
            commandList->SetScissors(&scissor, 1);

            for (uint32_t i = context.GetSubmitRange().m_startIndex; i < context.GetSubmitRange().m_endIndex; ++i)
            {
                if (i == 0)
                {
                    // Terrain
                    const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                        m_shaderResourceGroups[ForwardScope][0]
                            ->GetRHIShaderResourceGroup()
                            ->GetDeviceShaderResourceGroup(context.GetDeviceIndex())
                            .get(),
                        m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                    };

                    RHI::DrawIndexed drawIndexed;
                    drawIndexed.m_indexCount = 6;
                    drawIndexed.m_instanceCount = 1;

                    RHI::DeviceDrawItem drawItem;
                    drawItem.m_arguments = drawIndexed;
                    drawItem.m_pipelineState = m_terrainPipelineStates[ForwardScope]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                    auto deviceIndexBufferView{m_quadIndexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                    drawItem.m_indexBufferView = &deviceIndexBufferView;
                    drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                    drawItem.m_shaderResourceGroups = shaderResourceGroups;
                    drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_quadStreamBufferViews[ForwardScope].size());
                    AZStd::vector<RHI::DeviceStreamBufferView> deviceQuadStreamBufferViews;
                    for(const auto& streamBufferView : m_quadStreamBufferViews[ForwardScope])
                    {
                        deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                    }
                    drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();

                    commandList->Submit(drawItem, i);
                }
                else
                {
                    // Model
                    const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = {
                        m_shaderResourceGroups[ForwardScope][i]
                            ->GetRHIShaderResourceGroup()
                            ->GetDeviceShaderResourceGroup(context.GetDeviceIndex())
                            .get(),
                        m_viewShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get()
                    };

                    for (const auto& mesh : m_model->GetLods()[0]->GetMeshes())
                    {
                        RHI::DeviceDrawItem drawItem;
                        drawItem.m_arguments = mesh.m_drawArguments.GetDeviceDrawArguments(context.GetDeviceIndex());
                        drawItem.m_pipelineState = m_modelPipelineStates[ForwardScope]->GetDevicePipelineState(context.GetDeviceIndex()).get();
                        auto deviceIndexBufferView{mesh.m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                        drawItem.m_indexBufferView = &deviceIndexBufferView;
                        drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                        drawItem.m_shaderResourceGroups = shaderResourceGroups;
                        drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_modelStreamBufferViews[ForwardScope].size());
                        AZStd::vector<RHI::DeviceStreamBufferView> deviceQuadStreamBufferViews;
                        for(const auto& streamBufferView : m_modelStreamBufferViews[ForwardScope])
                        {
                            deviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                        }
                        drawItem.m_streamBufferViews = deviceQuadStreamBufferViews.data();

                        commandList->Submit(drawItem, i);
                    }
                }
            }
       };

        const RHI::ScopeId forwardScope("ForwardScope");
        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                forwardScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void AsyncComputeExampleComponent::CreateTonemappingScope()
    {
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                RHI::ImageScopeAttachmentDescriptor inputOuputDescriptor;
                inputOuputDescriptor.m_attachmentId = m_sceneIds[m_previousSceneImageIndex];
                inputOuputDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseShaderAttachment(inputOuputDescriptor, RHI::ScopeAttachmentAccess::ReadWrite);

                RHI::ImageScopeAttachmentDescriptor luminanceDescriptor;
                luminanceDescriptor.m_attachmentId = m_averageLuminanceAttachmentId;
                luminanceDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseShaderAttachment(luminanceDescriptor, RHI::ScopeAttachmentAccess::Read);
            }

            frameGraph.SetEstimatedItemCount(1);
            frameGraph.SetHardwareQueueClass(m_asyncComputeEnabled ? RHI::HardwareQueueClass::Compute : RHI::HardwareQueueClass::Graphics);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* hdrSceneView = context.GetImageView(m_sceneIds[m_previousSceneImageIndex]);
            const auto* luminanceView = context.GetImageView(m_averageLuminanceAttachmentId);

            for (const auto& shaderResourceGroup : m_shaderResourceGroups[TonemappingScope])
            {
                shaderResourceGroup->SetImageView(m_tonemappingShaderImageIndex, hdrSceneView);
                shaderResourceGroup->SetImageView(m_tonemappingLuminanceImageIndex, luminanceView);
                shaderResourceGroup->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            RHI::DeviceDispatchItem dispatchItem;
            decltype(dispatchItem.m_shaderResourceGroups) shaderResourceGroups = { { m_shaderResourceGroups[TonemappingScope][0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() } };

            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_totalNumberOfThreadsX = m_outputWidth;
            dispatchArgs.m_threadsPerGroupX      = aznumeric_cast<uint16_t>(m_numThreads[TonemappingScope].m_X);
            dispatchArgs.m_totalNumberOfThreadsY = m_outputHeight;
            dispatchArgs.m_threadsPerGroupY      = aznumeric_cast<uint16_t>(m_numThreads[TonemappingScope].m_Y);
            dispatchArgs.m_totalNumberOfThreadsZ = 1;
            dispatchArgs.m_threadsPerGroupZ      = aznumeric_cast<uint16_t>(m_numThreads[TonemappingScope].m_Z);

            AZ_Assert(dispatchArgs.m_threadsPerGroupX == dispatchArgs.m_threadsPerGroupY, "If the shader source changes, this logic should change too.");
            AZ_Assert(dispatchArgs.m_threadsPerGroupZ == 1, "If the shader source changes, this logic should change too.");

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_tonemappingPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 1;
            dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

            commandList->Submit(dispatchItem);
        };

        const RHI::ScopeId tonemappingScope("TonemappingScope");
        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                tonemappingScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void AsyncComputeExampleComponent::CreateLuminanceMapScope()
    {
        // Create a luminance map (that will be reduce) from the scene image.
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                RHI::ImageScopeAttachmentDescriptor luminanceMapDesc;
                luminanceMapDesc.m_attachmentId = m_luminanceMapAttachmentId;
                luminanceMapDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                frameGraph.UseColorAttachment(luminanceMapDesc);

                RHI::ImageScopeAttachmentDescriptor sceneDescriptor;
                sceneDescriptor.m_attachmentId = m_sceneIds[m_previousSceneImageIndex];
                sceneDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseShaderAttachment(sceneDescriptor, RHI::ScopeAttachmentAccess::Read);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageView = context.GetImageView(m_sceneIds[m_previousSceneImageIndex]);

            for (const auto& shaderResourceGroup : m_shaderResourceGroups[LuminanceMapScope])
            {
                shaderResourceGroup->SetImageView(m_luminanceShaderInputImageIndex, imageView);
                shaderResourceGroup->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            RHI::Viewport viewport(0, static_cast<float>(AsyncCompute::s_luminanceMapSize), 0, static_cast<float>(AsyncCompute::s_luminanceMapSize));
            RHI::Scissor scissor(0, 0, AsyncCompute::s_luminanceMapSize, AsyncCompute::s_luminanceMapSize);
            commandList->SetViewports(&viewport, 1);
            commandList->SetScissors(&scissor, 1);

            {
                // Quad
                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::DeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[LuminanceMapScope][0]
                                                                                     ->GetRHIShaderResourceGroup()
                                                                                     ->GetDeviceShaderResourceGroup(
                                                                                         context.GetDeviceIndex())
                                                                                     .get() };

                RHI::DeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_luminancePipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                auto deviceIndexBufferView{m_quadIndexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
                drawItem.m_indexBufferView = &deviceIndexBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;
                drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_quadStreamBufferViews[LuminanceMapScope].size());
                AZStd::vector<AZ::RHI::DeviceStreamBufferView> singleDeviceQuadStreamBufferViews;
                for(const auto& streamBufferView : m_quadStreamBufferViews[LuminanceMapScope])
                {
                    singleDeviceQuadStreamBufferViews.emplace_back(streamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex()));
                }
                drawItem.m_streamBufferViews = singleDeviceQuadStreamBufferViews.data();
                commandList->Submit(drawItem);
            }
        };

        const RHI::ScopeId shadowScope("LuminanceMapScope");
        m_scopeProducers.emplace_back(aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                shadowScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void AsyncComputeExampleComponent::CreateLuminanceReduceScopes()
    {
        // We reduce the luminance map texture using multiple chained compute scopes
        // until we get the 1x1 texture.
        RHI::AttachmentId inputAttachmentId = m_luminanceMapAttachmentId;

        // By design, the luminance reduce shader uses the same size for X and Y.
        // If the shader code changes this logic should change too, otherwise taking numThreads.m_X is enough
        const auto luminanceMapThreadGroupSize = m_numThreads[LuminanceReduceScope].m_X;
        for(uint32_t inputSize = AsyncCompute::s_luminanceMapSize, i = 0; inputSize > 1; ++i)
        {
            uint32_t outputSize = AZStd::max(inputSize / (luminanceMapThreadGroupSize * 2), 1u);
            AZStd::string outputAttachmentString = AZStd::string::format("LuminanceReduce%d", static_cast<int>(outputSize));
            RHI::AttachmentId outputAttachmentId(outputAttachmentString);

            const auto prepareFunction = [this, outputSize, inputAttachmentId, outputAttachmentId](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
            {
                {
                    const RHI::ImageDescriptor imageDescriptor = RHI::ImageDescriptor::Create2D(
                        RHI::ImageBindFlags::ShaderReadWrite | RHI::ImageBindFlags::Color,
                        outputSize,
                        outputSize,
                        RHI::Format::R32_FLOAT);

                    frameGraph.GetAttachmentDatabase().CreateTransientImage(RHI::TransientImageDescriptor(outputAttachmentId, imageDescriptor));
                }

                {
                    RHI::ImageScopeAttachmentDescriptor inputDescriptor;
                    inputDescriptor.m_attachmentId = inputAttachmentId;
                    inputDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                    frameGraph.UseShaderAttachment(inputDescriptor, RHI::ScopeAttachmentAccess::Read);

                    RHI::ImageScopeAttachmentDescriptor outputDescriptor;
                    outputDescriptor.m_attachmentId = outputAttachmentId;
                    outputDescriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                    frameGraph.UseShaderAttachment(outputDescriptor, RHI::ScopeAttachmentAccess::ReadWrite);
                }

                frameGraph.SetEstimatedItemCount(1);
                frameGraph.SetHardwareQueueClass(m_asyncComputeEnabled ? RHI::HardwareQueueClass::Compute : RHI::HardwareQueueClass::Graphics);
            };

            const auto compileFunction = [this, inputAttachmentId, outputAttachmentId, i](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                const auto* inputView = context.GetImageView(inputAttachmentId);
                const auto* outputView = context.GetImageView(outputAttachmentId);

                const auto& shaderResourceGroup = m_shaderResourceGroups[LuminanceReduceScope][i];
                shaderResourceGroup->SetImageView(m_luminanceReduceShaderInputImageIndex, inputView);
                shaderResourceGroup->SetImageView(m_luminanceReduceShaderOutputImageIndex, outputView);
                shaderResourceGroup->Compile();
            };

            const auto executeFunction = [this, i, outputSize](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
            {
                RHI::CommandList* commandList = context.GetCommandList();

                RHI::DeviceDispatchItem dispatchItem;
                decltype(dispatchItem.m_shaderResourceGroups) shaderResourceGroups = { { m_shaderResourceGroups[LuminanceReduceScope][i]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() } };

                RHI::DispatchDirect dispatchArgs;

                dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_numThreads[LuminanceReduceScope].m_X);
                dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_numThreads[LuminanceReduceScope].m_Y);
                dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_numThreads[LuminanceReduceScope].m_Z);
                AZ_Assert(dispatchArgs.m_threadsPerGroupZ == 1, "If the shader source changes, this logic should change too.");

                dispatchArgs.m_totalNumberOfThreadsX = outputSize * dispatchArgs.m_threadsPerGroupX;
                dispatchArgs.m_totalNumberOfThreadsY = outputSize * dispatchArgs.m_threadsPerGroupY;
                dispatchArgs.m_totalNumberOfThreadsZ = 1;

                dispatchItem.m_arguments = dispatchArgs;
                dispatchItem.m_pipelineState = m_luminanceReducePipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                dispatchItem.m_shaderResourceGroupCount = 1;
                dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

                commandList->Submit(dispatchItem);
            };
            
            AZStd::string scopeName = AZStd::string::format("LuminanceReduce%d", static_cast<int>(outputSize));
            const RHI::ScopeId tonemappingScope(scopeName);
            m_scopeProducers.emplace_back(
                aznew RHI::ScopeProducerFunction<
                ScopeData,
                decltype(prepareFunction),
                decltype(compileFunction),
                decltype(executeFunction)>(
                    tonemappingScope,
                    ScopeData{},
                    prepareFunction,
                    compileFunction,
                    executeFunction));

            inputAttachmentId = outputAttachmentId;
            inputSize = outputSize;
        }
    }

    bool AsyncComputeExampleComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        auto config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        AZ_Assert(config && config->IsValid(), "SampleComponentConfig required for sample component configuration.");
        m_cameraEntityId = config->m_cameraEntityId;
        return BasicRHIComponent::ReadInConfig(baseConfig);
    }
}
