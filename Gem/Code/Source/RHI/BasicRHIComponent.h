/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once
#include <AtomSampleComponent.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/RenderPass.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/ScopeProducer.h>
#include <Atom/RHI.Reflect/Format.h>

#include <AtomCore/Instance/InstanceId.h>

#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>

namespace AZ
{
    class AssetCollectionAsyncLoader;

    namespace RHI
    {
        class Device;
    }
    namespace RPI
    {
        class PipelineStateCache;
    }
}

namespace AtomSampleViewer
{
    class BasicRHIComponent;

    // Using RenderPass instead of Pass so we can have a scope to clear RenderTarget
    class RHISamplePass
        : public AZ::RPI::RenderPass
    {
        using Base = AZ::RPI::RenderPass;
        AZ_RPI_PASS(RHISamplePass);
    public:
        AZ_RTTI(RHISamplePass, "{7F70BF4C-F5B3-453A-AEC4-3F5599BAFC16}", Pass);
        AZ_CLASS_ALLOCATOR(RHISamplePass, AZ::SystemAllocator, 0);

        virtual ~RHISamplePass();

        //! Creates a new pass without a PassTemplate
        static AZ::RPI::Ptr<RHISamplePass> Create(const AZ::RPI::PassDescriptor& descriptor);

        void SetRHISample(BasicRHIComponent* sample);

        // Pass overrides
        const AZ::RPI::PipelineViewTag& GetPipelineViewTag() const override;

        // Return the view index of the pass
        uint32_t GetViewIndex() const;
        void SetViewIndex(const uint32_t viewIndex);

    protected:
        explicit RHISamplePass(const AZ::RPI::PassDescriptor& descriptor);

        // --- Pass Behaviour Overrides ---
        void BuildInternal() override; // build attachment
        void FrameBeginInternal(FramePrepareParams params) override; // import scopes

        BasicRHIComponent* m_rhiSample = nullptr;

        AZ::RPI::Ptr<AZ::RPI::PassAttachment> m_outputAttachment;

        AZ::RPI::PipelineViewTag m_pipelineViewTag;

        // Used to determine view index for XR sample
        uint32_t m_viewIndex = 0;
    };

    class BasicRHIComponent
        : public AtomSampleComponent
        , public AZ::RHI::RHISystemNotificationBus::Handler
    {
        friend class RHISamplePass;

    public:
        AZ_RTTI(BasicRHIComponent, "{FAB340E4-2D91-48CD-A7BC-81ED25721415}", AtomSampleComponent);

        BasicRHIComponent() = default;
        ~BasicRHIComponent() override = default;

        // Creates a 3D image from 2D images. All 2D images are required to have the same format and layout
        static void CreateImage3dData(
            AZStd::vector<uint8_t>& data,
            AZ::RHI::DeviceImageSubresourceLayout& layout,
            AZ::RHI::Format& format,
            AZStd::vector<const char*>&& imageAssetPaths);

        void SetOutputInfo(uint32_t width, uint32_t height, AZ::RHI::Format format, AZ::RHI::AttachmentId attachmentId);

        bool IsSupportedRHISamplePipeline();

        float GetViewportWidth();
        
        float GetViewportHeight();
        
        void SetViewIndex(const uint32_t viewIndex);
       
    protected:
        AZ_DISABLE_COPY(BasicRHIComponent);

        struct VertexPosition;
        struct VertexColor;
        struct VertexU;
        struct VertexUV;
        struct VertexUVW;
        struct VertexUVWX;
        struct VertexNormal;

        // Component
        virtual void Activate() override = 0;
        virtual void Deactivate() override = 0;
        virtual bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;

        // RHISystemNotificationBus::Handler
        virtual void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) override;

        // virtual function which is called by OnFramePrepare
        virtual void FrameBeginInternal([[maybe_unused]] AZ::RHI::FrameGraphBuilder& frameGraphBuilder) {};

