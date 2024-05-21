/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/BasicRHIComponent.h>

#include <SampleComponentConfig.h>

#include <Atom/Utils/AssetCollectionAsyncLoader.h>

namespace AtomSampleViewer
{
    AZ::RPI::Ptr<RHISamplePass> RHISamplePass::Create(const AZ::RPI::PassDescriptor& descriptor)
    {
        AZ::RPI::Ptr<RHISamplePass> pass = aznew RHISamplePass(descriptor);
        return AZStd::move(pass);
    }


    RHISamplePass::RHISamplePass(const AZ::RPI::PassDescriptor& descriptor)
        : AZ::RPI::RenderPass(descriptor)
    {
        m_flags.m_bindViewSrg = true;
        m_pipelineViewTag = "MainCamera";
    }

    RHISamplePass::~RHISamplePass()
    {
        m_rhiSample = nullptr;
    }

    const AZ::RPI::PipelineViewTag& RHISamplePass::GetPipelineViewTag() const
    {
        return m_pipelineViewTag;
    }

    void RHISamplePass::SetRHISample(BasicRHIComponent* sample)
    {
        m_rhiSample = sample;
        if (m_rhiSample)
        {
            m_rhiSample->SetOutputInfo(m_outputAttachment->m_descriptor.m_image.m_size.m_width
                , m_outputAttachment->m_descriptor.m_image.m_size.m_height
                , m_outputAttachment->m_descriptor.m_image.m_format
                , m_outputAttachment->GetAttachmentId());
        }
    }

    void RHISamplePass::BuildInternal()
    {
        Base::BuildInternal();

        // The RHISamplePass template should have one owned image attachment which is the render target
        m_outputAttachment = m_ownedAttachments[0];

        // Force update pass attachment to get correct size and save it to local variables
        m_outputAttachment->Update();

        // update output info for the rhi sample
        if (m_rhiSample)
        {
            SetRHISample(m_rhiSample);
        }
    }

    void RHISamplePass::FrameBeginInternal(FramePrepareParams params)
    {
        // Note: we can't preview the output attachment of this pass properly
        // Ideally, we should import RHISamplePass's scope producer first, then rhi sample's scope producers
        // then add readback scope and preview copy scope.
        // We can't do it since some of them private to RenderPass which we can't access here. 

        // Import this pass's scope producer
        Base::FrameBeginInternal(params);

        // Import sample's scope producer
        if (m_rhiSample)
        {
            auto frameGraphBuilder = params.m_frameGraphBuilder;

            m_rhiSample->FrameBeginInternal(*frameGraphBuilder);
            for (AZStd::shared_ptr<AZ::RHI::ScopeProducer>& producer : m_rhiSample->m_scopeProducers)
            {
                frameGraphBuilder->ImportScopeProducer(*producer);
            }
        }
    }

    uint32_t RHISamplePass::GetViewIndex() const
    {
        return m_viewIndex;
    }

    void RHISamplePass::SetViewIndex(const uint32_t viewIndex)
    {
        m_viewIndex = viewIndex;
    }

    bool BasicRHIComponent::ReadInConfig(const AZ::ComponentConfig* baseConfig)
    {
        using namespace AZ;

        const auto config = azrtti_cast<const SampleComponentConfig*>(baseConfig);

        if (config != nullptr && config->m_windowContext)
        {
            m_windowContext = config->m_windowContext;
            return true;
        }

        return false;
    }

