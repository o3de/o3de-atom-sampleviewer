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
#include <Atom/RHI.Reflect/QueryPoolDescriptor.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/QueryExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    namespace QueryExample
    {
        const char* ShaderFilePath = "Shaders/RHI/colorMesh.azshader";
        const char* SampleName = "QueryExample";
        const char* PredicationBufferId = "bufferAttachmentId";
    }

    void QueryExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<QueryExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    QueryExampleComponent::QueryExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void QueryExampleComponent::Activate()
    {
        CreateGeometryResources();
        CreateShaderResources();
        CreateCopyBufferScopeProducer();
        CreateScopeProducer();
        CreateQueryResources();
        CreatePredicationResources();

        SetQueryType(m_currentType);
        m_imguiSidebar.Activate();
        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
    }

    void QueryExampleComponent::Deactivate()
    {
        m_quadInputAssemblyBuffer = nullptr;
        m_inputAssemblyBufferPool = nullptr;
        m_quadPipelineState = nullptr;
        m_boudingBoxPipelineState = nullptr;
        m_shaderResourceGroups.fill(nullptr);

        QueryEntry nullQueryEntry;
        nullQueryEntry.m_isValid = false;
        nullQueryEntry.m_query = nullptr;
        m_occlusionQueries.fill(nullQueryEntry);
        m_timestampQueries.fill(nullQueryEntry);
        m_statisticsQueries.fill(nullQueryEntry);
        m_occlusionQueryPool = nullptr;
        m_timeStampQueryPool = nullptr;
        m_statisticsQueryPool = nullptr;

        m_predicationBuffer = nullptr;
        m_predicationBufferPool = nullptr;

        m_timestampEnabled = false;
        m_pipelineStatisticsEnabled = false;
        m_precisionOcclusionEnabled = false;

        m_imguiSidebar.Deactivate();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        m_windowContext = nullptr;
        m_scopeProducers.clear();
    }

    void QueryExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {         
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void QueryExampleComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_elapsedTime += deltaTime;

        switch (m_currentType)
        {
        case QueryType::Occlusion:
            m_currentOcclusionQueryIndex = (m_currentOcclusionQueryIndex + 1) % static_cast<uint32_t>(m_occlusionQueries.size());
            break;
        case QueryType::Predication:
            // For predication we use only one query
            m_currentOcclusionQueryIndex = 0;
            break;
        }

        {
            auto scaleMatrix = AZ::Matrix4x4::CreateScale(AZ::Vector3(0.2, 0.2, 0.2));
            [[maybe_unused]] bool success = m_shaderResourceGroups[0]->SetConstant(m_objectMatrixConstantIndex, scaleMatrix * AZ::Matrix4x4::CreateTranslation(AZ::Vector3(0, 0, 0.5)));
            success &= m_shaderResourceGroups[0]->SetConstant(m_colorConstantIndex, AZ::Vector4(1.0, 0, 0, 1.0));
            AZ_Warning(QueryExample::SampleName, success, "Failed to set SRG Constant data");
        }
        {
            auto scaleMatrix = AZ::Matrix4x4::CreateScale(AZ::Vector3(0.5, 0.5, 0.5));
            AZ::Vector3 translation(
                sinf(m_elapsedTime),
                0.0f,
                0.0f);
            [[maybe_unused]] bool success = m_shaderResourceGroups[1]->SetConstant(m_objectMatrixConstantIndex, scaleMatrix * AZ::Matrix4x4::CreateTranslation(translation));
            success &= m_shaderResourceGroups[1]->SetConstant(m_colorConstantIndex, AZ::Color(1.f, 1.f, 1.f, 0.5f).GetAsVector4());
            AZ_Warning(QueryExample::SampleName, success, "Failed to set SRG Constant data");
        }
        {
            auto scaleMatrix = AZ::Matrix4x4::CreateScale(AZ::Vector3(0.2, 0.2, 0.2));
            [[maybe_unused]] bool success = m_shaderResourceGroups[2]->SetConstant(m_objectMatrixConstantIndex, scaleMatrix * AZ::Matrix4x4::CreateTranslation(AZ::Vector3(0, 0, 0.49)));
            AZ_Warning(QueryExample::SampleName, success, "Failed to set SRG Constant data");
        }

        if(m_timestampEnabled)
        {
            // Timestamp use two queries to get the time.
            m_currentTimestampQueryIndex = (m_currentTimestampQueryIndex + 2) % m_timestampQueries.size();
        }

        if (m_pipelineStatisticsEnabled)
        {            
            m_currentStatisticsQueryIndex = (m_currentStatisticsQueryIndex + 1) % static_cast<uint32_t>(m_statisticsQueries.size());
        }

        if (m_imguiSidebar.Begin())
        {
            DrawSampleSettings();
        }
    }

    void QueryExampleComponent::CreateGeometryResources()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(RHI::MultiDevice::DefaultDevice, bufferPoolDesc);

        {
            // Create quad buffer and views
            BufferData bufferData;
            SetFullScreenRect(bufferData.m_positions.data(), nullptr, bufferData.m_indices.data());

            m_quadInputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();

            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::MultiDeviceBufferInitRequest request;
            request.m_buffer = m_quadInputAssemblyBuffer.get();
            request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
            request.m_initialData = &bufferData;
            result = m_inputAssemblyBufferPool->InitBuffer(request);

            if (result != RHI::ResultCode::Success)
            {
                AZ_Error(QueryExample::SampleName, false, "Failed to initialize position buffer with error code %d", result);
                return;
            }

            m_quadStreamBufferView = {
                *m_quadInputAssemblyBuffer,
                offsetof(BufferData, m_positions),
                sizeof(BufferData::m_positions),
                sizeof(VertexPosition)
            };

            RHI::InputStreamLayoutBuilder layoutBuilder;
            layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
            m_quadInputStreamLayout = layoutBuilder.End();

            RHI::ValidateStreamBufferViews(m_quadInputStreamLayout, AZStd::span<const RHI::MultiDeviceStreamBufferView>(&m_quadStreamBufferView, 1));
        }
    }

    const AZ::RHI::MultiDevicePipelineState* CreatePipelineState(
        const AZ::RHI::InputStreamLayout& inputStreamLayout, 
        const AZ::Data::Instance<AZ::RPI::Shader>& shader, 
        const AZ::RHI::Format format,
        const AZ::RHI::TargetBlendState& blendState)
    {
        AZ::RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
        pipelineStateDescriptor.m_inputStreamLayout = inputStreamLayout;

        auto shaderVariant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);
        shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);

        AZ::RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(format)
            ->DepthStencilAttachment(AZ::RHI::Format::D32_FLOAT);
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineStateDescriptor.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        pipelineStateDescriptor.m_renderStates.m_blendState.m_targets[0] = blendState;
        pipelineStateDescriptor.m_renderStates.m_depthStencilState = AZ::RHI::DepthStencilState::CreateDepth();

        return shader->AcquirePipelineState(pipelineStateDescriptor);
    }

    void QueryExampleComponent::CreateShaderResources()
    {
        using namespace AZ;
        auto shader = LoadShader(QueryExample::ShaderFilePath, QueryExample::SampleName);
        if (!shader)
        {
            return;
        }

        for (size_t i = 0; i < m_shaderResourceGroups.size(); ++i)
        {
            m_shaderResourceGroups[i] = CreateShaderResourceGroup(shader, "InstanceSrg", QueryExample::SampleName);
        }
        
        const Name objectMatrixConstantId{ "m_objectMatrix" };
        FindShaderInputIndex(&m_objectMatrixConstantIndex, m_shaderResourceGroups.front(), objectMatrixConstantId, QueryExample::SampleName);

        const Name colorConstantId{ "m_color" };
        FindShaderInputIndex(&m_colorConstantIndex, m_shaderResourceGroups.front(), colorConstantId, QueryExample::SampleName);

        RHI::TargetBlendState blendState;
        blendState.m_enable = true;
        blendState.m_blendSource = RHI::BlendFactor::AlphaSource;
        blendState.m_blendDest = RHI::BlendFactor::AlphaSourceInverse;

        // The top quad is translucent to see what's behind and if the back quad is being rendered.
        m_quadPipelineState = CreatePipelineState(m_quadInputStreamLayout, shader, m_outputFormat, blendState);
        if (!m_quadPipelineState)
        {
            AZ_Error(QueryExample::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", QueryExample::ShaderFilePath);
            return;
        }

        // Bounding box quad doesn't write to the render targets.
        blendState.m_writeMask = 0;
        m_boudingBoxPipelineState = CreatePipelineState(m_quadInputStreamLayout, shader, m_outputFormat, blendState);
        if (!m_boudingBoxPipelineState)
        {
            AZ_Error(QueryExample::SampleName, false, "Failed to acquire default pipeline state for shader '%s'", QueryExample::ShaderFilePath);
            return;
        }      
    }

    void QueryExampleComponent::CreateCopyBufferScopeProducer()
    {
        // This scope is used for copying the results from the query to a buffer that is later
        // used for predication.
        using namespace AZ;
        
        struct ScopeData
        {
        };
        
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Handle the case where the window may have closed
            if (m_currentType != QueryType::Predication)
            {
                return;
            }
            
            [[maybe_unused]] RHI::ResultCode result = frameGraph.GetAttachmentDatabase().ImportBuffer(RHI::AttachmentId{QueryExample::PredicationBufferId}, m_predicationBuffer);
            AZ_Error(QueryExample::SampleName, result == RHI::ResultCode::Success, "Failed to import predication buffer with error %d", result);

            frameGraph.UseQueryPool(
                m_occlusionQueryPool,
                RHI::Interval(m_currentOcclusionQueryIndex, m_currentOcclusionQueryIndex),
                RHI::QueryPoolScopeAttachmentType::Local,
                RHI::ScopeAttachmentAccess::Read);

            frameGraph.UseCopyAttachment(m_predicationBufferAttachmentDescriptor, RHI::ScopeAttachmentAccess::Write);
            frameGraph.SetEstimatedItemCount(1);
        };

        AZ::RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            // Handle the case where the window may have closed
            if (m_currentType != QueryType::Predication)
            {
                return;
            }
            
            RHI::SingleDeviceCopyQueryToBufferDescriptor descriptor;
            descriptor.m_sourceQueryPool = m_occlusionQueryPool->GetDeviceQueryPool(context.GetDeviceIndex()).get();
            descriptor.m_firstQuery = m_occlusionQueries[m_currentOcclusionQueryIndex].m_query->GetHandle(context.GetDeviceIndex());
            descriptor.m_queryCount = 1;
            descriptor.m_destinationBuffer = m_predicationBuffer->GetDeviceBuffer(context.GetDeviceIndex()).get();
            descriptor.m_destinationOffset = 0;
            descriptor.m_destinationStride = sizeof(uint64_t);

            RHI::SingleDeviceCopyItem copyItem(descriptor);

            context.GetCommandList()->Submit(copyItem);
        };

        const AZ::RHI::ScopeId occlusionScope("CopyPredicationBuffer");

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                occlusionScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void QueryExampleComponent::CreateScopeProducer()
    {
        using namespace AZ;
        
        struct ScopeData
        {
        };
        
        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                // Binds the swapchain color attachment.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;

                frameGraph.UseColorAttachment(descriptor);
            }

            {
                // Binds the transient depth attachment
                auto depthAttachmentId = AZ::RHI::AttachmentId{ "DepthStencilID" };
                const AZ::RHI::ImageDescriptor imageDescriptor = AZ::RHI::ImageDescriptor::Create2D(
                    AZ::RHI::ImageBindFlags::DepthStencil,
                    static_cast<uint32_t>(GetViewportWidth()),
                    static_cast<uint32_t>(GetViewportHeight()),
                    AZ::RHI::Format::D32_FLOAT);
                const AZ::RHI::TransientImageDescriptor transientImageDescriptor(depthAttachmentId, imageDescriptor);

                frameGraph.GetAttachmentDatabase().CreateTransientImage(transientImageDescriptor);

                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = depthAttachmentId;
                dsDesc.m_imageViewDescriptor.m_overrideFormat = AZ::RHI::Format::D32_FLOAT;
                dsDesc.m_loadStoreAction.m_clearValue = AZ::RHI::ClearValue::CreateDepth(1.f);
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Clear;
                frameGraph.UseDepthStencilAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::Write);
            }

            // Query pools
            {
                frameGraph.UseQueryPool(
                    m_occlusionQueryPool,
                    RHI::Interval(m_currentOcclusionQueryIndex, m_currentOcclusionQueryIndex),
                    m_currentType == QueryType::Predication ? RHI::QueryPoolScopeAttachmentType::Local : RHI::QueryPoolScopeAttachmentType::Global, 
                    RHI::ScopeAttachmentAccess::Write);

                if (m_timestampEnabled)
                {
                    frameGraph.UseQueryPool(
                        m_timeStampQueryPool,
                        RHI::Interval(m_currentTimestampQueryIndex, m_currentTimestampQueryIndex + 1),
                        RHI::QueryPoolScopeAttachmentType::Global,
                        RHI::ScopeAttachmentAccess::Write);
                }

                if (m_pipelineStatisticsEnabled)
                {
                    frameGraph.UseQueryPool(
                        m_statisticsQueryPool,
                        RHI::Interval(m_currentStatisticsQueryIndex, m_currentStatisticsQueryIndex),
                        RHI::QueryPoolScopeAttachmentType::Global,
                        RHI::ScopeAttachmentAccess::Write);
                }
            }

            // Add the predication buffer if necessary
            if (m_currentType == QueryType::Predication)
            {
                frameGraph.UseAttachment(m_predicationBufferAttachmentDescriptor, RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentUsage::Predication);
            }

            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(3);
        };

        using namespace AZ;
        const auto compileFunction = [this]([[maybe_unused]] const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            for (auto& srg : m_shaderResourceGroups)
            {
                srg->Compile();
            }
        };

        const auto executeFunction = [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            {
                const RHI::SingleDeviceIndexBufferView quadIndexBufferView =
                {
                    *m_quadInputAssemblyBuffer->GetDeviceBuffer(context.GetDeviceIndex()),
                    offsetof(BufferData, m_indices),
                    sizeof(BufferData::m_indices),
                    RHI::IndexFormat::Uint16
                };

                RHI::DrawIndexed drawIndexed;
                drawIndexed.m_indexCount = 6;
                drawIndexed.m_instanceCount = 1;

                const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_shaderResourceGroups[0]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

                RHI::SingleDeviceDrawItem drawItem;
                drawItem.m_arguments = drawIndexed;
                drawItem.m_pipelineState = m_quadPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                drawItem.m_indexBufferView = &quadIndexBufferView;
                drawItem.m_streamBufferViewCount = 1;
                auto deviceStreamBufferView{m_quadStreamBufferView.GetDeviceStreamBufferView(context.GetDeviceIndex())};
                drawItem.m_streamBufferViews = &deviceStreamBufferView;
                drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
                drawItem.m_shaderResourceGroups = shaderResourceGroups;

                // Draw possibly occluded quad
                {
                    if (m_timestampEnabled)
                    {
                        m_timestampQueries[m_currentTimestampQueryIndex].m_query->GetDeviceQuery(context.GetDeviceIndex())->WriteTimestamp(*commandList);
                        m_timestampQueries[m_currentTimestampQueryIndex].m_isValid = true;
                    }

                    if (m_pipelineStatisticsEnabled)
                    {
                        m_statisticsQueries[m_currentStatisticsQueryIndex].m_query->GetDeviceQuery(context.GetDeviceIndex())->Begin(*commandList);
                    }

                    switch (m_currentType)
                    {
                    case QueryType::Occlusion:
                    {
                        // This is the oldest occlusion query (3 frames ago)
                        uint64_t occlusionResults = 0;
                        auto& queryEntry = m_occlusionQueries[(m_currentOcclusionQueryIndex + 1) % m_occlusionQueries.size()];
                        if (queryEntry.m_isValid)
                        {
                            m_occlusionQueryPool->GetResults(queryEntry.m_query.get(), &occlusionResults, sizeof(occlusionResults), RHI::QueryResultFlagBits::Wait);
                        }

                        if (occlusionResults)
                        {
                            commandList->Submit(drawItem, 0);
                        }
                        break;
                    }
                    case QueryType::Predication:
                    {
                        commandList->BeginPredication(*m_predicationBuffer->GetDeviceBuffer(context.GetDeviceIndex()), 0, RHI::PredicationOp::EqualZero);
                        commandList->Submit(drawItem, 0);
                        commandList->EndPredication();
                        break;
                    }
                    }

                    if (m_pipelineStatisticsEnabled)
                    {
                        m_statisticsQueries[m_currentStatisticsQueryIndex].m_query->GetDeviceQuery(context.GetDeviceIndex())->End(*commandList);
                        m_statisticsQueries[m_currentStatisticsQueryIndex].m_isValid = true;
                    }

                    if (m_timestampEnabled)
                    {
                        m_timestampQueries[m_currentTimestampQueryIndex + 1].m_query->GetDeviceQuery(context.GetDeviceIndex())->WriteTimestamp(*commandList);
                        m_timestampQueries[m_currentTimestampQueryIndex + 1].m_isValid = true;
                    }
                }

                // Draw occluding quad
                shaderResourceGroups[0] = m_shaderResourceGroups[1]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
                commandList->Submit(drawItem, 1);

                // Draw quad to use for the oclussion query
                drawItem.m_pipelineState = m_boudingBoxPipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
                shaderResourceGroups[0] = m_shaderResourceGroups[2]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get();
                auto& queryEntry = m_occlusionQueries[m_currentOcclusionQueryIndex];
                queryEntry.m_query->GetDeviceQuery(context.GetDeviceIndex())->Begin(*commandList, m_precisionOcclusionEnabled ? RHI::QueryControlFlags::PreciseOcclusion : RHI::QueryControlFlags::None);
                commandList->Submit(drawItem, 2);
                queryEntry.m_query->GetDeviceQuery(context.GetDeviceIndex())->End(*commandList);
                queryEntry.m_isValid = true;
            }
        };

        const AZ::RHI::ScopeId OcclusionScope("Occlusion");

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                OcclusionScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    template<class T>
    void CreateQueries(AZ::RHI::Ptr<AZ::RHI::MultiDeviceQueryPool>& queryPool, T& queries, AZ::RHI::QueryType type, AZ::RHI::PipelineStatisticsFlags statisticsMask = AZ::RHI::PipelineStatisticsFlags::None)
    {
        using namespace AZ;
        RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        const auto& features = device->GetFeatures();
        if (!RHI::CheckBitsAll(
            features.m_queryTypesMask[static_cast<uint32_t>(RHI::HardwareQueueClass::Graphics)], 
            static_cast<RHI::QueryTypeFlags>(AZ_BIT(static_cast<uint32_t>(type)))))
        {
            return;
        }

        RHI::QueryPoolDescriptor queryPoolDesc;
        queryPoolDesc.m_queriesCount = static_cast<uint32_t>(queries.size());
        queryPoolDesc.m_type = type;
        queryPoolDesc.m_pipelineStatisticsMask = statisticsMask;

        queryPool = aznew RHI::MultiDeviceQueryPool;
        auto result = queryPool->Init(RHI::MultiDevice::DefaultDevice, queryPoolDesc);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Assert(false, "Failed to createa query pool");
            return;
        }

        for (auto& queryEntry : queries)
        {
            queryEntry.m_query = aznew RHI::MultiDeviceQuery;
            queryPool->InitQuery(queryEntry.m_query.get());
        }
    }

    void QueryExampleComponent::CreateQueryResources()
    {
        CreateQueries(m_occlusionQueryPool, m_occlusionQueries, AZ::RHI::QueryType::Occlusion);
        CreateQueries(m_timeStampQueryPool, m_timestampQueries, AZ::RHI::QueryType::Timestamp);
        AZ::RHI::PipelineStatisticsFlags statisticsMask =
            AZ::RHI::PipelineStatisticsFlags::IAVertices | AZ::RHI::PipelineStatisticsFlags::VSInvocations |
            AZ::RHI::PipelineStatisticsFlags::IAPrimitives | AZ::RHI::PipelineStatisticsFlags::CInvocations |
            AZ::RHI::PipelineStatisticsFlags::CPrimitives | AZ::RHI::PipelineStatisticsFlags::PSInvocations;

        CreateQueries(m_statisticsQueryPool, m_statisticsQueries, AZ::RHI::QueryType::PipelineStatistics, statisticsMask);
    }

    void QueryExampleComponent::CreatePredicationResources()
    {
        using namespace AZ;
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        if (!device->GetFeatures().m_predication)
        {
            return;
        }

        m_predicationBufferPool = aznew RHI::MultiDeviceBufferPool();

        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::Predication | RHI::BufferBindFlags::CopyWrite;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_predicationBufferPool->Init(RHI::MultiDevice::DefaultDevice, bufferPoolDesc);

        m_predicationBuffer = aznew RHI::MultiDeviceBuffer();

        RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_predicationBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::Predication | RHI::BufferBindFlags::CopyWrite, sizeof(uint64_t) };
        RHI::ResultCode result = m_predicationBufferPool->InitBuffer(request);

        if (result != RHI::ResultCode::Success)
        {
            AZ_Error(QueryExample::SampleName, false, "Failed to initialize position buffer with error code %d", result);
            return;
        }

        m_predicationBufferAttachmentDescriptor.m_attachmentId = QueryExample::PredicationBufferId;
        const uint64_t resultsCount = 1;
        m_predicationBufferAttachmentDescriptor.m_bufferViewDescriptor = RHI::BufferViewDescriptor::CreateStructured(
            0, 
            resultsCount, 
            static_cast<uint32_t>(m_predicationBuffer->GetDescriptor().m_byteCount / resultsCount));
    }

    void QueryExampleComponent::SetQueryType(QueryType type)
    {
        if (type == m_currentType)
        {
            return;
        }

        m_currentType = type;
        m_elapsedTime = 0;

        if (m_currentType == QueryType::Predication)
        {
            m_precisionOcclusionEnabled = false;
        }
    }

    void QueryExampleComponent::DrawSampleSettings()
    {
        const auto& features = Utils::GetRHIDevice()->GetFeatures();
        ImGui::Spacing();
        AZStd::vector<const char*> items = { "Occlusion" };
        if (features.m_predication)
        {
            items.push_back("Predication");
        }
        int current_item = static_cast<int>(m_currentType);
        ImGui::Text("Type");
        ScriptableImGui::Combo("", &current_item, items.data(), static_cast<int>(items.size()));
        SetQueryType(static_cast<QueryType>(current_item));
        if (m_currentType == QueryType::Occlusion && features.m_occlusionQueryPrecise)
        {
            // Precision occlusion is only available for the Occlusion query type
            ScriptableImGui::Checkbox("Enable Precision occlusion", &m_precisionOcclusionEnabled);
        }

        auto& supportedQueries = features.m_queryTypesMask[static_cast<uint32_t>(AZ::RHI::HardwareQueueClass::Graphics)];
        if (AZ::RHI::CheckBitsAll(supportedQueries, AZ::RHI::QueryTypeFlags::Timestamp))
        {
            ScriptableImGui::Checkbox("Enable Timestamp queries", &m_timestampEnabled);
        }

        if (AZ::RHI::CheckBitsAll(supportedQueries, AZ::RHI::QueryTypeFlags::PipelineStatistics))
        {
            ScriptableImGui::Checkbox("Enable Statistics queries", &m_pipelineStatisticsEnabled);
        }

        if (m_precisionOcclusionEnabled)
        {
            uint64_t occlusionResults = 0;
            // This is the oldest occlusion query (3 frames ago).
            auto queryEntry = m_occlusionQueries[(m_currentOcclusionQueryIndex + 1) % m_occlusionQueries.size()];
            if (queryEntry.m_isValid)
            {
                m_occlusionQueryPool->GetResults(queryEntry.m_query.get(), &occlusionResults, sizeof(occlusionResults), AZ::RHI::QueryResultFlagBits::Wait);
            }
            ImGui::Text("Occlusion Result: %llu", static_cast<unsigned long long>(occlusionResults));
        }

        if(m_timestampEnabled)
        {
            // This is the oldest pair of timestamp queries (3 frames ago).
            size_t beginIndex = (m_currentTimestampQueryIndex + 2) % m_timestampQueries.size();
            uint64_t timestamps[2] = {};
            if (m_timestampQueries[beginIndex].m_isValid && m_timestampQueries[beginIndex + 1].m_isValid)
            {
                AZ::RHI::MultiDeviceQuery* queries[] = { m_timestampQueries[beginIndex].m_query.get() , m_timestampQueries[beginIndex + 1].m_query.get() };
                m_timeStampQueryPool->GetResults(queries, 2, timestamps, 2, AZ::RHI::QueryResultFlagBits::Wait);
            }
            
            if (ImGui::CollapsingHeader("Timestamp", ImGuiTreeNodeFlags_DefaultOpen))
            {
                AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
                auto durationInMicroseconds = device->GpuTimestampToMicroseconds(timestamps[1] - timestamps[0], AZ::RHI::HardwareQueueClass::Graphics);
                ImGui::Indent();
                ImGui::Text("Draw time: %llu us", static_cast<unsigned long long>(durationInMicroseconds.count()));
                ImGui::Unindent();
            }            
        }

        if (m_pipelineStatisticsEnabled)
        {
            AZStd::vector<uint64_t> statistics;
            uint32_t statisticsCount = AZ::RHI::CountBitsSet(static_cast<uint64_t>(m_statisticsQueryPool->GetDescriptor().m_pipelineStatisticsMask));
            statistics.resize(statisticsCount);
            size_t queryFrameIndex = (m_currentStatisticsQueryIndex + 1) % m_statisticsQueries.size();
            if (m_statisticsQueries[queryFrameIndex].m_isValid)
            {
                m_statisticsQueryPool->GetResults(m_statisticsQueries[queryFrameIndex].m_query.get(), statistics.data(), statisticsCount, AZ::RHI::QueryResultFlagBits::Wait);
            }

            const char* statisticsName[] =
            {
                "IAVertices",
                "IAPrimitives",
                "VSInvocations",
                "CInvocations",
                "CPrimitives",
                "PSInvocations"
            };

            if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                for (size_t i = 0; i < statistics.size(); ++i)
                {
                    ImGui::Text("%s: %llu\n", statisticsName[i], static_cast<unsigned long long>(statistics[i]));
                }
                ImGui::Unindent();
            }
        }
        m_imguiSidebar.End();
    }

} // namespace AtomSampleViewer
