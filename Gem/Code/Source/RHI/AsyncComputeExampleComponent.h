/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/ScopeProducer.h>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLod.h>

#include <Atom/RPI.Reflect/ResourcePoolAssetCreator.h>
#include <Atom/Utils/AssetCollectionAsyncLoader.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <RHI/BasicRHIComponent.h>
#include <ExampleComponentBus.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiProgressList.h>

namespace AtomSampleViewer
{
    //! This sample demostrates the use of async compute.
    //! It applies a simple tonemapping post processing. 
    //! In order to get better scheduling between graphic and compute work, the sample
    //! uses two scene textures, one from the previous frame and one from the current one.
    //! Tonemapping is applied to the previous frame scene image.
    //! The tonemapping scope runs on a compute shader in the compute queue.
    //! The average luminance calculation used for the tonemapping also runs in a compute shader in the compute queue.
    //! There's also a shadow generation and luminance map scopes.
    //! [GFX TODO][ATOM-2034] Add scope synchronization once it's supported in the RHI.
    //!
    //!                                                Next / Previous Frame
    //!                       +--------------------------------------------------------------------+
    //!                       |                                                                    |
    //!                       |      +--------------+        +----------------+       +-------+    |     +---------------+
    //!   Graphics Queue      +----->| LuminanceMap |---+    |ShadowGeneration|------>|Forward|----+     |CopyToSwapchain|------> Present
    //!                              +-------------++   |    +----------------+       +-------+          +-------+-------+
    //!                                                 |                                                        ^
    //!                                                 |                                                        |
    //!                                                 |    +---------------+        +-----------+              |
    //!   Compute Queue                                 +--->|LuminanceReduce|------->|Tonemapping|--------------+
    //!                                                      +---------------+        +-----------+
    //!
    //!
    class AsyncComputeExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
        , public ExampleComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(AsyncComputeExampleComponent, "{782EA426-AB4A-4F4F-A296-775EE350FF0A}", AZ::Component);
        AZ_DISABLE_COPY(AsyncComputeExampleComponent);

        static void Reflect(AZ::ReflectContext* context);

        AsyncComputeExampleComponent();
        ~AsyncComputeExampleComponent() = default;

    protected:
        struct ScopeData
        {
        };

        struct NumThreadsCS
        {
            int m_X = 8;
            int m_Y = 8;
            int m_Z = 1;
        };

        // BasicRHIComponent overrides...
        void Activate() override;
        void Deactivate() override;
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;
        void FrameBeginInternal(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // ExampleComponentRequestBus::Handler
        void ResetCamera() override;

        void OnAllAssetsReadyActivate();
        void CreateSceneRenderTargets();
        void CreateQuad();
        void LoadShaders();
        void LoadModel();
        void CreatePipelines();
        void SetupScene();
        void SetArcBallControllerParams();
        void CreateCopyTextureScope();
        void CreateShadowScope();
        void CreateForwardScope();
        void CreateTonemappingScope();
        void CreateLuminanceMapScope();
        void CreateLuminanceReduceScopes();

        // Scope types
        enum AsyncComputeScopes
        {
            CopyTextureScope = 0, // Copy content back to the swapchain (R16G16B16A16 to Swapchain format conversion).
            ShadowScope, // Generates shadow map.
            ForwardScope, // Renders the scene.
            LuminanceMapScope, // Creates the luminance map from the scene image.
            LuminanceReduceScope, // Reduce luminance map to a 1x1 image.
            TonemappingScope, // Appplies tonemapping.
            NumScopes
        };
       
        // Quad related variables
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_quadBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_quadInputAssemblyBuffer;
        AZ::RHI::IndexBufferView m_quadIndexBufferView;
        AZStd::array<AZStd::vector<AZ::RHI::StreamBufferView>, NumScopes> m_quadStreamBufferViews;

        // Terrain related variables
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, NumScopes> m_terrainPipelineStates;

        // Model related variables
        AZStd::array<AZ::RPI::ModelLod::StreamBufferViewList, NumScopes> m_modelStreamBufferViews;
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::PipelineState>, NumScopes> m_modelPipelineStates;
        AZ::Data::Instance<AZ::RPI::Model> m_model;

        // Copy Texture related variables
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_copyTexturePipelineState;

        // Luminance map related variables
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_luminancePipelineState;
        
        // Luminance reduce related variables
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_luminanceReducePipelineState;

        // Tonemapping related variables
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_tonemappingPipelineState;

        // Camera projection matrix
        AZ::Matrix4x4 m_projectionMatrix;

        AZStd::array<AZStd::vector<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>>, NumScopes> m_shaderResourceGroups;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_viewShaderResourceGroup;
        AZ::RHI::ShaderInputImageIndex m_shaderInputImageIndex;
        AZ::RHI::ShaderInputImageIndex m_copyTextureShaderInputImageIndex;
        AZ::RHI::ShaderInputImageIndex m_luminanceShaderInputImageIndex;
        AZ::RHI::ShaderInputImageIndex m_luminanceReduceShaderInputImageIndex;
        AZ::RHI::ShaderInputImageIndex m_luminanceReduceShaderOutputImageIndex;
        AZ::RHI::ShaderInputImageIndex m_tonemappingShaderImageIndex;
        AZ::RHI::ShaderInputImageIndex m_tonemappingLuminanceImageIndex;
        AZ::EntityId m_cameraEntityId;

        AZStd::array<AZ::Data::Instance<AZ::RPI::Shader>, NumScopes> m_shaders;
        AZStd::array<NumThreadsCS, NumScopes> m_numThreads;

        // Scene images
        static constexpr uint32_t NumSceneImages = 2;
        AZStd::array<AZ::RHI::AttachmentId, NumSceneImages> m_sceneIds = { { AZ::RHI::AttachmentId("SceneId1"), AZ::RHI::AttachmentId("SceneId2") } };
        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_imagePool;
        AZStd::array<AZ::RHI::Ptr<AZ::RHI::Image>, NumSceneImages> m_sceneImages;
        uint32_t m_currentSceneImageIndex = 0;
        uint32_t m_previousSceneImageIndex = 1;

        ImGuiSidebar m_imguiSidebar;
        bool m_asyncComputeEnabled = true;

        AZ::RHI::AttachmentId m_forwardDepthStencilId;
        AZ::RHI::AttachmentId m_shadowAttachmentId;
        AZ::RHI::AttachmentId m_luminanceMapAttachmentId;
        AZ::RHI::AttachmentId m_averageLuminanceAttachmentId;

        AZStd::unique_ptr<AZ::AssetCollectionAsyncLoader> m_assetLoadManager;
        bool m_fullyActivated = false;
        ImGuiProgressList m_imguiProgressList;
    };
}