        // Buffer Setting
        void SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, float x, float y, float z);
        void SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, const AZ::Vector3& position);
        void SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, const VertexPosition& position);

        void SetVertexColor(VertexColor* colorBuffer, int bufferIndex, float r, float g, float b, float a);
        void SetVertexColor(VertexColor* colorBuffer, int bufferIndex, const AZ::Vector4& color);
        void SetVertexColor(VertexColor* colorBuffer, int bufferIndex, const VertexColor& color);

        void SetVertexIndex(uint16_t* indexBuffer, int bufferIndex, const uint16_t index);

        void SetVertexU(VertexU* uBuffer, int bufferIndex, float u);
        void SetVertexUV(VertexUV* uvBuffer, int bufferIndex, float u, float v);
        void SetVertexUVW(VertexUVW* uvwBuffer, int bufferIndex, float u, float v, float w);
        void SetVertexUVW(VertexUVW* uvwBuffer, int bufferIndex, const AZ::Vector3& uvw);
        void SetVertexUVWX(VertexUVWX* uvwxBuffer, int bufferIndex, float u, float v, float w, float x);

        void SetVertexNormal(VertexNormal* normalBuffer, int bufferIndex, float nx, float ny, float nz);
        void SetVertexNormal(VertexNormal* normalBuffer, int bufferIndex, const AZ::Vector3& normal);

        void SetVertexIndexIncreasing(uint16_t* indexBuffer, size_t bufferSize);
        void SetFullScreenRect(VertexPosition* positionBuffer, VertexUV* uvBuffer, uint16_t* indexBuffer);
        void SetCube(VertexPosition* positionBuffer, VertexColor* colorBuffer, VertexNormal* normalBuffer, uint16_t* indexBuffer);

        AZ::Matrix4x4 CreateViewMatrix(AZ::Vector3 eye, AZ::Vector3 up, AZ::Vector3 lookAt);

        // Shader
        AZ::Data::Instance<AZ::RPI::Shader> LoadShader(const char* shaderFilePath, const char* sampleName, const AZ::Name* supervariantName = nullptr);
        AZ::Data::Instance<AZ::RPI::Shader> LoadShader(const AZ::AssetCollectionAsyncLoader& assetLoadMgr, const char* shaderFilePath, const char* sampleName);
        static AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> CreateShaderResourceGroup(AZ::Data::Instance<AZ::RPI::Shader> shader, const char* shaderResourceGroupId, const char* sampleName);

        void FindShaderInputIndex(
            AZ::RHI::ShaderInputConstantIndex* shaderInputConstIndex,
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
            const AZ::Name& shaderInputName,
            const char* componentName
        );

        void FindShaderInputIndex(
            AZ::RHI::ShaderInputImageIndex* shaderInputImageIndex,
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
            const AZ::Name& shaderInputName,
            const char* componentName
        );

        void FindShaderInputIndex(
            AZ::RHI::ShaderInputBufferIndex* shaderInputBufferIndex,
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
            const AZ::Name& shaderInputName,
            const char* componentName
        );

        void FindShaderInputIndex(
            AZ::RHI::ShaderInputSamplerIndex* shaderInputSamplerIndex,
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
            const AZ::Name& shaderInputName,
            const char* componentName
        );

        // set output info from m_windowContext
        void SetOutputInfoFromWindowContext();

        // setup viewport and scissor from output width and height
        void UpdateViewportAndScissor();

        // Texture
        AZ::Data::Instance<AZ::RPI::StreamingImage> LoadStreamingImage(const char* textureFilePath, const char* sampleName);

        // Input Assembly Data
        struct VertexPosition
        {
            float m_position[3];
        };
        struct VertexColor
        {
            float m_color[4];
        };
        struct VertexNormal
        {
            float m_normal[3];
        };
        struct VertexU
        {
            float m_u;
        };
        struct VertexUV
        {
            float m_uv[2];
        };
        struct VertexUVW
        {
            float m_uvw[3];
        };
        struct VertexUVWX
        {
            float m_uvwx[4];
        };

        // Variables
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZStd::vector<AZStd::shared_ptr<AZ::RHI::ScopeProducer>> m_scopeProducers;

        // The output (render target) info
        uint32_t m_outputWidth = 1920;
        uint32_t m_outputHeight = 1080;
        AZ::RHI::Format m_outputFormat;
        AZ::RHI::AttachmentId m_outputAttachmentId;
        AZ::RHI::Viewport m_viewport;
        AZ::RHI::Scissor m_scissor;

        // whether this sample supports RHI sample render pipeline or not
        bool m_supportRHISamplePipeline = false;
        
        // view index. Used by XR related samples
        uint32_t m_viewIndex = 0;
    };
} // namespace AtomSampleViewer
