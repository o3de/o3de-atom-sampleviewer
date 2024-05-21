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
#include <AzCore/Math/Color.h>
#include <AzCore/std/containers/array.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/ShaderReloadNotificationBus.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/CopyItem.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/IndirectBufferSignature.h>
#include <Atom/RHI/IndirectBufferWriter.h>
#include <Atom/RHI/PipelineState.h>

#include <Atom/RHI.Reflect/IndirectBufferLayout.h>

#include <RHI/BasicRHIComponent.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiProgressList.h>
#include <Atom/Utils/AssetCollectionAsyncLoader.h>

namespace AtomSampleViewer
{
    //! This samples demonstrates the use of indirect rendering.
    //! It uses an indirect buffer that contains a list of commands
    //! to be executed by the GPU in an indirect manner.
    //! The commands are generated at initialization by the CPU. Each frame
    //! a compute shader culls the commands that are outside a designated area
    //! and the remaining commands are draw using indirect calls.
    //! The sample has two user control variables:
    //! - The number of primitives to render
    //! - The cull area.
    //!
    //! Depending on the capabilities of the platform the sample can run the following indirect commands:
    //! 1) - Inline Constants Command
    //!    - Set Vertex View Command
    //!    - Set Index View Command
    //!    - Draw Indexed Command
    //!
    //! 2) - Draw Indexed command
    //!
    //! Depending on capabilities, the samples may use a count buffer.
    //! The number of indirect draw calls is also determinate by device capabilities.
    //!
    class IndirectRenderingExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
        , public AzFramework::WindowNotificationBus::Handler
        , public AZ::RPI::ShaderReloadNotificationBus::MultiHandler
    {
    public:
        AZ_COMPONENT(IndirectRenderingExampleComponent, "{B799BEFC-52AF-47FF-A806-29E38A5BDF12}", AZ::Component);
        AZ_DISABLE_COPY(IndirectRenderingExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        IndirectRenderingExampleComponent();
        ~IndirectRenderingExampleComponent() override = default;

        static constexpr char LogName[] = "IndirectRenderingExampleComponent";
        static constexpr char IndirectDrawShaderFilePath[] = "Shaders/RHI/IndirectDraw.azshader";
        static constexpr char IndirectDispatchShaderFilePath[] = "Shaders/RHI/IndirectDispatch.azshader";
        static constexpr char IndirectDrawVariantLabel[] = "IndirectDraw variant";
        static constexpr char IndirectDispatchVariantLabel[] = "IndirectDispatch variant";

    private:
        /// Max number of objects to render.
        static const uint32_t s_maxNumberOfObjects = 4096;

        /// Data to be use for Input Assembly.
        struct BufferData
        {
            AZStd::array<VertexPosition, 3> m_trianglePositions;
            AZStd::array<VertexPosition, 4> m_quadPositions;
            AZStd::array<uint16_t, 3> m_triangleIndices;
            AZStd::array<uint16_t, 6> m_quadIndices;
            AZStd::array<uint32_t, s_maxNumberOfObjects> m_instanceIndices;
        };

        /// Data specific to an object.
        struct InstanceData
        {
            AZ::Color m_color;
            AZ::Vector4 m_offset;
            AZ::Vector4 m_scale;
            AZ::Vector4 m_velocity;
        };

        struct ScopeData
        {
            //UserDataParam - Empty for this samples
        };

        /// The indirect commands that the indirect buffer will contain.
        enum class SequenceType: uint8_t
        {
            DrawOnly = 0,           // The buffer will contain only a draw indexed command.
            IARootConstantsDraw,   // The buffer will contain a set vertex buffer, set index buffer, set
                                    // root constants and a draw indexed command.
            Count
        };

        static const uint32_t NumSequencesType = static_cast<uint32_t>(SequenceType::Count);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time);

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // AzFramework::WindowNotificationBus::Handler
        void OnWindowResized(uint32_t width, uint32_t height) override;

        ///////////////////////////////////////////////////////////////////////
        // ShaderReloadNotificationBus overrides
        void OnShaderVariantReinitialized(const AZ::RPI::ShaderVariant& shaderVariant) override;
        ///////////////////////////////////////////////////////////////////////

        void InitRequestsForShaderVariants();
        void OnAllAssetsReadyActivate();
        void InitInputAssemblyResources();
        void InitShaderResources();
        void InitIndirectRenderingResources();
        void InitInstancesDataResources();
        void CreateResetCounterBufferScope();
        void CreateCullingScope();
        void CreateDrawingScope();
        void DrawSampleSettings();
        void UpdateInstancesData(float deltaTime);
        void UpdateIndirectDispatchArguments();

        AZ::RHI::InputStreamLayout m_inputStreamLayout;

        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_inputAssemblyBufferPool;
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_shaderBufferPool;
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_instancesBufferPool;
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_copyBufferPool;

        AZ::RHI::Ptr<AZ::RHI::Buffer> m_inputAssemblyBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_sourceIndirectBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_instancesDataBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_resetCounterBuffer;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_indirectDispatchBuffer;

        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_drawPipelineState;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_cullPipelineState;

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_sceneShaderResourceGroup;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_cullShaderResourceGroup;

        AZStd::array<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, NumSequencesType> m_indirectCommandsShaderResourceGroups;

        AZStd::array<AZ::RHI::StreamBufferView, 3> m_streamBufferViews;
        AZStd::array<AZ::RHI::IndexBufferView, 2> m_indexBufferViews;
        AZ::RHI::IndirectBufferView m_indirectDrawBufferView;
        AZ::RHI::IndirectBufferView m_indirectDispatchBufferView;

        AZ::RHI::Ptr<AZ::RHI::BufferView> m_sourceIndirectBufferView;
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_instancesDataBufferView;

        AZ::RHI::IndirectBufferLayout m_indirectDrawBufferLayout;
        AZ::RHI::IndirectBufferLayout m_indirectDispatchBufferLayout;
        AZ::RHI::Ptr<AZ::RHI::IndirectBufferSignature> m_indirectDrawBufferSignature;
        AZ::RHI::Ptr<AZ::RHI::IndirectBufferSignature> m_indirectDispatchBufferSignature;

        AZ::RHI::ShaderInputBufferIndex m_sceneInstancesDataBufferIndex;
        AZ::RHI::ShaderInputConstantIndex m_sceneMatrixInputIndex;
        AZ::RHI::ShaderInputBufferIndex m_cullingCountBufferIndex;
        AZ::RHI::ShaderInputConstantIndex m_cullingOffsetIndex;
        AZ::RHI::ShaderInputConstantIndex m_cullingNumCommandsIndex;
        AZ::RHI::ShaderInputConstantIndex m_cullingMaxCommandsIndex;

        AZStd::array<AZ::RHI::ShaderInputBufferIndex, NumSequencesType> m_cullingInputIndirectBufferIndices;
        AZStd::array<AZ::RHI::ShaderInputBufferIndex, NumSequencesType> m_cullingOutputIndirectBufferIndices;

        AZ::RHI::Ptr<AZ::RHI::IndirectBufferWriter> m_indirectDispatchWriter;

        AZ::RHI::MultiDeviceDrawIndirect m_drawIndirect;
        AZ::RHI::CopyBufferDescriptor m_copyDescriptor;

        ImGuiSidebar m_imguiSidebar;
        float m_cullOffset = 1.0f;

        uint32_t m_numObjects = 0;

        AZStd::vector<InstanceData> m_instancesData;

        SequenceType m_mode = SequenceType::DrawOnly;
        bool m_updateIndirectDispatchArguments = false;
        bool m_deviceSupportsCountBuffer = false;

        // REMARK: This example requires static shader options to run properly.
        // We use notifications from ShaderVariantFinderNotificationBus & this flag to get the
        // green light to proceed into full activation.
        bool m_doneLoadingShaders = false;
        AZStd::unique_ptr<AZ::AssetCollectionAsyncLoader> m_assetLoadManager;
        ImGuiProgressList m_imguiProgressList;

        AZ::Data::Instance<AZ::RPI::Shader> m_indirectDrawShader;
        AZ::RPI::ShaderOptionGroup m_indirectDrawShaderOptionGroup;
        AZ::RPI::ShaderVariantStableId m_indirectDrawShaderVariantStableId;

        AZ::Data::Instance<AZ::RPI::Shader> m_indirectDispatchShader;
        AZ::RPI::ShaderOptionGroup m_indirectDispatchShaderOptionGroup;
        AZ::RPI::ShaderVariantStableId m_indirectDispatchShaderVariantStableId;
    };
} // namespace AtomSampleViewer
