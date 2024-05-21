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

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLod.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/RHISystemInterface.h>

#include <AzFramework/Entity/EntityContextBus.h>

#include <RHI/BasicRHIComponent.h>
#include <Utils/ImGuiSidebar.h>
#include <ExampleComponentBus.h>

namespace AtomSampleViewer
{
    //! Example that showcases the use of SubpassInputs.
    //! The example implements a simple deferred renderer with two passes.
    //! The first one is a Gbuffer pass that writes to 3 render targets: position, normal and albedo.
    //! The second pass is the composition one that reads from the 3 previous render targets as subpassInputs
    //! and then outputs the correct lighting of the scene to the swapchain.
    class SubpassExampleComponent final
        : public BasicRHIComponent
        , public ExampleComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(SubpassExampleComponent, "{931B2CA5-5718-4DED-B2BE-262DEA0586B5}", AZ::Component);
        AZ_DISABLE_COPY(SubpassExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        SubpassExampleComponent();
        ~SubpassExampleComponent() override = default;

    private:

        // Scope types
        enum InputAttachmentScopes
        {
            GBufferScope,
            CompositionScope,
            NumScopes
        };

        enum ModelType
        {
            ModelType_Plane = 0,
            ModelType_ShaderBall,
            ModelType_Bunny,
            ModelType_Suzanne,
            ModelType_Count
        };

        struct ModelData
        {
            AZ::RPI::ModelLod::StreamBufferViewList m_streamBufferList;
            AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_shaderResourceGroup;
            ModelType m_modelType = ModelType_ShaderBall;
        };

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // ExampleComponentRequestBus::Handler
        void ResetCamera() override;

        // AZ::Component
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;
        
        void LoadModels();
        void LoadShaders();
        void CreateShaderResourceGroups();
        void CreatePipelines();
        void CreateGBufferScope();
        void CreateCompositionScope();
        void SetupScene();

        AZStd::array<AZ::Data::Instance<AZ::RPI::Model>, ModelType_Count> m_models;
        AZStd::vector<ModelData> m_opaqueModelsData;
        uint32_t m_meshCount = 0;

        AZStd::vector<AZ::Data::Instance<AZ::RPI::Shader>> m_shaders;

        AZ::RHI::AttachmentId m_albedoAttachmentId;
        AZ::RHI::AttachmentId m_normalAttachmentId;
        AZ::RHI::AttachmentId m_positionAttachmentId;
        AZ::RHI::AttachmentId m_depthStencilAttachmentId;

        AZ::EntityId m_cameraEntityId;
        AzFramework::EntityContextId m_entityContextId;

        // Camera projection matrix
        AZ::Matrix4x4 m_projectionMatrix;
        AZ::RHI::ShaderInputConstantIndex m_modelMatrixIndex;
        AZ::RHI::ShaderInputConstantIndex m_lightsInfoIndex;

        AZ::RHI::ShaderInputImageIndex m_subpassInputPosition;
        AZ::RHI::ShaderInputImageIndex m_subpassInputNormal;
        AZ::RHI::ShaderInputImageIndex m_subpassInputAlbedo;

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_sceneShaderResourceGroup;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_compositionSubpassInputsSRG;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_viewShaderResourceGroup;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_compositionPipeline;
    };
} // namespace AtomSampleViewer