    void BasicRHIComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        // Check the swap chain since the system may still tick one more frame when window is closed and swapchain got destroied. 
        if (!IsSupportedRHISamplePipeline() && m_windowContext->GetSwapChain())
        {
            SetOutputInfoFromWindowContext();

            FrameBeginInternal(frameGraphBuilder);

            for (AZStd::shared_ptr<AZ::RHI::ScopeProducer>& producer : m_scopeProducers)
            {
                frameGraphBuilder.ImportScopeProducer(*producer);
            }
        }
    }

    void BasicRHIComponent::SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, float x, float y, float z)
    {
        positionBuffer[bufferIndex].m_position[0] = x;
        positionBuffer[bufferIndex].m_position[1] = y;
        positionBuffer[bufferIndex].m_position[2] = z;
    }

    void BasicRHIComponent::SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, const AZ::Vector3& position)
    {
        positionBuffer[bufferIndex].m_position[0] = position.GetX();
        positionBuffer[bufferIndex].m_position[1] = position.GetY();
        positionBuffer[bufferIndex].m_position[2] = position.GetZ();
    }

    void BasicRHIComponent::SetVertexPosition(VertexPosition* positionBuffer, int bufferIndex, const VertexPosition& position)
    {
        positionBuffer[bufferIndex].m_position[0] = position.m_position[0];
        positionBuffer[bufferIndex].m_position[1] = position.m_position[1];
        positionBuffer[bufferIndex].m_position[2] = position.m_position[2];
    }

    void BasicRHIComponent::SetVertexColor(VertexColor* colorBuffer, int bufferIndex, float r, float g, float b, float a)
    {
        colorBuffer[bufferIndex].m_color[0] = r;
        colorBuffer[bufferIndex].m_color[1] = g;
        colorBuffer[bufferIndex].m_color[2] = b;
        colorBuffer[bufferIndex].m_color[3] = a;
    }

    void BasicRHIComponent::SetVertexColor(VertexColor* colorBuffer, int bufferIndex, const AZ::Vector4& color)
    {
        colorBuffer[bufferIndex].m_color[0] = color.GetX();
        colorBuffer[bufferIndex].m_color[1] = color.GetY();
        colorBuffer[bufferIndex].m_color[2] = color.GetZ();
        colorBuffer[bufferIndex].m_color[3] = color.GetW();
    }

    void BasicRHIComponent::SetVertexColor(VertexColor* colorBuffer, int bufferIndex, const VertexColor& color)
    {
        colorBuffer[bufferIndex].m_color[0] = color.m_color[0];
        colorBuffer[bufferIndex].m_color[1] = color.m_color[1];
        colorBuffer[bufferIndex].m_color[2] = color.m_color[2];
        colorBuffer[bufferIndex].m_color[3] = color.m_color[3];
    }

    void BasicRHIComponent::SetVertexIndex(uint16_t* indexBuffer, int bufferIndex, const uint16_t index)
    {
        indexBuffer[bufferIndex] = index;
    }

    void BasicRHIComponent::SetVertexU(VertexU* uBuffer, int bufferIndex, float u)
    {
        uBuffer[bufferIndex].m_u = u;
    }

    void BasicRHIComponent::SetVertexUV(VertexUV* uvBuffer, int bufferIndex, float u, float v)
    {
        uvBuffer[bufferIndex].m_uv[0] = u;
        uvBuffer[bufferIndex].m_uv[1] = v;
    }

    void BasicRHIComponent::SetVertexUVW(VertexUVW* uvwBuffer, int bufferIndex, float u, float v, float w)
    {
        uvwBuffer[bufferIndex].m_uvw[0] = u;
        uvwBuffer[bufferIndex].m_uvw[1] = v;
        uvwBuffer[bufferIndex].m_uvw[2] = w;
    }

    void BasicRHIComponent::SetVertexUVW(VertexUVW* uvwBuffer, int bufferIndex, const AZ::Vector3& uvw)
    {
        uvwBuffer[bufferIndex].m_uvw[0] = uvw.GetX();
        uvwBuffer[bufferIndex].m_uvw[1] = uvw.GetY();
        uvwBuffer[bufferIndex].m_uvw[2] = uvw.GetZ();
    }

    void BasicRHIComponent::SetVertexUVWX(VertexUVWX* uvwxBuffer, int bufferIndex, float u, float v, float w, float x)
    {
        uvwxBuffer[bufferIndex].m_uvwx[0] = u;
        uvwxBuffer[bufferIndex].m_uvwx[1] = v;
        uvwxBuffer[bufferIndex].m_uvwx[2] = w;
        uvwxBuffer[bufferIndex].m_uvwx[3] = x;
    }

    void BasicRHIComponent::SetVertexNormal(VertexNormal* normalBuffer, int bufferIndex, float nx, float ny, float nz)
    {
        normalBuffer[bufferIndex].m_normal[0] = nx;
        normalBuffer[bufferIndex].m_normal[1] = ny;
        normalBuffer[bufferIndex].m_normal[2] = nz;
    }

    void BasicRHIComponent::SetVertexNormal(VertexNormal* normalBuffer, int bufferIndex, const AZ::Vector3& normal)
    {
        normalBuffer[bufferIndex].m_normal[0] = normal.GetX();
        normalBuffer[bufferIndex].m_normal[1] = normal.GetY();
        normalBuffer[bufferIndex].m_normal[2] = normal.GetZ();
    }

    void BasicRHIComponent::SetFullScreenRect(VertexPosition* positionBuffer, VertexUV* uvBuffer,  uint16_t* indexBuffer)
    {
        SetVertexPosition(positionBuffer, 0, -1.0f, -1.0f, 0.0f);
        SetVertexPosition(positionBuffer, 1, -1.0f,  1.0f, 0.0f);
        SetVertexPosition(positionBuffer, 2,  1.0f,  1.0f, 0.0f);
        SetVertexPosition(positionBuffer, 3,  1.0f, -1.0f, 0.0f);

        if (uvBuffer)
        {
            SetVertexUV(uvBuffer, 0, 0.0f, 0.0f);
            SetVertexUV(uvBuffer, 1, 0.0f, 1.0f);
            SetVertexUV(uvBuffer, 2, 1.0f, 1.0f);
            SetVertexUV(uvBuffer, 3, 1.0f, 0.0f);
        }

        indexBuffer[0] = 0;
        indexBuffer[1] = 3;
        indexBuffer[2] = 1;
        indexBuffer[3] = 1;
        indexBuffer[4] = 3;
        indexBuffer[5] = 2;
    }

    void BasicRHIComponent::SetCube(VertexPosition* positionBuffer, VertexColor* colorBuffer, VertexNormal* normalBuffer, uint16_t* indexBuffer)
    {
        static AZ::Vector3 vertices[] =
        {
            //Front Face
            AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),    AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(1.0, -1.0, 1.0),
            //Back Face 
            AZ::Vector3(1.0, 1.0, -1.0),        AZ::Vector3(-1.0, 1.0, -1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(1.0, -1.0, -1.0),
            //Left Face 
            AZ::Vector3(-1.0, 1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(-1.0, 1.0, -1.0),
            //Right Face 
            AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(1.0, -1.0, 1.0),    AZ::Vector3(1.0, -1.0, -1.0),   AZ::Vector3(1.0, 1.0, -1.0),
            //Top Face 
            AZ::Vector3(1.0, 1.0, 1.0),         AZ::Vector3(-1.0, 1.0, 1.0),    AZ::Vector3(-1.0, 1.0, -1.0),   AZ::Vector3(1.0, 1.0, -1.0),
            //Bottom Face 
            AZ::Vector3(1.0, -1.0, 1.0),        AZ::Vector3(-1.0, -1.0, 1.0),   AZ::Vector3(-1.0, -1.0, -1.0),  AZ::Vector3(1.0, -1.0, -1.0)
        };

        static AZ::Vector4 colors[] =
        {
            //Front Face
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
            //Back Face 
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
            //Left Face 
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
            //Right Face
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
            //Top Face  
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0),
            //Bottom Face 
            AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0), AZ::Vector4(0.0, 0.0, 1.0, 1.0)
        };

        static AZ::Vector3 normals[] =
        {
            //Front Face
            AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),     AZ::Vector3(0.0, 0.0, 1.0),
            //Back Face 
            AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),     AZ::Vector3(0.0, 0.0,-1.0),
            //Left Face 
            AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),    AZ::Vector3(-1.0, 0.0, 0.0),
            //Right Face 
            AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),     AZ::Vector3(1.0, 0.0, 0.0),
            //Top Face 
            AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),     AZ::Vector3(0.0, 1.0, 0.0),
            //Bottom Face 
            AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0),    AZ::Vector3(0.0, -1.0, 0.0)
        };

        static uint16_t indices[] =
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
            23, 20, 22
        };

        for (int i = 0; i < AZ_ARRAY_SIZE(vertices); ++i)
        {
            SetVertexPosition(positionBuffer, i, vertices[i]);
            if (colorBuffer)
            {
                SetVertexColor(colorBuffer, i, colors[i]);
            }
            
            if (normalBuffer)
            {
                SetVertexNormal(normalBuffer, i, normals[i]);
            }            
        }
        
        ::memcpy(indexBuffer, indices, sizeof(indices));
    }

    void BasicRHIComponent::SetVertexIndexIncreasing(uint16_t* indexBuffer, size_t bufferSize)
    {
        for (int i = 0; i < bufferSize; ++i)
        {
            indexBuffer[i] = static_cast<uint16_t>(i);
        }
    }

    AZ::Matrix4x4 BasicRHIComponent::CreateViewMatrix(AZ::Vector3 eye, AZ::Vector3 up, AZ::Vector3 lookAt)
    {
        AZ::Vector3 zaxis, xaxis, yaxis;

        zaxis = eye - lookAt;
        if (!zaxis.IsZero())
        {
            zaxis.Normalize();
        }

        xaxis = up.Cross(zaxis);
        if (!xaxis.IsZero())
        {
            xaxis.Normalize();
        }

        yaxis = zaxis.Cross(xaxis);

        AZ::Matrix4x4 result = AZ::Matrix4x4::CreateFromRows
        (
            AZ::Vector4(xaxis.GetX(), xaxis.GetY(), xaxis.GetZ(), -xaxis.Dot(eye)),
            AZ::Vector4(yaxis.GetX(), yaxis.GetY(), yaxis.GetZ(), -yaxis.Dot(eye)),
            AZ::Vector4(zaxis.GetX(), zaxis.GetY(), zaxis.GetZ(), -zaxis.Dot(eye)),
            AZ::Vector4(         0.f,          0.f,          0.f,             1.f)
       );
        return result;
    }

    void BasicRHIComponent::FindShaderInputIndex(
        AZ::RHI::ShaderInputConstantIndex* shaderInputConstIndex,
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
        const AZ::Name& shaderInputName,
        [[maybe_unused]] const char* componentName)
    {
        *shaderInputConstIndex = shaderResourceGroup->FindShaderInputConstantIndex(shaderInputName);
        AZ_Error(componentName, shaderInputConstIndex->IsValid(), "Failed to find shader input constant %s.", shaderInputName.GetCStr());
    }

    void BasicRHIComponent::FindShaderInputIndex(
        AZ::RHI::ShaderInputImageIndex* shaderInputImageIndex,
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
        const AZ::Name& shaderInputName,
        [[maybe_unused]] const char* componentName)
    {
        *shaderInputImageIndex = shaderResourceGroup->FindShaderInputImageIndex(shaderInputName);
        AZ_Error(componentName, shaderInputImageIndex->IsValid(), "Failed to find shader input image %s.", shaderInputName.GetCStr());
    }

    void BasicRHIComponent::FindShaderInputIndex(
        AZ::RHI::ShaderInputBufferIndex* shaderInputBufferIndex,
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
        const AZ::Name& shaderInputName,
        [[maybe_unused]] const char* componentName)
    {
        *shaderInputBufferIndex = shaderResourceGroup->FindShaderInputBufferIndex(shaderInputName);
        AZ_Error(componentName, shaderInputBufferIndex->IsValid(), "Failed to find shader input buffer %s.", shaderInputName.GetCStr());
    }

    void BasicRHIComponent::FindShaderInputIndex(
        AZ::RHI::ShaderInputSamplerIndex* shaderInputSamplerIndex,
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> shaderResourceGroup,
        const AZ::Name& shaderInputName,
        [[maybe_unused]] const char* componentName)
    {
        *shaderInputSamplerIndex = shaderResourceGroup->FindShaderInputSamplerIndex(shaderInputName);
        AZ_Error(componentName, shaderInputSamplerIndex->IsValid(), "Failed to find sampler %s.", shaderInputName.GetCStr());
    }

    AZ::Data::Instance<AZ::RPI::Shader> BasicRHIComponent::LoadShader(const char* shaderFilePath, [[maybe_unused]] const char* sampleName, const AZ::Name* supervariantName)
    {
        using namespace AZ;

        Data::AssetId shaderAssetId;
        Data::AssetCatalogRequestBus::BroadcastResult(
            shaderAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            shaderFilePath, azrtti_typeid<RPI::ShaderAsset>(), false);
        if (!shaderAssetId.IsValid())
        {
            AZ_Error(sampleName, false, "Failed to get shader asset id with path %s", shaderFilePath);
            return nullptr;
        }

        auto shaderAsset = Data::AssetManager::Instance().GetAsset<RPI::ShaderAsset>(
            shaderAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
        shaderAsset.BlockUntilLoadComplete();
        if (!shaderAsset.IsReady())
        {
            AZ_Error(sampleName, false, "Failed to get shader asset with path %s", shaderFilePath);
            return nullptr;
        }

        auto shader = RPI::Shader::FindOrCreate(shaderAsset, (supervariantName != nullptr) ? *supervariantName : AZ::Name{""});
        if (!shader)
        {
            AZ_Error(sampleName, false, "Failed to find or create a shader instance from shader asset '%s'", shaderFilePath);
            return nullptr;
        }

        return shader;
    }

    AZ::Data::Instance<AZ::RPI::Shader> BasicRHIComponent::LoadShader(const AZ::AssetCollectionAsyncLoader& assetLoadMgr, const char* shaderFilePath, [[maybe_unused]] const char* sampleName)
    {
        using namespace AZ;

        auto shaderAsset = assetLoadMgr.GetAsset<RPI::ShaderAsset>(shaderFilePath);
        AZ_Assert(shaderAsset.IsReady(), "Shader asset %s is supposed to be ready", shaderFilePath);

        auto shader = RPI::Shader::FindOrCreate(shaderAsset);
        if (!shader)
        {
            AZ_Error(sampleName, false, "Failed to find or create a shader instance from shader asset '%s'", shaderFilePath);
            return nullptr;
        }

        return shader;
    }

    AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> BasicRHIComponent::CreateShaderResourceGroup(AZ::Data::Instance<AZ::RPI::Shader> shader, const char* shaderResourceGroupId, [[maybe_unused]] const char* sampleName)
    {
        auto srg = AZ::RPI::ShaderResourceGroup::Create(shader->GetAsset(), shader->GetSupervariantIndex(), AZ::Name { shaderResourceGroupId });
        if (!srg)
        {
            AZ_Error(sampleName, false, "Failed to create shader resource group");
            return nullptr;
        }

        return srg;
    }

    AZ::Data::Instance<AZ::RPI::StreamingImage> BasicRHIComponent::LoadStreamingImage(const char* textureFilePath, [[maybe_unused]] const char* sampleName)
    {
        using namespace AZ;

        Data::AssetId streamingImageAssetId;
        Data::AssetCatalogRequestBus::BroadcastResult(
            streamingImageAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            textureFilePath, azrtti_typeid<RPI::StreamingImageAsset>(), false);
        if (!streamingImageAssetId.IsValid())
        {
            AZ_Error(sampleName, false, "Failed to get streaming image asset id with path %s", textureFilePath);
            return nullptr;
        }

        auto streamingImageAsset = Data::AssetManager::Instance().GetAsset<RPI::StreamingImageAsset>(
            streamingImageAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
        streamingImageAsset.BlockUntilLoadComplete();
        if (!streamingImageAsset.IsReady())
        {
            AZ_Error(sampleName, false, "Failed to get streaming image asset '%s'", textureFilePath);
            return nullptr;
        }

        auto image = RPI::StreamingImage::FindOrCreate(streamingImageAsset);
        if (!image)
        {
            AZ_Error(sampleName, false, "Failed to find or create an image instance from image asset '%s'", textureFilePath);
            return nullptr;
        }

        return image;
    }

    void BasicRHIComponent::CreateImage3dData(
        AZStd::vector<uint8_t>& data,
        AZ::RHI::DeviceImageSubresourceLayout& layout,
        AZ::RHI::Format& format,
        AZStd::vector<const char*>&& imageAssetPaths)
    {
        using namespace AZ;

        const auto getStreamingImageAsset = [](const char* path) -> AZ::Data::Asset<AZ::RPI::StreamingImageAsset>
        {
            Data::AssetId streamingImageAssetId;
            Data::AssetCatalogRequestBus::BroadcastResult(
                streamingImageAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                path, azrtti_typeid<RPI::StreamingImageAsset>(), false);

            AZ::Data::Asset<AZ::RPI::StreamingImageAsset> streamingImageAsset = Data::AssetManager::Instance().GetAsset<RPI::StreamingImageAsset>(
                streamingImageAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
            streamingImageAsset.BlockUntilLoadComplete();

            return streamingImageAsset;
        };

        AZStd::vector<RHI::ImageDescriptor> imageDescriptors;
        imageDescriptors.reserve(imageAssetPaths.size());

        AZStd::vector<RHI::DeviceImageSubresourceLayout> imageSubresourceLayouts;
        imageSubresourceLayouts.reserve(imageAssetPaths.size());

        AZStd::vector<Data::Asset<RPI::ImageMipChainAsset>> imageMipAssets;
        imageMipAssets.reserve(imageAssetPaths.size());

        // Load the first mip of the images
        for (const char* imageAssetPath : imageAssetPaths)
        {
            Data::Asset<AZ::RPI::StreamingImageAsset> streamingImageAsset = getStreamingImageAsset(imageAssetPath);
            RHI::ImageDescriptor imageDescriptor = streamingImageAsset->GetImageDescriptor();
            imageDescriptors.emplace_back(imageDescriptor);

            // Load the mip
            Data::Asset<RPI::ImageMipChainAsset> mipAsset = streamingImageAsset->GetMipChainAsset(0);
            mipAsset = Data::AssetManager::Instance().GetAsset<RPI::ImageMipChainAsset>(
                mipAsset.GetId(), AZ::Data::AssetLoadBehavior::PreLoad);
            mipAsset.BlockUntilLoadComplete();
            imageMipAssets.emplace_back(mipAsset);

            const auto subImageLayout = mipAsset->GetSubImageLayout(0);
            imageSubresourceLayouts.emplace_back(subImageLayout);
        }

        // Validate the compatibility of the image sub resources
        for (uint32_t i = 1; i < imageSubresourceLayouts.size(); i++)
        {
            [[maybe_unused]] const bool compatibleFormat = imageDescriptors[0].m_format == imageDescriptors[i].m_format;
            [[maybe_unused]] const bool compatibleLayout = imageSubresourceLayouts[0].m_size == imageSubresourceLayouts[i].m_size &&
                imageSubresourceLayouts[0].m_rowCount == imageSubresourceLayouts[i].m_rowCount &&
                imageSubresourceLayouts[0].m_bytesPerRow == imageSubresourceLayouts[i].m_bytesPerRow &&
                imageSubresourceLayouts[0].m_bytesPerImage == imageSubresourceLayouts[i].m_bytesPerImage;
            AZ_Assert(compatibleLayout && compatibleFormat, "The image sub resources of the first MIP aren't compatible.");
        }

        // Allocate the data of the first mip of the images sequentially
        const uint32_t bytesPerImage = imageSubresourceLayouts[0].m_bytesPerImage;
        data.resize(imageSubresourceLayouts[0].m_bytesPerImage * imageSubresourceLayouts.size());
        for (uint32_t i = 0; i < imageMipAssets.size(); i++)
        {
            const uint64_t dataOffset = i * bytesPerImage;
            memcpy(static_cast<uint8_t*>(data.data()) + dataOffset, imageMipAssets[i]->GetSubImageData(0).data(), imageSubresourceLayouts[0].m_bytesPerImage);
        }

        format = imageDescriptors[0].m_format;
        layout = imageSubresourceLayouts[0];
        layout.m_size.m_depth = static_cast<uint32_t>(imageAssetPaths.size());
    }

    void BasicRHIComponent::SetOutputInfo(uint32_t width, uint32_t height, AZ::RHI::Format format, AZ::RHI::AttachmentId attachmentId)
    {
        m_outputWidth = width;
        m_outputHeight = height;
        m_outputFormat = format;
        m_outputAttachmentId = attachmentId;
        UpdateViewportAndScissor();
    }

    void BasicRHIComponent::SetOutputInfoFromWindowContext()
    {
        m_outputFormat = m_windowContext->GetSwapChain()->GetDescriptor().m_dimensions.m_imageFormat;
        m_outputWidth = aznumeric_cast<uint32_t>(m_windowContext->GetViewport().m_maxX - m_windowContext->GetViewport().m_minX);
        m_outputHeight = aznumeric_cast<uint32_t>(m_windowContext->GetViewport().m_maxY - m_windowContext->GetViewport().m_minY);
        m_outputAttachmentId = m_windowContext->GetSwapChain()->GetAttachmentId();

        UpdateViewportAndScissor();
    }

    void BasicRHIComponent::UpdateViewportAndScissor()
    {
        m_viewport = AZ::RHI::Viewport(0, aznumeric_cast<float>(m_outputWidth), 0, aznumeric_cast<float>(m_outputHeight));
        m_scissor = AZ::RHI::Scissor(0, 0, m_outputWidth, m_outputHeight);
    }

    bool BasicRHIComponent::IsSupportedRHISamplePipeline()
    {
        return m_supportRHISamplePipeline;
    }

    float BasicRHIComponent::GetViewportWidth()
    {
        return m_viewport.m_maxX - m_viewport.m_minX;
    }

    float BasicRHIComponent::GetViewportHeight()
    {
        return m_viewport.m_maxY - m_viewport.m_minY;
    }

    void BasicRHIComponent::SetViewIndex(const uint32_t viewIndex)
    {
        m_viewIndex = viewIndex;
    }
}
