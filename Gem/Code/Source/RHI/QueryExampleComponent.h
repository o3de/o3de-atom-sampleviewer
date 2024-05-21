/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ScopeId.h>

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/QueryPool.h>
#include <Atom/RHI/Query.h>
#include <Atom/RHI/StreamBufferView.h>

#include <RHI/BasicRHIComponent.h>
#include <ExampleComponentBus.h>
#include <Utils/ImGuiSidebar.h>

namespace AZ
{
    namespace RHI
    {
        class FrameGraphBuilder;
    }
}

namespace AtomSampleViewer
{
    /*
    * QueryPool RHI example.
    * Test use of QueryPools for occlusion queries, predication, timestamp information and pipeline
    * statistics.
    * Three quads are rendered in the following order:
    * First: a quad in the back that may be occluded.
    * Second: a quad in the front that may occlude the one in the back.
    * Third: a bounding box quad that represent the first quad. This quad doesn't write to the rendertargets
    *        and is used to write to the current occlusion query.
    *
    * Predication is available for platforms that support it. A separate scope is in charge of copying the data
    * from the QueryPool to a buffer that is used later for predication.
    * A sidebar is used for enable/disable the different options.
    */
    class QueryExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(QueryExampleComponent, "{5ECAF824-8D1A-49CB-86D6-42511865638D}", AZ::Component);
        AZ_DISABLE_COPY(QueryExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        QueryExampleComponent();
        ~QueryExampleComponent() override = default;

    protected:
        enum class QueryType : uint32_t
        {
            Occlusion,
            Predication
        };

        // AZ::Component overrides ...
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler ...
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // AZ::TickBus::Handler ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        void CreateGeometryResources();
        void CreateShaderResources();
        void CreateCopyBufferScopeProducer();
        void CreateScopeProducer();
        void CreateQueryResources();
        void CreatePredicationResources();

        void SetQueryType(QueryType type);
        void DrawSampleSettings();

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_quadInputAssemblyBuffer;

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<uint16_t, 6> m_indices;
        };
        AZ::RHI::StreamBufferView m_quadStreamBufferView;
        AZ::RHI::InputStreamLayout m_quadInputStreamLayout;

        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, 3> m_shaderResourceGroups;
        AZ::RHI::ShaderInputConstantIndex m_objectMatrixConstantIndex;
        AZ::RHI::ShaderInputConstantIndex m_colorConstantIndex;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_quadPipelineState;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_boudingBoxPipelineState;

        AZ::RHI::Ptr<AZ::RHI::QueryPool> m_occlusionQueryPool;
        AZ::RHI::Ptr<AZ::RHI::QueryPool> m_timeStampQueryPool;
        AZ::RHI::Ptr<AZ::RHI::QueryPool> m_statisticsQueryPool;

        struct QueryEntry
        {
            AZ::RHI::Ptr<AZ::RHI::Query> m_query;
            bool m_isValid = false; // Indicates if the query has been written at least once.
        };

        AZStd::array<QueryEntry, AZ::RHI::Limits::Device::FrameCountMax> m_occlusionQueries;
        AZStd::array<QueryEntry, AZ::RHI::Limits::Device::FrameCountMax * 2> m_timestampQueries;
        AZStd::array<QueryEntry, AZ::RHI::Limits::Device::FrameCountMax> m_statisticsQueries;
        uint32_t m_currentOcclusionQueryIndex = 0;
        uint32_t m_currentTimestampQueryIndex = 0;
        uint32_t m_currentStatisticsQueryIndex = 0;

        QueryType m_currentType = QueryType::Occlusion;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_predicationBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_predicationBuffer;
        AZ::RHI::BufferScopeAttachmentDescriptor m_predicationBufferAttachmentDescriptor;

        bool m_timestampEnabled = false;
        bool m_pipelineStatisticsEnabled = false;
        bool m_precisionOcclusionEnabled = false;

        float m_elapsedTime = 0.0f;

        ImGuiSidebar m_imguiSidebar;
    };
} // namespace AtomSampleViewer
