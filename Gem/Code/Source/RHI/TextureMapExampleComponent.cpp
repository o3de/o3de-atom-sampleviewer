/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>

#include <RHI/TextureMapExampleComponent.h>
#include <SampleComponentManager.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    const char* TextureMapExampleComponent::s_textureMapExampleName = "TextureMapExample";
    
    void TextureMapExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TextureMapExampleComponent, AZ::Component>()->Version(0);
        }
    }

    TextureMapExampleComponent::TextureMapExampleComponent()
    {
        m_attachmentID[RenderTargetIndex::Texture1D] = AZ::RHI::AttachmentId("Texture1D");
        m_attachmentID[RenderTargetIndex::Texture1DArray] = AZ::RHI::AttachmentId("Texture1DArray");
        m_attachmentID[RenderTargetIndex::Texture2DArray] = AZ::RHI::AttachmentId("Texture2DArray");
        m_attachmentID[RenderTargetIndex::TextureCubemap] = AZ::RHI::AttachmentId("TextureCubemap");
        m_attachmentID[RenderTargetIndex::TextureCubemapArray] = AZ::RHI::AttachmentId("TextureCubemapArray");
        m_attachmentID[RenderTargetIndex::Texture3D] = AZ::RHI::AttachmentId("Texture3D");

        m_baseRectangle[0] = AZ::Vector3(-1, -1, 1);
        m_baseRectangle[1] = AZ::Vector3(-1, 1, 1);
        m_baseRectangle[2] = AZ::Vector3(1, 1, 1);
        m_baseRectangle[3] = AZ::Vector3(1, -1, 1);

        m_supportRHISamplePipeline = true;
    }

    void TextureMapExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        float value = (sinf(m_time) + 1) * 0.5f;
        
        for (int i = 0; i < s_numOfTargets; ++i)
        {
            m_targetSRGs[i]->SetConstant(m_shaderInputConstantIndices[i], AZ::Vector4(value, value, value, 1));
            m_targetSRGs[i]->Compile();
        }
        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void TextureMapExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        AZ_UNUSED(time);
        m_time += deltaTime;
    }

    void TextureMapExampleComponent::Activate()
    {
        CreateInputAssemblyBufferPool();
        InitRenderTargetBufferView();
        InitRenderTargets();
        LoadRenderTargetShader();

        AZ::RHI::ImageViewDescriptor desc;
        CreateTextureScope(RenderTargetIndex::Texture1D, desc, "texture1d");        // 1D

        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 0, 0);
        CreateTextureScope(RenderTargetIndex::Texture1DArray, desc, "texture1darray0");  // 1D Array layer 0
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 1, 1);
        CreateTextureScope(RenderTargetIndex::Texture1DArray, desc, "texture1darray1");  // 1D Array layer 1
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 2, 2);
        CreateTextureScope(RenderTargetIndex::Texture1DArray, desc, "texture1darray2");  // 1D Array layer 2

        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 0, 0);
        CreateTextureScope(RenderTargetIndex::Texture2DArray, desc, "texture2darray0");  // 2D Array layer 0
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 1, 1);
        CreateTextureScope(RenderTargetIndex::Texture2DArray, desc, "texture2darray1");  // 2D Array layer 1
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 2, 2);
        CreateTextureScope(RenderTargetIndex::Texture2DArray, desc, "texture2darray2");  // 2D Array layer 2
        
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 0);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube0");     // Cubemap 0
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 1);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube1");     // Cubemap 1
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 2);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube2");     // Cubemap 2
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 3);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube3");     // Cubemap 3
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 4);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube4");     // Cubemap 4
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 5);
        CreateTextureScope(RenderTargetIndex::TextureCubemap, desc, "texturecube5");     // Cubemap 5

        // Cubemap Array 0
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 0);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray0");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 1);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray1");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 2);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray2");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 3);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray3");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 4);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray4");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 5);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray5");
        // Cubemap Array 1
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 6);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray6");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 7);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray7");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 8);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray8");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 9);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray9");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 10);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray10");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 11);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray11");
        // Cubemap Array 2
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 12);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray12");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 13);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray13");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 14);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray14");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 15);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray15");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 16);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray16");
        desc = AZ::RHI::ImageViewDescriptor::CreateCubemapFace(s_textureFormat, 0, 0, 17);
        CreateTextureScope(RenderTargetIndex::TextureCubemapArray, desc, "texturecubearray17");

        // Texture 3D
        desc = AZ::RHI::ImageViewDescriptor::Create3D(s_textureFormat, 0, 0, 0, 0);
        CreateTextureScope(RenderTargetIndex::Texture3D, desc, "texture3d0");
        desc = AZ::RHI::ImageViewDescriptor::Create3D(s_textureFormat, 0, 0, 4, 4);
        CreateTextureScope(RenderTargetIndex::Texture3D, desc, "texture3d1");
        desc = AZ::RHI::ImageViewDescriptor::Create3D(s_textureFormat, 0, 0, 7, 7);
        CreateTextureScope(RenderTargetIndex::Texture3D, desc, "texture3d2");


        InitTexture1DBufferView();
        LoadRasterShader("Shaders/RHI/texturemap1d.azshader", RenderTargetIndex::Texture1D);
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0);
        CreateRasterScope(RenderTargetIndex::Texture1D, desc, "rendertexture1d");

        InitTexture1DArrayBufferView();
        LoadRasterShader("Shaders/RHI/texturemap1darray.azshader", RenderTargetIndex::Texture1DArray);
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 0, s_arraySize - 1);
        CreateRasterScope(RenderTargetIndex::Texture1DArray, desc, "rendertexture1darray");

        InitTexture2DArrayBufferView();
        LoadRasterShader("Shaders/RHI/texturemap2darray.azshader", RenderTargetIndex::Texture2DArray);
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0, 0, s_arraySize - 1);
        CreateRasterScope(RenderTargetIndex::Texture2DArray, desc, "rendertexture2darray");

        InitCubemapBufferView();
        LoadRasterShader("Shaders/RHI/texturemapcubemap.azshader", RenderTargetIndex::TextureCubemap);

        desc = AZ::RHI::ImageViewDescriptor::CreateCubemap();
        CreateRasterScope(RenderTargetIndex::TextureCubemap, desc, "rendertexturecubemap");

        InitCubemapArrayBufferView();
        LoadRasterShader("Shaders/RHI/texturemapcubemaparray.azshader", RenderTargetIndex::TextureCubemapArray);
        CreateRasterScope(RenderTargetIndex::TextureCubemapArray, desc, "rendertexturecubemaparray");

        InitTexture3DBufferView();
        LoadRasterShader("Shaders/RHI/texturemap3d.azshader", RenderTargetIndex::Texture3D);
        desc = AZ::RHI::ImageViewDescriptor::Create(s_textureFormat, 0, 0);
        CreateRasterScope(RenderTargetIndex::Texture3D, desc, "rendertexture3d");

        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void TextureMapExampleComponent::Deactivate()
    {
        m_inputAssemblyBufferPool = nullptr;
        m_positionBuffer.fill(nullptr);
        m_uvBuffer.fill(nullptr);
        m_indexBuffer.fill(nullptr);

        m_targetPipelineStates.fill(nullptr);
        m_targetSRGs.fill(nullptr);
        m_screenPipelineStates.fill(nullptr);
        m_screenSRGs.fill(nullptr);

        m_renderTargetImageDescriptors.fill({});
        m_scopeProducers.clear();
        m_windowContext = nullptr;
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }
    
    void TextureMapExampleComponent::CreateInputAssemblyBufferPool()
    {
        const AZ::RHI::Ptr<AZ::RHI::Device> device = Utils::GetRHIDevice();
        m_inputAssemblyBufferPool = aznew AZ::RHI::MultiDeviceBufferPool();

        AZ::RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = AZ::RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = AZ::RHI::HeapMemoryLevel::Device;
        m_inputAssemblyBufferPool->Init(AZ::RHI::MultiDevice::AllDevices, bufferPoolDesc);
    }

    void TextureMapExampleComponent::InitRenderTargetBufferView()
    {
        SetFullScreenRect(m_positions.data(), m_uvs.data(), m_indices.data());

        uint32_t posSize = 4 * sizeof(VertexPosition);
        uint32_t uvSize = 4 * sizeof(VertexUV);
        uint32_t indexSize = 6 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::BufferViewIndex, posSize, m_positions.data(), uvSize, m_uvs.data(), sizeof(VertexUV), indexSize, m_indices.data());
        
        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32_FLOAT);
        m_bufferViews[RenderTargetIndex::BufferViewIndex].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::BufferViewIndex].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::BufferViewIndex].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitTexture1DBufferView()
    {
        AZ::Matrix3x3 transform = AZ::Matrix3x3::CreateScale(AZ::Vector3(0.02, 0.02, 1));
        SetVertexPositionTransform(m_positions.data(), 0, transform, -0.9, -0.8);

        SetVertexUV(m_uvs.data(), 0, 0.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 1, 0.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 2, 1.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 3, 1.0f, 0.0f);

        m_indices[0] = 0;
        m_indices[1] = 3;
        m_indices[2] = 1;
        m_indices[3] = 1;
        m_indices[4] = 3;
        m_indices[5] = 2;

        uint32_t posSize = 4 * sizeof(VertexPosition);
        uint32_t uvSize = 4 * sizeof(VertexUV);
        uint32_t indexSize = 6 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::Texture1D, posSize, m_positions.data(), uvSize, m_uvs.data(), sizeof(VertexUV), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32_FLOAT);
        m_bufferViews[RenderTargetIndex::Texture1D].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::Texture1D].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::Texture1D].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitTexture1DArrayBufferView()
    {
        SetVertexPositionArray(m_positions.data(), 0.02, -0.7, -0.8);

        SetVertexUV(m_uvs.data(), 0,     0.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 1,     0.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 2,     1.0f, 0.0f);
        SetVertexUV(m_uvs.data(), 3,     1.0f, 0.0f);

        SetVertexUV(m_uvs.data(), 4,     0.0f, 1.0f);
        SetVertexUV(m_uvs.data(), 5,     0.0f, 1.0f);
        SetVertexUV(m_uvs.data(), 6,     1.0f, 1.0f);
        SetVertexUV(m_uvs.data(), 7,     1.0f, 1.0f);

        SetVertexUV(m_uvs.data(), 8,     0.0f, 2.0f);
        SetVertexUV(m_uvs.data(), 9,     0.0f, 2.0f);
        SetVertexUV(m_uvs.data(), 10,    1.0f, 2.0f);
        SetVertexUV(m_uvs.data(), 11,    1.0f, 2.0f);

        SetVertexIndexRectsCounterClock(m_indices.data(), 18);

        uint32_t posSize = 12 * sizeof(VertexPosition);
        uint32_t uvSize = 12 * sizeof(VertexUV);
        uint32_t indexSize = 18 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::Texture1DArray, posSize, m_positions.data(), uvSize, m_uvs.data(), sizeof(VertexUV), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32_FLOAT);
        m_bufferViews[RenderTargetIndex::Texture1DArray].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::Texture1DArray].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::Texture1DArray].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitTexture2DArrayBufferView()
    {
        SetVertexPositionArray(m_positions.data(), 0.05, -0.4, -0.8);
        SetVertexUVWArray(m_uvws.data(), s_arraySize);
        SetVertexIndexRectsCounterClock(m_indices.data(), 18);

        uint32_t posSize = 12 * sizeof(VertexPosition);
        uint32_t uvSize = 12 * sizeof(VertexUVW);
        uint32_t indexSize = 18 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::Texture2DArray, posSize, m_positions.data(), uvSize, m_uvws.data(), sizeof(VertexUVW), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32B32_FLOAT);
        m_bufferViews[RenderTargetIndex::Texture2DArray].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::Texture2DArray].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::Texture2DArray].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitCubemapBufferView()
    {
        SetVertexPositionCubemap(m_positions.data(), 0, 0.0, -0.8);
        SetVertexUVWCubemap(m_uvws.data());
        SetVertexIndexRectsCounterClock(m_indices.data(), 36);

        uint32_t posSize = 24 * sizeof(VertexPosition);
        uint32_t uvSize = 24 * sizeof(VertexUVW);
        uint32_t indexSize = 36 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::TextureCubemap, posSize, m_positions.data(), uvSize, m_uvws.data(), sizeof(VertexUVW), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32B32_FLOAT);
        m_bufferViews[RenderTargetIndex::TextureCubemap].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::TextureCubemap].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::TextureCubemap].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitCubemapArrayBufferView()
    {
        SetVertexPositionCubemap(m_positions.data(), 0, 0.4, -0.8);
        SetVertexPositionCubemap(m_positions.data(), 24, 0.4, -0.2);
        SetVertexPositionCubemap(m_positions.data(), 48, 0.4, 0.4);
        SetVertexUVWXCubemapArray(m_uvwxs.data(), s_arraySize);
        SetVertexIndexRectsCounterClock(m_indices.data(), 108);

        uint32_t posSize = 72 * sizeof(VertexPosition);
        uint32_t uvSize = 72 * sizeof(VertexUVWX);
        uint32_t indexSize = 108 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::TextureCubemapArray, posSize, m_positions.data(), uvSize, m_uvwxs.data(), sizeof(VertexUVWX), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32B32A32_FLOAT);
        m_bufferViews[RenderTargetIndex::TextureCubemapArray].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::TextureCubemapArray].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::TextureCubemapArray].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitTexture3DBufferView()
    {
        SetVertexPositionArray(m_positions.data(), 0.05, 0.8, -0.8);

        SetVertexUVW(m_uvws.data(), 0, 0.0f, 0.0f, 0.05f);
        SetVertexUVW(m_uvws.data(), 1, 0.0f, 1.0f, 0.05f);
        SetVertexUVW(m_uvws.data(), 2, 1.0f, 1.0f, 0.05f);
        SetVertexUVW(m_uvws.data(), 3, 1.0f, 0.0f, 0.05f);

        SetVertexUVW(m_uvws.data(), 4, 0.0f, 0.0f, 0.55f);
        SetVertexUVW(m_uvws.data(), 5, 0.0f, 1.0f, 0.55f);
        SetVertexUVW(m_uvws.data(), 6, 1.0f, 1.0f, 0.55f);
        SetVertexUVW(m_uvws.data(), 7, 1.0f, 0.0f, 0.55f);

        SetVertexUVW(m_uvws.data(), 8, 0.0f, 0.0f, 0.95f);
        SetVertexUVW(m_uvws.data(), 9, 0.0f, 1.0f, 0.95f);
        SetVertexUVW(m_uvws.data(), 10, 1.0f, 1.0f, 0.95f);
        SetVertexUVW(m_uvws.data(), 11, 1.0f, 0.0f, 0.95f);

        SetVertexIndexRectsCounterClock(m_indices.data(), 18);

        uint32_t posSize = 12 * sizeof(VertexPosition);
        uint32_t uvSize = 12 * sizeof(VertexUVW);
        uint32_t indexSize = 18 * sizeof(uint16_t);

        InitBufferView(RenderTargetIndex::Texture3D, posSize, m_positions.data(), uvSize, m_uvws.data(), sizeof(VertexUVW), indexSize, m_indices.data());

        AZ::RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", AZ::RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", AZ::RHI::Format::R32G32B32_FLOAT);
        m_bufferViews[RenderTargetIndex::Texture3D].m_inputStreamLayout = layoutBuilder.End();

        AZ::RHI::ValidateStreamBufferViews(m_bufferViews[RenderTargetIndex::Texture3D].m_inputStreamLayout, m_bufferViews[RenderTargetIndex::Texture3D].m_streamBufferViews);
    }

    void TextureMapExampleComponent::InitRenderTargets()
    {
        using namespace AZ;
        RHI::ImageDescriptor imageDesc;

        imageDesc = RHI::ImageDescriptor::Create1D(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::Texture1D] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::Texture1D], imageDesc);

        imageDesc = RHI::ImageDescriptor::Create1DArray(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_arraySize, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::Texture1DArray] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::Texture1DArray], imageDesc);

        imageDesc = RHI::ImageDescriptor::Create2DArray(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_textureHeight, s_arraySize, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::Texture2DArray] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::Texture2DArray], imageDesc);

        imageDesc = RHI::ImageDescriptor::CreateCubemap(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::TextureCubemap] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::TextureCubemap], imageDesc);

        imageDesc = RHI::ImageDescriptor::CreateCubemapArray(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_arraySize, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::TextureCubemapArray] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::TextureCubemapArray], imageDesc);

        imageDesc = RHI::ImageDescriptor::Create3D(RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead, s_textureWidth, s_textureHeight, s_textureDepth, s_textureFormat);
        m_renderTargetImageDescriptors[RenderTargetIndex::Texture3D] = RHI::TransientImageDescriptor(m_attachmentID[RenderTargetIndex::Texture3D], imageDesc);
    }

    void TextureMapExampleComponent::LoadRenderTargetShader()
    {
        using namespace AZ;
        const char* shaderFilePath = "Shaders/RHI/texturemaptarget.azshader";
        auto shader = LoadShader(shaderFilePath, s_textureMapExampleName);
        if (shader == nullptr)
        {
            return;
        }

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_bufferViews[RenderTargetIndex::BufferViewIndex].m_inputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(s_textureFormat);
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        for (int i = 0; i < s_numOfTargets; ++i)
        {
            m_targetPipelineStates[i] = shader->AcquirePipelineState(pipelineDesc);
            m_targetSRGs[i] = CreateShaderResourceGroup(shader, "TextureMapTargetSrg", s_textureMapExampleName);
            FindShaderInputIndex(&m_shaderInputConstantIndices[i], m_targetSRGs[i], Name{"sinValue"}, s_textureMapExampleName);
        }
    }

    void TextureMapExampleComponent::LoadRasterShader(const char* shaderFilePath, int target)
    {
        using namespace AZ;

        auto shader = LoadShader(shaderFilePath, s_textureMapExampleName);
        if (shader == nullptr)
        {
            return;
        }

        RHI::PipelineStateDescriptorForDraw pipelineDesc;
        shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);
        pipelineDesc.m_inputStreamLayout = m_bufferViews[target].m_inputStreamLayout;

        RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
        attachmentsBuilder.AddSubpass()
            ->RenderTargetAttachment(m_outputFormat);
        [[maybe_unused]] AZ::RHI::ResultCode result = attachmentsBuilder.End(pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout);
        AZ_Assert(result == AZ::RHI::ResultCode::Success, "Failed to create render attachment layout");

        m_screenPipelineStates[target] = shader->AcquirePipelineState(pipelineDesc);
        m_screenSRGs[target] = CreateShaderResourceGroup(shader, "TextureMapSrg", s_textureMapExampleName);
        FindShaderInputIndex(&m_shaderInputImageIndices[target], m_screenSRGs[target], Name{"texture"}, s_textureMapExampleName);
    }

    void TextureMapExampleComponent::CreateTextureScope(int target, const AZ::RHI::ImageViewDescriptor& imageViewDescriptor, const char* scopeName)
    {
        using namespace AZ;

        struct ScopeData
        {
        };

        const auto prepareFunction = [this, imageViewDescriptor, target](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Create & Binds RenderTarget images
            {
                if (!frameGraph.GetAttachmentDatabase().IsAttachmentValid(m_attachmentID[target]))
                {
                    frameGraph.GetAttachmentDatabase().CreateTransientImage(m_renderTargetImageDescriptors[target]);
                }
                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_attachmentID[target];
                desc.m_imageViewDescriptor = imageViewDescriptor;
                desc.m_loadStoreAction.m_clearValue = m_clearValue;
                desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Clear;
                frameGraph.UseColorAttachment(desc);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this, target](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            
            RHI::Scissor scissor(0, 0, m_renderTargetImageDescriptors[target].m_imageDescriptor.m_size.m_width, m_renderTargetImageDescriptors[target].m_imageDescriptor.m_size.m_height);
            commandList->SetScissors(&scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_targetSRGs[target]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_targetPipelineStates[target]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_bufferViews[RenderTargetIndex::BufferViewIndex].m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_bufferViews[RenderTargetIndex::BufferViewIndex].m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_bufferViews[RenderTargetIndex::BufferViewIndex].m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_bufferViews[RenderTargetIndex::BufferViewIndex].m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;

            // Submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId(scopeName),
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void TextureMapExampleComponent::CreateRasterScope(int target, const AZ::RHI::ImageViewDescriptor& imageViewDescriptor, const char* scopeName)
    {
        using namespace AZ;
        struct ScopeData
        {
        };

        const auto prepareFunction = [this, target, imageViewDescriptor](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            // Binds the swap chain as a color attachment.
            {
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                descriptor.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                frameGraph.UseColorAttachment(descriptor);
            }

            // ShaderResourceGroup Texture input
            {
                RHI::ImageScopeAttachmentDescriptor desc;
                desc.m_attachmentId = m_attachmentID[target];
                desc.m_imageViewDescriptor = imageViewDescriptor;
                frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::Read);
            }
            // We will submit a single draw item.
            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this, target](const AZ::RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            const auto* imageView = context.GetImageView(m_attachmentID[target]);

            m_screenSRGs[target]->SetImageView(m_shaderInputImageIndices[target], imageView);
            m_screenSRGs[target]->Compile();
        };

        const auto executeFunction = [this, target](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = m_bufferViews[target].m_indexBufferView.GetByteCount() / sizeof(uint16_t);
            drawIndexed.m_instanceCount = 1;

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_screenSRGs[target]->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_screenPipelineStates[target]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_bufferViews[target].m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_bufferViews[target].m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_bufferViews[target].m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_bufferViews[target].m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;

            // Submit the triangle draw item.
            commandList->Submit(drawItem);
        };

        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                AZ::RHI::ScopeId(scopeName),
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void TextureMapExampleComponent::SetVertexUVWArray(VertexUVW* uvwBuffer, int arraySize)
    {
        for (int i = 0; i < arraySize; ++i)
        {
            SetVertexUVW(uvwBuffer, i * 4 + 0, 0.0f, 0.0f, static_cast<float>(i));
            SetVertexUVW(uvwBuffer, i * 4 + 1, 0.0f, 1.0f, static_cast<float>(i));
            SetVertexUVW(uvwBuffer, i * 4 + 2, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVW(uvwBuffer, i * 4 + 3, 1.0f, 0.0f, static_cast<float>(i));
        }
    }

    void TextureMapExampleComponent::SetVertexIndexRectsCounterClock(uint16_t* indexBuffer, size_t arraySize)
    {
        uint16_t vertices = 0;
        for (int i = 0; i < arraySize; i += 6)
        {
            indexBuffer[i + 0] = vertices + 0;
            indexBuffer[i + 1] = vertices + 3;
            indexBuffer[i + 2] = vertices + 1;
            indexBuffer[i + 3] = vertices + 1;
            indexBuffer[i + 4] = vertices + 3;
            indexBuffer[i + 5] = vertices + 2;
            vertices += 4;
        }
    }

    void TextureMapExampleComponent::SetVertexUVWCubemap(VertexUVW* uvwBuffer)
    {
        SetVertexUVW(uvwBuffer, 0, -1.0f, -1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 1, -1.0f, -1.0f,  1.0f);
        SetVertexUVW(uvwBuffer, 2, -1.0f,  1.0f,  1.0f);
        SetVertexUVW(uvwBuffer, 3, -1.0f,  1.0f, -1.0f);

        SetVertexUVW(uvwBuffer, 4, -1.0f, -1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 5, -1.0f, -1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 6, 1.0f, -1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 7, 1.0f, -1.0f, -1.0f);

        SetVertexUVW(uvwBuffer, 8, 1.0f, -1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 9, 1.0f, -1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 10, 1.0f, 1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 11, 1.0f, 1.0f, -1.0f);

        SetVertexUVW(uvwBuffer, 12, -1.0f, -1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 13, -1.0f, 1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 14, 1.0f, 1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 15, 1.0f, -1.0f, 1.0f);

        SetVertexUVW(uvwBuffer, 16, -1.0f, -1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 17, -1.0f, 1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 18, 1.0f, 1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 19, 1.0f, -1.0f, -1.0f);

        SetVertexUVW(uvwBuffer, 20, -1.0f, 1.0f, -1.0f);
        SetVertexUVW(uvwBuffer, 21, -1.0f, 1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 22, 1.0f, 1.0f, 1.0f);
        SetVertexUVW(uvwBuffer, 23, 1.0f, 1.0f, -1.0f);
    }

    void TextureMapExampleComponent::SetVertexUVWXCubemapArray(VertexUVWX* uvwxBuffer, int arraySize)
    {
        for (int i = 0; i < arraySize; ++i)
        {
            SetVertexUVWX(uvwxBuffer, i * 24 + 0, 1.0f, -1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 1, 1.0f, -1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 2, 1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 3, 1.0f, 1.0f, -1.0f, static_cast<float>(i));

            SetVertexUVWX(uvwxBuffer, i * 24 + 4, -1.0f, -1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 5, -1.0f, -1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 6, -1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 7, -1.0f, 1.0f, -1.0f, static_cast<float>(i));

            SetVertexUVWX(uvwxBuffer, i * 24 + 8, -1.0f, 1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 9, -1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 10, 1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 11, 1.0f, 1.0f, -1.0f, static_cast<float>(i));

            SetVertexUVWX(uvwxBuffer, i * 24 + 12, -1.0f, -1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 13, -1.0f, -1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 14, 1.0f, -1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 15, 1.0f, -1.0f, -1.0f, static_cast<float>(i));

            SetVertexUVWX(uvwxBuffer, i * 24 + 16, -1.0f, -1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 17, -1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 18, 1.0f, 1.0f, 1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 19, 1.0f, -1.0f, 1.0f, static_cast<float>(i));

            SetVertexUVWX(uvwxBuffer, i * 24 + 20, -1.0f, -1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 21, -1.0f, 1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 22, 1.0f, 1.0f, -1.0f, static_cast<float>(i));
            SetVertexUVWX(uvwxBuffer, i * 24 + 23, 1.0f, -1.0f, -1.0f, static_cast<float>(i));
        }    
    }

    void TextureMapExampleComponent::SetVertexPositionTransform(VertexPosition* positionBuffer, int bufferIndex, AZ::Matrix3x3 transform, float translateX, float translateY)
    {
        transform.SetColumn(2, AZ::Vector3(translateX, translateY, 0.0));

        AZ::Vector3 transformedVertex = transform * m_baseRectangle[0];
        positionBuffer[bufferIndex].m_position[0] = transformedVertex.GetX();
        positionBuffer[bufferIndex].m_position[1] = transformedVertex.GetY();
        positionBuffer[bufferIndex].m_position[2] = 0;

        transformedVertex = transform * m_baseRectangle[1];
        positionBuffer[bufferIndex + 1].m_position[0] = transformedVertex.GetX();
        positionBuffer[bufferIndex + 1].m_position[1] = transformedVertex.GetY();
        positionBuffer[bufferIndex + 1].m_position[2] = 0;

        transformedVertex = transform * m_baseRectangle[2];
        positionBuffer[bufferIndex + 2].m_position[0] = transformedVertex.GetX();
        positionBuffer[bufferIndex + 2].m_position[1] = transformedVertex.GetY();
        positionBuffer[bufferIndex + 2].m_position[2] = 0;

        transformedVertex = transform * m_baseRectangle[3];
        positionBuffer[bufferIndex + 3].m_position[0] = transformedVertex.GetX();
        positionBuffer[bufferIndex + 3].m_position[1] = transformedVertex.GetY();
        positionBuffer[bufferIndex + 3].m_position[2] = 0;
    }

    void TextureMapExampleComponent::SetVertexPositionArray(VertexPosition* positionBuffer, float scale, float translateX, float translateY)
    {
        AZ::Matrix3x3 transform = AZ::Matrix3x3::CreateScale(AZ::Vector3(scale, scale, 1));

        for (int i = 0; i < s_arraySize; ++i)
        {
            SetVertexPositionTransform(positionBuffer, i * 4, transform, translateX, translateY + 0.2f*i );
        }
    }

    void TextureMapExampleComponent::SetVertexPositionCubemap(VertexPosition* positionBuffer, int bufferIndex, float translateX, float translateY)
    {
        AZ::Matrix3x3 transform = AZ::Matrix3x3::CreateScale(AZ::Vector3(0.05, 0.05, 1));

        SetVertexPositionTransform(positionBuffer, bufferIndex + 0, transform, translateX, translateY);
        SetVertexPositionTransform(positionBuffer, bufferIndex + 4, transform, translateX, translateY + 0.1f);
        SetVertexPositionTransform(positionBuffer, bufferIndex + 8, transform, translateX, translateY + 0.2f);
        SetVertexPositionTransform(positionBuffer, bufferIndex + 12, transform, translateX - 0.1f, translateY + 0.2f);
        SetVertexPositionTransform(positionBuffer, bufferIndex + 16, transform, translateX + 0.1f, translateY + 0.2f);
        SetVertexPositionTransform(positionBuffer, bufferIndex + 20, transform, translateX, translateY + 0.3f);
    }

    void TextureMapExampleComponent::InitBufferView(int target,
        uint32_t posSize, void* posData,
        uint32_t uvSize, void* uvData, uint32_t uvTypeSize,
        uint32_t indexSize, void* indexData)
    {
        m_positionBuffer[target] = aznew AZ::RHI::MultiDeviceBuffer();
        AZ::RHI::ResultCode result = AZ::RHI::ResultCode::Success;
        AZ::RHI::MultiDeviceBufferInitRequest request;
        request.m_buffer = m_positionBuffer[target].get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, posSize };
        request.m_initialData = posData;

        result = m_inputAssemblyBufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error(s_textureMapExampleName, false, "Failed to initialize buffer with error code %d", result);
        }

        m_bufferViews[target].m_streamBufferViews[0] =
        {
            *m_positionBuffer[target],
            0,
            posSize,
            sizeof(VertexPosition)
        };

        m_uvBuffer[target] = aznew AZ::RHI::MultiDeviceBuffer();
        request.m_buffer = m_uvBuffer[target].get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, uvSize };
        request.m_initialData = uvData;

        result = m_inputAssemblyBufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error(s_textureMapExampleName, false, "Failed to initialize buffer with error code %d", result);
        }

        m_bufferViews[target].m_streamBufferViews[1] =
        {
            *m_uvBuffer[target],
            0,
            uvSize,
            uvTypeSize
        };

        m_indexBuffer[target] = aznew AZ::RHI::MultiDeviceBuffer();
        request.m_buffer = m_indexBuffer[target].get();
        request.m_descriptor = AZ::RHI::BufferDescriptor{ AZ::RHI::BufferBindFlags::InputAssembly, indexSize };
        request.m_initialData = indexData;

        result = m_inputAssemblyBufferPool->InitBuffer(request);
        if (result != AZ::RHI::ResultCode::Success)
        {
            AZ_Error(s_textureMapExampleName, false, "Failed to initialize buffer with error code %d", result);
        }

        m_bufferViews[target].m_indexBufferView =
        {
            *m_indexBuffer[target],
            0,
            indexSize,
            AZ::RHI::IndexFormat::Uint16
        };
    }
}
