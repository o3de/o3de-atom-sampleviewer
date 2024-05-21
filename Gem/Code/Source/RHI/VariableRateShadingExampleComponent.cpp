/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RHI/VariableRateShadingExampleComponent.h>
#include <Utils/Utils.h>

#include <SampleComponentManager.h>

#include <Atom/RHI/CommandList.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayoutBuilder.h>
#include <Atom/RHI.Reflect/VariableRateShadingEnums.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Input/Devices/Touch/InputDeviceTouch.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace VariableRateShading
    {
        const char* SampleName = "VariableRateShadingExample";
        const char* ShadingRateAttachmentId = "ShadingRateAttachmentId";
        const char* ShadingRateAttachmentUpdateId = "ShadingRateAttachmentUpdateId";
    }

    RHI::Format ConvertToUInt(RHI::Format format)
    {
        uint32_t count = GetFormatComponentCount(format);
        if (count == 1)
        {
            return RHI::Format::R8_UINT;
        }
        else if (count == 2)
        {
            return RHI::Format::R8G8_UINT;
        }
        return RHI::Format::R8G8B8A8_UINT;
    }

    const char* ToString(RHI::ShadingRate rate)
    {
        switch (rate)
        {
        case RHI::ShadingRate::Rate1x1: return "Rate1x1";
        case RHI::ShadingRate::Rate1x2: return "Rate1x2";
        case RHI::ShadingRate::Rate2x1: return "Rate2x1";
        case RHI::ShadingRate::Rate2x2: return "Rate2x2";
        case RHI::ShadingRate::Rate2x4: return "Rate2x4";
        case RHI::ShadingRate::Rate4x2: return "Rate4x2";
        case RHI::ShadingRate::Rate4x4: return "Rate4x4";
        case RHI::ShadingRate::Rate4x1: return "Rate4x1";
        case RHI::ShadingRate::Rate1x4: return "Rate1x4";
        default: return "";
        }
    }

    void VariableRateShadingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<VariableRateShadingExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    VariableRateShadingExampleComponent::VariableRateShadingExampleComponent()
    {
        m_supportRHISamplePipeline = true;
    }

    void VariableRateShadingExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_imguiSidebar.Begin())
        {
            DrawSettings();
        }
    }

    void VariableRateShadingExampleComponent::OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder)
    {
        if (m_windowContext->GetSwapChainsSize() && m_windowContext->GetSwapChain())
        {
            if (m_useImageShadingRate)
            {
                frameGraphBuilder.GetAttachmentDatabase().ImportImage(RHI::AttachmentId{ VariableRateShading::ShadingRateAttachmentId }, m_shadingRateImages[m_frameCount % m_shadingRateImages.size()]);
                if (!Utils::GetRHIDevice()->GetFeatures().m_dynamicShadingRateImage)
                {
                    // We cannot update and use the same shading rate image because "m_dynamicShadingRateImage" is not supported.
                    frameGraphBuilder.GetAttachmentDatabase().ImportImage(RHI::AttachmentId{ VariableRateShading::ShadingRateAttachmentUpdateId }, m_shadingRateImages[(m_frameCount + m_shadingRateImages.size() - 1) % m_shadingRateImages.size()]);
                }
            }
            m_frameCount++;
        }

        BasicRHIComponent::OnFramePrepare(frameGraphBuilder);
    }

    void VariableRateShadingExampleComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();
        AZ::RHI::RHISystemNotificationBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        const auto& deviceFeatures = device->GetFeatures();
        if (!RHI::CheckBitsAll(deviceFeatures.m_shadingRateTypeMask, RHI::ShadingRateTypeFlags::PerRegion))
        {
            m_useImageShadingRate = false;
        }

        if (!RHI::CheckBitsAll(deviceFeatures.m_shadingRateTypeMask, RHI::ShadingRateTypeFlags::PerDraw))
        {
            m_useDrawShadingRate = false;
        }

        if (m_useImageShadingRate)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(RHI::Format::Count); ++i)
            {
                RHI::Format format = static_cast<RHI::Format>(i);
                RHI::FormatCapabilities capabilities = device->GetFormatCapabilities(format);
                if (RHI::CheckBitsAll(capabilities, RHI::FormatCapabilities::ShadingRate))
                {
                    m_rateShadingImageFormat = format;
                    break;
                }
            }
            AZ_Assert(m_rateShadingImageFormat != RHI::Format::Unknown, "Could not find a format for the shading rate image");
        }

        const auto& supportedMask = device->GetFeatures().m_shadingRateMask;
        for (uint32_t i = 0; i < static_cast<uint32_t>(RHI::ShadingRate::Count); ++i)
        {
            if (RHI::CheckBitsAll(supportedMask, static_cast<RHI::ShadingRateFlags>(AZ_BIT(i))))
            {
                m_supportedModes.push_back(static_cast<RHI::ShadingRate>(i));
            }
        }
        m_shadingRate = m_supportedModes[0];

        CreateShadingRateImage();
        LoadShaders();
        CreateInputAssemblyBuffersAndViews();
        CreateShaderResourceGroups();
        CreatePipelines();
        CreateComputeScope();
        CreateRenderScope();
        CreatImageDisplayScope();
        m_frameCount = 0;
    }

    void VariableRateShadingExampleComponent::CreateShadingRateImage()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        const auto& tileSize = device->GetLimits().m_shadingRateTileSize;
        m_shadingRateImageSize = Vector2(ceil(static_cast<float>(m_outputWidth) / tileSize.m_width), ceil(static_cast<float>(m_outputHeight) / tileSize.m_height));

        m_imagePool = aznew RHI::MultiDeviceImagePool();
        RHI::ImagePoolDescriptor imagePoolDesc;
        imagePoolDesc.m_bindFlags = RHI::ImageBindFlags::ShadingRate | RHI::ImageBindFlags::ShaderReadWrite;
        m_imagePool->Init(RHI::MultiDevice::AllDevices, imagePoolDesc);

        // Initialize the shading rate images with proper values. Invalid values may cause a crash.
        uint32_t width = static_cast<uint32_t>(m_shadingRateImageSize.GetX());
        uint32_t height = static_cast<uint32_t>(m_shadingRateImageSize.GetY());
        uint32_t formatSize = GetFormatSize(m_rateShadingImageFormat);
        uint32_t bufferSize = width * height * formatSize;
        AZStd::vector<uint8_t> shadingRatePatternData(bufferSize);
        if (m_useImageShadingRate)
        {
            // Use the lowest shading rate as the default value.
            RHI::ShadingRateImageValue defaultValue = device->ConvertShadingRate(m_supportedModes[m_supportedModes.size() - 1]);
            uint8_t* ptrData = shadingRatePatternData.data();
            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    ::memcpy(ptrData, &defaultValue, formatSize);
                    ptrData += formatSize;
                }
            }
        }

        // Since the device may not support "Dynamic Shading Rate Image", we need to buffer the update of the shading rate image
        // because the CPU may be trying to read the image.
        m_shadingRateImages.resize(device->GetFeatures().m_dynamicShadingRateImage ? 1 : device->GetDescriptor().m_frameCountMax+3);
        for (auto& image : m_shadingRateImages)
        {
            image = aznew RHI::MultiDeviceImage();
            RHI::MultiDeviceImageInitRequest initImageRequest;
            RHI::ClearValue clearValue = RHI::ClearValue::CreateVector4Float(1, 1, 1, 1);
            initImageRequest.m_image = image.get();
            initImageRequest.m_descriptor = RHI::ImageDescriptor::Create2D(
                imagePoolDesc.m_bindFlags,
                static_cast<uint32_t>(m_shadingRateImageSize.GetX()),
                static_cast<uint32_t>(m_shadingRateImageSize.GetY()),
                m_rateShadingImageFormat);
            initImageRequest.m_optimizedClearValue = &clearValue;
            m_imagePool->InitImage(initImageRequest);

            RHI::MultiDeviceImageUpdateRequest request;
            request.m_image = image.get();
            request.m_sourceData = shadingRatePatternData.data();
            request.m_sourceSubresourceLayout = RHI::MultiDeviceImageSubresourceLayout();
            request.m_sourceSubresourceLayout.Init(
                RHI::MultiDevice::AllDevices, { RHI::Size(width, height, 1), height, width * formatSize, bufferSize, 1, 1 });

            m_imagePool->UpdateImageContents(request);
        }        
    }  

    void VariableRateShadingExampleComponent::LoadShaders()
    {
        const char* shaders[] =
        {
            "Shaders/RHI/VariableRateShading.azshader",
            "Shaders/RHI/VariableRateShadingCompute.azshader",
            "Shaders/RHI/VariableRateShadingImage.azshader"
        };

        m_shaders.resize(AZ_ARRAY_SIZE(shaders));
        for (size_t i = 0; i < AZ_ARRAY_SIZE(shaders); ++i)
        {
            auto shader = LoadShader(shaders[i], VariableRateShading::SampleName);
            if (shader == nullptr)
            {
                return;
            }

            m_shaders[i] = shader;
        }

        const auto& numThreads = m_shaders[1]->GetAsset()->GetAttribute(RHI::ShaderStage::Compute, Name("numthreads"));
        if (numThreads)
        {
            const RHI::ShaderStageAttributeArguments& args = *numThreads;
            m_numThreadsX = args[0].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[0]) : m_numThreadsX;
            m_numThreadsY = args[1].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[1]) : m_numThreadsY;
            m_numThreadsZ = args[2].type() == azrtti_typeid<int>() ? AZStd::any_cast<int>(args[2]) : m_numThreadsZ;
        }
    }

    void VariableRateShadingExampleComponent::CreateShaderResourceGroups()
    {
        const Name albedoId{ "m_texture" };
        auto textureIamge = LoadStreamingImage("textures/bricks.png.streamingimage", VariableRateShading::SampleName);

        AZ::RHI::ShaderInputImageIndex albedoIndex;
        m_modelShaderResourceGroup = CreateShaderResourceGroup(m_shaders[0], "InstanceSrg", VariableRateShading::SampleName);
        FindShaderInputIndex(&albedoIndex, m_modelShaderResourceGroup, albedoId, VariableRateShading::SampleName);
        m_modelShaderResourceGroup->SetImage(albedoIndex, textureIamge);
        m_modelShaderResourceGroup->Compile();

        const Name centerId{ "m_center" };
        const Name distancesId{ "m_distances" };
        const Name patternId{ "m_pattern" };
        const Name shadingRateImageId{ "m_shadingRateTexture" };

        AZ::RHI::ShaderInputConstantIndex patternIndex;

        m_computeShaderResourceGroup = CreateShaderResourceGroup(m_shaders[1], "ComputeSrg", VariableRateShading::SampleName);
        FindShaderInputIndex(&patternIndex, m_computeShaderResourceGroup, patternId, VariableRateShading::SampleName);
        FindShaderInputIndex(&m_centerIndex, m_computeShaderResourceGroup, centerId, VariableRateShading::SampleName);
        FindShaderInputIndex(&m_shadingRateIndex, m_computeShaderResourceGroup, shadingRateImageId, VariableRateShading::SampleName);

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();

        constexpr uint32_t elementsCount = 4;
        struct Pattern
        {
            float m_distance[elementsCount];
            uint32_t m_rate[elementsCount];
        };

        struct Color
        {
            float m_color[4];
            uint32_t m_rate[4];
        };

        const float alpha = 0.3f;
        const uint32_t numRates = static_cast<uint32_t>(RHI::ShadingRate::Count);
        AZStd::array<Pattern, numRates> pattern;
        AZStd::array<Color, numRates> patternColors;
        AZStd::array<AZ::Color, numRates> colors = 
        {{
            AZ::Color(0.f, 0.f, 1.f, alpha),
            AZ::Color(1.f, 0.f, 0.f, alpha),
            AZ::Color(0.f, 1.f, 0.f, alpha),
            AZ::Color(1.f, 0.f, 1.f, alpha),
            AZ::Color(1.f, 1.f, 0.f, alpha),
            AZ::Color(0.f, 1.f, 1.f, alpha),
            AZ::Color(1.f, 1.f, 1.f, alpha)
        }};

        float range = 60.0f / numRates;
        float currentRange = 8.0f;

        const auto& supportedMask = device->GetFeatures().m_shadingRateMask;
        for (uint32_t i = 0; i < pattern.size(); ++i)
        {
            RHI::ShadingRateImageValue rate = {};
            pattern[i].m_distance[0] = 0.0f;
            if (RHI::CheckBitsAll(supportedMask, static_cast<RHI::ShadingRateFlags>(AZ_BIT(i))))
            {
                rate = device->ConvertShadingRate(static_cast<RHI::ShadingRate>(i));
                pattern[i].m_distance[0] = currentRange;
            }
            pattern[i].m_rate[0] = rate.m_x;
            pattern[i].m_rate[1] = rate.m_y;
            currentRange += range;

            patternColors[i].m_rate[0] = pattern[i].m_rate[0];
            patternColors[i].m_rate[1] = pattern[i].m_rate[1];
            colors[i].StoreToFloat4(patternColors[i].m_color);
        }       

        Vector2 center(static_cast<float>(m_shadingRateImageSize.GetX()) * 0.5f, static_cast<float>(m_shadingRateImageSize.GetY()) * 0.5f);
        m_computeShaderResourceGroup->SetConstant(m_centerIndex, center);
        m_computeShaderResourceGroup->SetConstantArray(patternIndex, pattern);

        const Name colorsId{ "m_colors" };
        const Name textureId{ "m_texture" };
        AZ::RHI::ShaderInputConstantIndex colorsIndex;

        m_imageShaderResourceGroup = CreateShaderResourceGroup(m_shaders[2], "InstanceSrg", VariableRateShading::SampleName);
        FindShaderInputIndex(&colorsIndex, m_imageShaderResourceGroup, colorsId, VariableRateShading::SampleName);
        FindShaderInputIndex(&m_shadingRateDisplayIndex, m_imageShaderResourceGroup, textureId, VariableRateShading::SampleName);
        m_imageShaderResourceGroup->SetConstantArray(colorsIndex, patternColors);
    }

    void VariableRateShadingExampleComponent::CreatePipelines()
    {        
        {
            // We create one pipeline when using a shading rate attachment, and another one when we are not using it.
            RHI::RenderAttachmentLayoutBuilder shadingRateAttachmentsBuilder;
            shadingRateAttachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat)
                ->ShadingRateAttachment(m_rateShadingImageFormat);

            RHI::RenderAttachmentLayout shadingRateRenderAttachmentLayout;
            [[maybe_unused]] RHI::ResultCode result = shadingRateAttachmentsBuilder.End(shadingRateRenderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            const auto& shader = m_shaders[0];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);

            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState = RHI::DepthStencilState::CreateDisabled();
            pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout = shadingRateRenderAttachmentLayout;
            pipelineDesc.m_renderAttachmentConfiguration.m_subpassIndex = 0;
            pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;

            m_modelPipelineState[0] = shader->AcquirePipelineState(pipelineDesc);
            if (!m_modelPipelineState[0])
            {
                AZ_Error(VariableRateShading::SampleName, false, "Failed to acquire default pipeline state for shader");
                return;
            }

            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);

            RHI::RenderAttachmentLayout rateRenderAttachmentLayout;
            result = attachmentsBuilder.End(rateRenderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");
            pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout = rateRenderAttachmentLayout;

            m_modelPipelineState[1] = shader->AcquirePipelineState(pipelineDesc);
            if (!m_modelPipelineState[1])
            {
                AZ_Error(VariableRateShading::SampleName, false, "Failed to acquire default pipeline state for shader");
                return;
            }
        }

        {
            RHI::PipelineStateDescriptorForDispatch pipelineDesc;
            const auto& shader = m_shaders[1];
            shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId).ConfigurePipelineState(pipelineDesc);

            m_computePipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_computePipelineState)
            {
                AZ_Error(VariableRateShading::SampleName, false, "Failed to acquire default pipeline state for compute");
                return;
            }
        }

        {
            RHI::RenderAttachmentLayoutBuilder attachmentsBuilder;
            attachmentsBuilder.AddSubpass()
                ->RenderTargetAttachment(m_outputFormat);

            RHI::RenderAttachmentLayout renderAttachmentLayout;
            [[maybe_unused]] RHI::ResultCode result = attachmentsBuilder.End(renderAttachmentLayout);
            AZ_Assert(result == RHI::ResultCode::Success, "Failed to create render attachment layout");

            const auto& shader = m_shaders[2];
            auto& variant = shader->GetVariant(AZ::RPI::ShaderAsset::RootShaderVariantStableId);

            RHI::PipelineStateDescriptorForDraw pipelineDesc;
            variant.ConfigurePipelineState(pipelineDesc);
            pipelineDesc.m_renderStates.m_depthStencilState = RHI::DepthStencilState::CreateDisabled();
            pipelineDesc.m_renderAttachmentConfiguration.m_renderAttachmentLayout = renderAttachmentLayout;
            pipelineDesc.m_renderAttachmentConfiguration.m_subpassIndex = 0;
            pipelineDesc.m_inputStreamLayout = m_inputStreamLayout;

            RHI::TargetBlendState& targetBlendState = pipelineDesc.m_renderStates.m_blendState.m_targets[0];
            targetBlendState.m_enable = true;
            targetBlendState.m_blendSource = RHI::BlendFactor::AlphaSource;
            targetBlendState.m_blendDest = RHI::BlendFactor::AlphaSourceInverse;
            targetBlendState.m_blendOp = RHI::BlendOp::Add;

            m_imagePipelineState = shader->AcquirePipelineState(pipelineDesc);
            if (!m_imagePipelineState)
            {
                AZ_Error(VariableRateShading::SampleName, false, "Failed to acquire default pipeline state for shader");
                return;
            }
        }
    }

    void VariableRateShadingExampleComponent::CreateInputAssemblyBuffersAndViews()
    {
        m_bufferPool = aznew RHI::MultiDeviceBufferPool();
        RHI::BufferPoolDescriptor bufferPoolDesc;
        bufferPoolDesc.m_bindFlags = RHI::BufferBindFlags::InputAssembly;
        bufferPoolDesc.m_heapMemoryLevel = RHI::HeapMemoryLevel::Device;
        m_bufferPool->Init(RHI::MultiDevice::AllDevices, bufferPoolDesc);

        struct BufferData
        {
            AZStd::array<VertexPosition, 4> m_positions;
            AZStd::array<VertexUV, 4> m_uvs;
            AZStd::array<uint16_t, 6> m_indices;
        };

        BufferData bufferData;
        SetFullScreenRect(bufferData.m_positions.data(), bufferData.m_uvs.data(), bufferData.m_indices.data());

        m_inputAssemblyBuffer = aznew RHI::MultiDeviceBuffer();
        RHI::ResultCode result = RHI::ResultCode::Success;
        RHI::MultiDeviceBufferInitRequest request;

        request.m_buffer = m_inputAssemblyBuffer.get();
        request.m_descriptor = RHI::BufferDescriptor{ RHI::BufferBindFlags::InputAssembly, sizeof(bufferData) };
        request.m_initialData = &bufferData;
        result = m_bufferPool->InitBuffer(request);
        if (result != RHI::ResultCode::Success)
        {
            AZ_Error(VariableRateShading::SampleName, false, "Failed to initialize buffer with error code %d", result);
            return;
        }

        m_streamBufferViews[0] =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_positions),
            sizeof(BufferData::m_positions),
            sizeof(VertexPosition)
        };

        m_streamBufferViews[1] =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_uvs),
            sizeof(BufferData::m_uvs),
            sizeof(VertexUV)
        };

        m_indexBufferView =
        {
            *m_inputAssemblyBuffer,
            offsetof(BufferData, m_indices),
            sizeof(BufferData::m_indices),
            RHI::IndexFormat::Uint16
        };

        RHI::InputStreamLayoutBuilder layoutBuilder;
        layoutBuilder.AddBuffer()->Channel("POSITION", RHI::Format::R32G32B32_FLOAT);
        layoutBuilder.AddBuffer()->Channel("UV", RHI::Format::R32G32_FLOAT);
        m_inputStreamLayout = layoutBuilder.End();

        RHI::ValidateStreamBufferViews(m_inputStreamLayout, m_streamBufferViews);
    }

    void VariableRateShadingExampleComponent::CreateRenderScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                // Binds the swap chain as a color attachment.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
            bool useImageShadingRate = m_useImageShadingRate && (device->GetFeatures().m_dynamicShadingRateImage || m_frameCount > device->GetDescriptor().m_frameCountMax);
            if (useImageShadingRate)
            {
                // Binds the shading rate image attachment
                AZ::RHI::ImageScopeAttachmentDescriptor dsDesc;
                dsDesc.m_attachmentId = VariableRateShading::ShadingRateAttachmentId;
                dsDesc.m_loadStoreAction.m_loadAction = AZ::RHI::AttachmentLoadAction::Load;
                dsDesc.m_loadStoreAction.m_storeAction = AZ::RHI::AttachmentStoreAction::DontCare;
                frameGraph.UseAttachment(dsDesc, AZ::RHI::ScopeAttachmentAccess::Read, AZ::RHI::ScopeAttachmentUsage::ShadingRate);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        RHI::EmptyCompileFunction<ScopeData> compileFunction;

        const auto executeFunction = [this]([[maybe_unused]] const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
            if (m_useDrawShadingRate)
            {
                RHI::ShadingRateCombinators combinators = { RHI::ShadingRateCombinerOp::Passthrough, m_combinerOp };
                commandList->SetFragmentShadingRate(m_shadingRate, combinators);
            }

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_modelShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };
            // We have to wait until the updating of the initial contents of the shading rate image is done if
            // dynamic mode is not supported (since the CPU would try to read it while the GPU is updating the contents)
            bool useImageShadingRate = m_useImageShadingRate && (device->GetFeatures().m_dynamicShadingRateImage || m_frameCount > device->GetDescriptor().m_frameCountMax);

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_modelPipelineState[useImageShadingRate ? 0 : 1]->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));;
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);

            if (m_useDrawShadingRate)
            {
                commandList->SetFragmentShadingRate(RHI::ShadingRate::Rate1x1);
            }
        };

        const RHI::ScopeId forwardScope("SceneScope");
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

    void VariableRateShadingExampleComponent::CreateComputeScope()
    {
        struct ScopeData
        {
        };

        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        const auto& deviceFeatures = device->GetFeatures();
        // If "m_dynamicShadingRateImage" is not supported we cannot update the same image that is being used as shading rate this frame.
        // We use an "old" one that is not longer in used.
        const char* shadingRateAttachmentId = deviceFeatures.m_dynamicShadingRateImage ? VariableRateShading::ShadingRateAttachmentId : VariableRateShading::ShadingRateAttachmentUpdateId;
        const auto prepareFunction = [this, shadingRateAttachmentId](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            if (m_useImageShadingRate)
            {
                RHI::ImageScopeAttachmentDescriptor shadingRateImageDesc;
                shadingRateImageDesc.m_attachmentId = shadingRateAttachmentId;
                shadingRateImageDesc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::DontCare;
                shadingRateImageDesc.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::Store;
                shadingRateImageDesc.m_imageViewDescriptor.m_overrideFormat = ConvertToUInt(m_rateShadingImageFormat);
                frameGraph.UseShaderAttachment(shadingRateImageDesc, RHI::ScopeAttachmentAccess::Write);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this, shadingRateAttachmentId](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (m_useImageShadingRate)
            {
                Vector2 center = m_cursorPos * m_shadingRateImageSize;
                const auto* shadingRateImageView = context.GetImageView(RHI::AttachmentId(shadingRateAttachmentId));
                m_computeShaderResourceGroup->SetImageView(m_shadingRateIndex, shadingRateImageView);
                m_computeShaderResourceGroup->SetConstant(m_centerIndex, center);
                m_computeShaderResourceGroup->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (!m_useImageShadingRate)
            {
                return;
            }

            RHI::CommandList* commandList = context.GetCommandList();

            RHI::SingleDeviceDispatchItem dispatchItem;
            decltype(dispatchItem.m_shaderResourceGroups) shaderResourceGroups = { { m_computeShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() } };

            RHI::DispatchDirect dispatchArgs;

            dispatchArgs.m_totalNumberOfThreadsX = aznumeric_cast<uint32_t>(m_shadingRateImageSize.GetX());
            dispatchArgs.m_threadsPerGroupX = aznumeric_cast<uint16_t>(m_numThreadsX);
            dispatchArgs.m_totalNumberOfThreadsY = aznumeric_cast<uint32_t>(m_shadingRateImageSize.GetY());
            dispatchArgs.m_threadsPerGroupY = aznumeric_cast<uint16_t>(m_numThreadsY);
            dispatchArgs.m_totalNumberOfThreadsZ = 1;
            dispatchArgs.m_threadsPerGroupZ = aznumeric_cast<uint16_t>(m_numThreadsZ);

            AZ_Assert(dispatchArgs.m_threadsPerGroupX == dispatchArgs.m_threadsPerGroupY, "If the shader source changes, this logic should change too.");
            AZ_Assert(dispatchArgs.m_threadsPerGroupZ == 1, "If the shader source changes, this logic should change too.");

            dispatchItem.m_arguments = dispatchArgs;
            dispatchItem.m_pipelineState = m_computePipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            dispatchItem.m_shaderResourceGroupCount = 1;
            dispatchItem.m_shaderResourceGroups = shaderResourceGroups;

            commandList->Submit(dispatchItem);
        };

        const RHI::ScopeId computeScope("ShadingRateImageCompute");
        m_scopeProducers.emplace_back(
            aznew RHI::ScopeProducerFunction<
            ScopeData,
            decltype(prepareFunction),
            decltype(compileFunction),
            decltype(executeFunction)>(
                computeScope,
                ScopeData{},
                prepareFunction,
                compileFunction,
                executeFunction));
    }

    void VariableRateShadingExampleComponent::CreatImageDisplayScope()
    {
        struct ScopeData
        {
        };

        const auto prepareFunction = [this](RHI::FrameGraphInterface frameGraph, [[maybe_unused]] ScopeData& scopeData)
        {
            {
                // Binds the swap chain as a color attachment.
                RHI::ImageScopeAttachmentDescriptor descriptor;
                descriptor.m_attachmentId = m_outputAttachmentId;
                descriptor.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
                frameGraph.UseColorAttachment(descriptor);
            }

            if (m_showShadingRateImage)
            {
                // Binds the shading rate image for reading (not as attachment)
                RHI::ImageScopeAttachmentDescriptor shadingRateImageDesc;
                shadingRateImageDesc.m_attachmentId = VariableRateShading::ShadingRateAttachmentId;
                shadingRateImageDesc.m_loadStoreAction.m_storeAction = RHI::AttachmentStoreAction::DontCare;
                shadingRateImageDesc.m_imageViewDescriptor.m_overrideFormat = ConvertToUInt(m_rateShadingImageFormat);
                frameGraph.UseShaderAttachment(shadingRateImageDesc, RHI::ScopeAttachmentAccess::Read);
            }

            frameGraph.SetEstimatedItemCount(1);
        };

        const auto compileFunction = [this](const RHI::FrameGraphCompileContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (m_showShadingRateImage)
            {
                const auto* shadingRateImageView = context.GetImageView(RHI::AttachmentId(VariableRateShading::ShadingRateAttachmentId));
                m_imageShaderResourceGroup->SetImageView(m_shadingRateDisplayIndex, shadingRateImageView);
                m_imageShaderResourceGroup->Compile();
            }
        };

        const auto executeFunction = [this](const RHI::FrameGraphExecuteContext& context, [[maybe_unused]] const ScopeData& scopeData)
        {
            if (!m_showShadingRateImage)
            {
                return;
            }

            RHI::CommandList* commandList = context.GetCommandList();

            // Set persistent viewport and scissor state.
            commandList->SetViewports(&m_viewport, 1);
            commandList->SetScissors(&m_scissor, 1);

            const RHI::SingleDeviceShaderResourceGroup* shaderResourceGroups[] = { m_imageShaderResourceGroup->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(context.GetDeviceIndex()).get() };

            RHI::DrawIndexed drawIndexed;
            drawIndexed.m_indexCount = 6;
            drawIndexed.m_instanceCount = 1;

            RHI::SingleDeviceDrawItem drawItem;
            drawItem.m_arguments = drawIndexed;
            drawItem.m_pipelineState = m_imagePipelineState->GetDevicePipelineState(context.GetDeviceIndex()).get();
            auto deviceIndexBufferView{m_indexBufferView.GetDeviceIndexBufferView(context.GetDeviceIndex())};
            drawItem.m_indexBufferView = &deviceIndexBufferView;
            drawItem.m_shaderResourceGroupCount = static_cast<uint8_t>(RHI::ArraySize(shaderResourceGroups));
            drawItem.m_shaderResourceGroups = shaderResourceGroups;
            drawItem.m_streamBufferViewCount = static_cast<uint8_t>(m_streamBufferViews.size());
            AZStd::array<AZ::RHI::SingleDeviceStreamBufferView, 2> deviceStreamBufferViews{m_streamBufferViews[0].GetDeviceStreamBufferView(context.GetDeviceIndex()), 
                    m_streamBufferViews[1].GetDeviceStreamBufferView(context.GetDeviceIndex())};
            drawItem.m_streamBufferViews = deviceStreamBufferViews.data();

            commandList->Submit(drawItem);
        };

        const RHI::ScopeId forwardScope("ImageDisplayScope");
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

    void VariableRateShadingExampleComponent::Deactivate()
    {
        m_imguiSidebar.Deactivate();
        AZ::RHI::RHISystemNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::BusDisconnect();

        m_bufferPool = nullptr;
        m_inputAssemblyBuffer = nullptr;
        m_modelPipelineState[0] = nullptr;
        m_modelPipelineState[1] = nullptr;
        m_imagePipelineState = nullptr;
        m_modelShaderResourceGroup = nullptr;
        m_computeShaderResourceGroup = nullptr;
        m_imageShaderResourceGroup = nullptr;
        m_shaders.clear();
        m_supportedModes.clear();
        m_windowContext = nullptr;
        m_imagePool = nullptr;
        m_shadingRateImages.clear();
        m_scopeProducers.clear();
    }

    void VariableRateShadingExampleComponent::DrawSettings()
    {
        RHI::Ptr<RHI::Device> device = Utils::GetRHIDevice();
        const auto& deviceFeatures = device->GetFeatures();

        ImGui::Spacing();
        if (RHI::CheckBitsAll(deviceFeatures.m_shadingRateTypeMask, RHI::ShadingRateTypeFlags::PerRegion))
        {
            ScriptableImGui::Checkbox("Image Shade Rate", &m_useImageShadingRate);
            if (m_useImageShadingRate)
            {
                ImGui::Indent();
                ScriptableImGui::Checkbox("Show Image", &m_showShadingRateImage);
                ScriptableImGui::Checkbox("Follow Pointer", &m_followPointer);
                ImGui::Unindent();
            }
            else
            {
                m_showShadingRateImage = false;
                m_followPointer = false;
            }
        }

        if (RHI::CheckBitsAll(deviceFeatures.m_shadingRateTypeMask, RHI::ShadingRateTypeFlags::PerDraw))
        {
            ScriptableImGui::Checkbox("Draw Shade Rate", &m_useDrawShadingRate);
            if (m_useDrawShadingRate)
            {
                ImGui::Indent();
                AZStd::vector<const char*> items;
                for(const auto rate : m_supportedModes)
                {
                    items.push_back(ToString(rate));
                }
                int current_item = static_cast<int>(AZStd::distance(m_supportedModes.begin(), AZStd::find(m_supportedModes.begin(), m_supportedModes.end(), m_shadingRate)));
                ScriptableImGui::Combo("Shading Rates", &current_item, items.data(), static_cast<int>(items.size()));
                m_shadingRate = m_supportedModes[current_item];
                ImGui::Unindent();
            }
        }

        if (m_useDrawShadingRate && m_useImageShadingRate)
        {
            AZStd::vector<const char*> items = { "Passthrough", "Override", "Min", "Max" };
            int current_item = static_cast<int>(m_combinerOp);
            ScriptableImGui::Combo("Combiner Op", &current_item, items.data(), static_cast<int>(items.size()));
            m_combinerOp = static_cast<RHI::ShadingRateCombinerOp>(current_item);
        }
        else if(m_useDrawShadingRate)
        {
            m_combinerOp = RHI::ShadingRateCombinerOp::Passthrough;
        }

        if (!m_followPointer)
        {
            m_cursorPos = AZ::Vector2(0.5f, 0.5f);
        }

        m_imguiSidebar.End();
    }    

    bool VariableRateShadingExampleComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        if (m_followPointer)
        {
            const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
            switch (inputChannel.GetState())
            {
            case AzFramework::InputChannel::State::Began:
            case AzFramework::InputChannel::State::Updated: // update the camera rotation
            {
                const AzFramework::InputChannel::PositionData2D* position = nullptr;
                // Mouse or Touch Events
                if (inputChannelId == AzFramework::InputDeviceMouse::SystemCursorPosition ||
                    inputChannelId == AzFramework::InputDeviceTouch::Touch::Index0)
                {
                    position = inputChannel.GetCustomData<AzFramework::InputChannel::PositionData2D>();
                }

                if (position)
                {
                    m_cursorPos = position->m_normalizedPosition;
                }
                break;
            }
            default:
                break;
            }
        }
        return false;
    }
} // namespace AtomSampleViewer
