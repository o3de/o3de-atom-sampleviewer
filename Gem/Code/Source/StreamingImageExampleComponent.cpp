/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/DrawPacket.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>

#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Public/Image/StreamingImagePool.h>
#include <Atom/RPI.Public/Image/StreamingImageController.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAssetHandler.h>

#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/time.h>
#include <AzCore/Utils/Utils.h>

#include <AzFramework/API/ApplicationAPI.h>

#include <RHI/BasicRHIComponent.h>
#include <SampleComponentConfig.h>
#include <StreamingImageExampleComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    void StreamingImageExampleComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class < StreamingImageExampleComponent, Component>()
                ->Version(0)
                ;
        }
    }

    void StreamingImageExampleComponent::PrepareRenderData()
    {
        const auto CreatePipeline = [](const char* shaderFilepath,
            const char* srgName,
            Data::Asset<AZ::RPI::ShaderAsset>& shaderAsset,
            RHI::Ptr<AZ::RHI::ShaderResourceGroupLayout>& srgLayout,
            RHI::ConstPtr<RHI::PipelineState>& pipelineState,
            RHI::DrawListTag& drawListTag,
            RPI::Scene* scene)
        {
            // Since the shader is using SV_VertexID and SV_InstanceID as VS input, we won't need to have vertex buffer.
            // Also, the index buffer is not needed with DrawLinear.
            RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;

            shaderAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::ShaderAsset>(shaderFilepath, RPI::AssetUtils::TraceLevel::Error);
            Data::Instance<RPI::Shader> shader = RPI::Shader::FindOrCreate(shaderAsset);

            if (!shader)
            {
                AZ_Error("Render", false, "Failed to find or create shader instance from shader asset with path %s", shaderFilepath);
                return;
            }

            const RPI::ShaderVariant& shaderVariant = shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);
            drawListTag = shader->GetDrawListTag();

            scene->ConfigurePipelineState(shader->GetDrawListTag(), pipelineStateDescriptor);

            pipelineStateDescriptor.m_inputStreamLayout.SetTopology(AZ::RHI::PrimitiveTopology::TriangleStrip);
            pipelineStateDescriptor.m_inputStreamLayout.Finalize();

            pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!pipelineState)
            {
                AZ_Error("Render", false, "Failed to acquire default pipeline state for shader %s", shaderFilepath);
            }

            // Load shader resource group layout
            srgLayout = shaderAsset->FindShaderResourceGroupLayout(AZ::Name(srgName));
        };

        // Create the example's main pipeline object
        {
            CreatePipeline("Shaders/streamingimageexample/imagemips.azshader", "ImageMipsSrg", m_shaderAsset, m_srgLayout, m_pipelineState, m_drawListTag, m_scene);

            // Set the input indices
            m_imageInputIndex = m_srgLayout->FindShaderInputImageIndex(Name("m_texture"));
            m_positionInputIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_position"));
            m_sizeInputIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_size"));
            m_residentMipInputIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_residentMip"));

            // Create an SRG instance for the hot reloaded image
            m_imageHotReload.m_srg = RPI::ShaderResourceGroup::Create(m_shaderAsset, m_srgLayout->GetName());
        }

        // Create the 3D pipeline object
        {
            CreatePipeline("Shaders/streamingimageexample/image3d.azshader", "ImageSrg", m_image3dShaderAsset, m_image3dSrgLayout, m_image3dPipelineState,
                m_image3dDrawListTag, m_scene);
        }
    }

    void StreamingImageExampleComponent::Activate()
    {
        // Save the streaming image pool's budget and streaming image pool controller's mip bias 
        // These would be recovered when exist the example
        Data::Instance<RPI::StreamingImagePool> streamingImagePool = RPI::ImageSystemInterface::Get()->GetSystemStreamingPool();
        m_cachedPoolBudget = streamingImagePool->GetMemoryBudget();
        m_cachedMipBias = streamingImagePool->GetMipBias();

        m_enableHotReloadTest = IsHotReloadTestSupported();

        m_dynamicDraw = RPI::GetDynamicDraw();

        m_imguiSidebar.Activate();

        PrepareRenderData();

        if (m_enableHotReloadTest)
        {
            DeleteHotReloadImage();
        }

        m_loadImageStart = AZStd::GetTimeUTCMilliSecond();

        // Queue load all the textures under Textures\Streaming folder
        for (uint32_t index = 0; index < TestDDSCount; index++)
        {
            AZ::IO::Path filePath = TestImageFolder / AZStd::string::format("streaming%d.dds.streamingimage", index);
            QueueForLoad(filePath.Native());
        }

        // All Images loaded here have non-power-of-two sizes
        for (uint32_t index = 0; index < TestPNGCount; index++)
        {
            AZ::IO::Path filePath = TestImageFolder / AZStd::string::format("streaming%d.png.streamingimage", index);
            QueueForLoad(filePath.Native());
        }

        AZ::TickBus::Handler::BusConnect();

        if (m_enableHotReloadTest)
        {
            AzFramework::AssetCatalogEventBus::Handler::BusConnect();
            SwitchHotReloadImage();
        }

        // Pause lua script (automation) until all the images loaded
        // Use a 10 seconds timeout instead of default one in case of longer loading time
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 10.0f);
        m_automationPaused = true;
    }

    void StreamingImageExampleComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        // Skip processing if the asset is the asset for hot reloading test
        // The original asset was already processed.
        if (m_reloadingAsset == asset.GetId())
        {
            return;
        }

        AZ::Data::AssetBus::MultiHandler::BusDisconnect(asset.GetId());;

        const float ratioYtoX = 1.0f;

        // Create the image for hot reload test and setup its srg for draw
        if (m_imageHotReload.m_assetId == asset.GetId())
        {
            m_imageHotReload.m_image = AZ::RPI::StreamingImage::FindOrCreate(asset);
            m_imageHotReload.m_srg->SetImage(m_imageInputIndex, m_imageHotReload.m_image);

            AZStd::array<float, 2> position{
                { 0.33f, 0.22f }
            };
            AZStd::array<float, 2> size{
                { 0.2f, 0.2f / ratioYtoX }
            };

            m_imageHotReload.m_srg->SetConstant(m_positionInputIndex, position);
            m_imageHotReload.m_srg->SetConstant(m_sizeInputIndex, size);
            m_imageHotReload.m_srg->SetConstant(m_residentMipInputIndex, static_cast<int>(m_imageHotReload.m_image->GetResidentMipLevel()));
            // Note: no need to queue the srg for compile since the OnTick would compile it anyway

            return;
        }

        auto iter = AZStd::find_if(m_images.begin(), m_images.end(), [&asset](const ImageToDraw& image)
        {
            return image.m_assetId == asset.GetId();
        });

        if (iter == m_images.end())
        {
            return;
        }
        ImageToDraw* imageToDraw = iter;
        ptrdiff_t index = AZStd::distance(m_images.begin(), iter);

        AZ::u64 startTime = AZStd::GetTimeUTCMilliSecond();
        imageToDraw->m_image = AZ::RPI::StreamingImage::FindOrCreate(asset);
        AZ::u64 endTime = AZStd::GetTimeUTCMilliSecond();
        m_createImageTime += endTime - startTime;

        AZ::Data::AssetInfo info;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(info, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, imageToDraw->m_image->GetAssetId());
        m_initialImageAssetSize += info.m_sizeBytes;

        imageToDraw->m_srg->SetImage(m_imageInputIndex, imageToDraw->m_image);

        size_t rows = 6;
        size_t columns = (m_images.size() + rows - 1) / rows;
        float xOffset = AreaWidth / columns;
        float yOffset = AreaHeight / rows;
        AZStd::array<float, 2> mipSize;
        if (yOffset * ratioYtoX > xOffset / 1.5f) // the xOffset should be 1.5 times of mip size so it can show lower mips
        {
            mipSize[0] = xOffset / 1.5f;
            mipSize[1] = mipSize[0] / ratioYtoX;
        }
        else
        {
            mipSize[0] = yOffset * ratioYtoX;
            mipSize[1] = yOffset;
        }
        AZStd::array<float, 2> position;
        position[0] = (index / rows) * xOffset + AreaLeft;
        position[1] = (index % rows) * yOffset + AreaBottom;

        imageToDraw->m_srg->SetConstant(m_positionInputIndex, position);
        imageToDraw->m_srg->SetConstant(m_sizeInputIndex, mipSize);
        imageToDraw->m_srg->SetConstant<int>(m_residentMipInputIndex, imageToDraw->m_image->GetResidentMipLevel());
        // no need to queue for compile since the OnTick would compile it anyway

        m_numImageCreated++;

        if (m_numImageAssetQueued == m_numImageCreated)
        {
            // All images created.
            m_loadImageEnd = AZStd::GetTimeUTCMilliSecond();
        }
    }

    void StreamingImageExampleComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (m_reloadingAsset == asset.GetId())
        {
            m_reloadingAsset.SetInvalid();
            AZ::Data::AssetBus::MultiHandler::BusDisconnect(asset.GetId());
        }
    }

    void StreamingImageExampleComponent::OnCatalogAssetChanged(const AZ::Data::AssetId& assetId)
    {
        OnCatalogAssetAdded(assetId);
    }

    void StreamingImageExampleComponent::OnCatalogAssetAdded(const AZ::Data::AssetId& assetId)
    {
        if (!m_imageHotReload.m_assetId.IsValid())
        {
            AZ::IO::Path filePath = TestImageFolder / (ReloadTestImageName.Native() + ".streamingimage");
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                m_imageHotReload.m_assetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, filePath.c_str(),
                azrtti_typeid <AZ::RPI::StreamingImageAsset>(), false);
        }

        if (m_imageHotReload.m_assetId == assetId)
        {
            m_imageHotReload.m_asset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(assetId, AZ::Data::AssetLoadBehavior::PreLoad);
            AZ::Data::AssetBus::MultiHandler::BusConnect(assetId);

            AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
        }
    }

    void StreamingImageExampleComponent::Deactivate()
    {
        if (AzFramework::AssetCatalogEventBus::Handler::BusIsConnected())
        {
            AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
        }
        AZ::TickBus::Handler::BusDisconnect();

        // If there are any assets that haven't finished loading yet, and thus haven't been disconnected, disconnect now.
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();

        if (m_enableHotReloadTest)
        {
            DeleteHotReloadImage();
        }

        m_imguiSidebar.Deactivate();

        m_images.clear();
        m_numImageAssetQueued = 0;
        m_numImageCreated = 0;

        m_imageHotReload.Reset();

        m_pipelineState = nullptr;
        m_drawListTag.Reset();
        m_shaderAsset = {};
        m_srgLayout = nullptr;

        m_3dImages.clear();
        m_image3dShaderAsset = {};
        m_image3dSrgLayout = nullptr;
        m_image3dPipelineState = nullptr;
        m_image3dDrawListTag.Reset();

        // Recover pool budget and mip bias
        Data::Instance<RPI::StreamingImagePool> streamingImagePool = RPI::ImageSystemInterface::Get()->GetSystemStreamingPool();
        streamingImagePool->SetMemoryBudget(m_cachedPoolBudget);
        streamingImagePool->SetMipBias(m_cachedMipBias);
    }

    void StreamingImageExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        // update image streaming states
        uint32_t numStreamed = 0;
        for (auto& imageInfo : m_images)
        {
            if (imageInfo.m_image)
            {
                imageInfo.m_srg->SetConstant<int>(m_residentMipInputIndex, imageInfo.m_image->GetResidentMipLevel());
                imageInfo.m_srg->Compile();
                if (imageInfo.m_image->IsStreamed())
                {
                    numStreamed ++;
                }
            }
        }

        // only need to set the image the first time all images were streamed
        if (m_streamingImageEnd == 0 && numStreamed == m_numImageCreated && m_numImageCreated > 0 && m_numImageAssetQueued == m_numImageCreated)
        {
            m_streamingImageEnd = AZStd::GetTimeUTCMilliSecond();
            for (auto& imageInfo : m_images)
            {
                m_imageAssetSize += GetImageAssetSize(imageInfo.m_image.get());
            }
        }

        if (m_imageHotReload.m_image)
        {
            m_imageHotReload.m_srg->SetConstant<int>(m_residentMipInputIndex, m_imageHotReload.m_image->GetResidentMipLevel());
            m_imageHotReload.m_srg->Compile();
        }

        bool streamingFinished = m_streamingImageEnd > 0;

        // Resume automation when all image streamed and hot reload finished
        if (m_automationPaused && streamingFinished && m_imageHotReload.m_image && m_imageHotReload.m_image->IsStreamed() && !m_reloadingAsset.IsValid())
        {
            ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
            m_automationPaused = false;
        }

        if (m_imguiSidebar.Begin())
        {
            Data::Instance<RPI::StreamingImagePool> streamingImagePool = RPI::ImageSystemInterface::Get()->GetSystemStreamingPool();
            const auto* rhiPool = streamingImagePool->GetRHIPool();
            const RHI::HeapMemoryUsage& memoryUsage = rhiPool->GetHeapMemoryUsage(RHI::HeapMemoryLevel::Device);

            const size_t MB = 1024*1024;
            // streaming image pool info
            ImGui::BeginGroup();
            ImGui::Text("Streaming image pool");
            ImGui::Indent();
            ImGui::Text("Image count: %u", streamingImagePool->GetImageCount());
            ImGui::Text("Streamable image count: %u", streamingImagePool->GetStreamableImageCount());
            size_t totalResident = memoryUsage.m_totalResidentInBytes.load();
            ImGui::Text("Allocated GPU memory: %zu MB (%zu)", totalResident/MB, totalResident);
            size_t usedResident = memoryUsage.m_usedResidentInBytes.load();
            ImGui::Text("Used GPU memory: %zu MB (%zu)", usedResident/MB, usedResident);
            
            size_t budgetInBytes = memoryUsage.m_budgetInBytes;
            int budgetInMB = aznumeric_cast<int>(budgetInBytes/MB);
            ImGui::Text("GPU memory budget: %d MB", budgetInMB);
            if (ScriptableImGui::SliderInt("MB", &budgetInMB, RHI::DeviceStreamingImagePool::ImagePoolMininumSizeInBytes / MB, 512))
            {
                streamingImagePool->SetMemoryBudget(budgetInMB * (1024*1024));
            }

            int mipBias = streamingImagePool->GetMipBias();
            ImGui::Text("Mip bias");
            if (ScriptableImGui::SliderInt("", &mipBias, -aznumeric_cast<int>(RHI::Limits::Image::MipCountMax), RHI::Limits::Image::MipCountMax))
            {
                streamingImagePool->SetMipBias(aznumeric_cast<int16_t>(mipBias));
            }
            ImGui::Unindent();
            ImGui::EndGroup();

            // Draw menus and info when streaming is finished
            if (streamingFinished)
            {
                // For switching rendering content between streamed images or 3d images
                ImGui::Text("Image type view");
                ImGui::Indent();
                const char* buttonLabel = m_viewStreamedImages ? "View 3D Images" : "View Streamed Images";
                if (ScriptableImGui::Button(buttonLabel))
                {
                    m_viewStreamedImages = !m_viewStreamedImages;
                }
                ImGui::Unindent();

                ImGui::Separator();
                ImGui::NewLine();

                // For StreamingImageAsset hot reloading test
                ImGui::Text("Hot Reload Test");

                ImGui::Indent();
                if (m_enableHotReloadTest && m_imageHotReload.m_image && m_imageHotReload.m_image->IsStreamed())
                {
                    if (ScriptableImGui::Button("Switch texture"))
                    {
                        SwitchHotReloadImage();

                        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 10.0f);
                        m_automationPaused = true;
                        m_reloadingAsset = m_imageHotReload.m_assetId;
                        AZ::Data::AssetBus::MultiHandler::BusConnect(m_reloadingAsset);

                    }
                }
                ImGui::Unindent();

                // Display streaming profile result
                ImGui::Separator();
                ImGui::NewLine();
                DisplayStreamingProfileData();
            }

            m_imguiSidebar.End();
        }

        if (m_viewStreamedImages)
        {
            DrawImages();
        }
        else
        {
            if (m_3dImages.size() == 0)
            {
                Create3dimages();
                if (m_3dImages.size() == 0)
                {
                    m_viewStreamedImages = true;
                    return;
                }
            }

            Draw3dImages();

        }
    }

    void StreamingImageExampleComponent::DrawImages()
    {
        for (auto& imageInfo : m_images)
        {
            if (imageInfo.m_image != nullptr)
            {
                DrawImage(&imageInfo);
            }
        }

        if (m_imageHotReload.m_image != nullptr)
        {
            DrawImage(&m_imageHotReload);
        }
    }

    void StreamingImageExampleComponent::Draw3dImages()
    {
        for (Image3dToDraw& image3d : m_3dImages)
        {
            // Build draw packet...
            RHI::DrawPacketBuilder drawPacketBuilder{RHI::MultiDevice::DefaultDevice};
            drawPacketBuilder.Begin(nullptr);
            RHI::DrawLinear drawLinear;
            drawLinear.m_vertexCount = 4;
            drawLinear.m_instanceCount = image3d.m_sliceCount;
            drawPacketBuilder.SetDrawArguments(drawLinear);

            RHI::DrawPacketBuilder::DrawRequest drawRequest;
            drawRequest.m_listTag = m_image3dDrawListTag;
            drawRequest.m_pipelineState = m_image3dPipelineState.get();
            drawRequest.m_sortKey = 0;
            drawRequest.m_uniqueShaderResourceGroup = image3d.m_srg->GetRHIShaderResourceGroup();
            drawPacketBuilder.AddDrawItem(drawRequest);

            // Submit draw packet...
            auto drawPacket{drawPacketBuilder.End()};
            m_dynamicDraw->AddDrawPacket(m_scene, AZStd::move(drawPacket));
        }
    }

    void StreamingImageExampleComponent::DrawImage(const ImageToDraw* imageInfo)
    {
        // Build draw packet...
        RHI::DrawPacketBuilder drawPacketBuilder{RHI::MultiDevice::DefaultDevice};
        drawPacketBuilder.Begin(nullptr);
        RHI::DrawLinear drawLinear;
        drawLinear.m_vertexCount = 4;
        drawLinear.m_instanceCount = imageInfo->m_image->GetMipLevelCount();
        drawPacketBuilder.SetDrawArguments(drawLinear);

        RHI::DrawPacketBuilder::DrawRequest drawRequest;
        drawRequest.m_listTag = m_drawListTag;
        drawRequest.m_pipelineState = m_pipelineState.get();
        drawRequest.m_sortKey = 0;
        drawRequest.m_uniqueShaderResourceGroup = imageInfo->m_srg->GetRHIShaderResourceGroup();
        drawPacketBuilder.AddDrawItem(drawRequest);

        // Submit draw packet...
        auto drawPacket{drawPacketBuilder.End()};
        m_dynamicDraw->AddDrawPacket(m_scene, AZStd::move(drawPacket));
    }

    void StreamingImageExampleComponent::DisplayStreamingProfileData()
    {
        ImGui::BeginGroup();
        ImGui::Text("Streaming Profile");
        ImGui::Indent();
        ImGui::Text("Streaming Image Count: %d", m_numImageCreated);
        ImGui::Text("Load and Create Images: %llu ms", m_loadImageEnd - m_loadImageStart);
        ImGui::Text("Create Images: %llu ms", m_createImageTime);
        ImGui::Text("Streaming Image Mips: %llu ms", m_streamingImageEnd - m_loadImageEnd);
        ImGui::Text("Initial image asset size: %.3f MB", m_initialImageAssetSize / (1024 * 1024.0f));
        ImGui::Text("Total image asset size: %.3f MB", m_imageAssetSize / (1024 * 1024.0f));
        ImGui::Unindent();
        ImGui::EndGroup();
    }

    bool StreamingImageExampleComponent::CopyFile(const AZStd::string& destFile, const AZStd::string& sourceFile)
    {
        IO::FileIOStream fileRead(sourceFile.c_str(), IO::OpenMode::ModeRead | IO::OpenMode::ModeBinary);
        IO::FileIOStream fileWrite(destFile.c_str(), IO::OpenMode::ModeWrite | IO::OpenMode::ModeCreatePath | IO::OpenMode::ModeBinary);

        if (!fileRead.IsOpen() || !fileWrite.IsOpen())
        {
            AZ_Assert(fileRead.IsOpen(), "Failed to open file [%s] for read", sourceFile.c_str());
            AZ_Assert(fileWrite.IsOpen(), "Failed to open file [%s] for write", destFile.c_str());
            return false;
        }
        auto fileLength = fileRead.GetLength();
        AZStd::vector<u8> buffer;
        buffer.resize_no_construct(fileLength);
        fileRead.Read(fileLength, buffer.data());
        fileWrite.Write(fileLength, buffer.data());
        return true;
    }

    void StreamingImageExampleComponent::SwitchHotReloadImage()
    {
        m_curSourceImage = 1 - m_curSourceImage;
        // uses smaller size images for the hot reloading test so the lua automation test won't take very long time
        AZStd::string sourceImageFiles[] = {
            {"streaming1.png"}, {"streaming2.png" }
        };

        AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
        AZ::IO::FixedMaxPath srcPath = projectPath / TestImageFolder / sourceImageFiles[m_curSourceImage];
        AZ::IO::FixedMaxPath destPath = projectPath / TestImageFolder / ReloadTestImageName;

        CopyFile(destPath.String(), srcPath.String());
    }

    void StreamingImageExampleComponent::DeleteHotReloadImage()
    {
        const auto testFileFullPath = AZ::IO::FixedMaxPath(AZ::Utils::GetProjectPath()) / TestImageFolder / ReloadTestImageName;
        if (AZ::IO::SystemFile::Exists(testFileFullPath.c_str()))
        {
            AZ::IO::SystemFile::Delete(testFileFullPath.c_str());
        }
    }

    u64 StreamingImageExampleComponent::GetImageAssetSize(AZ::RPI::StreamingImage* image)
    {
        u64 totalSize = 0;

        AZ::Data::AssetInfo info;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(info, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, image->GetAssetId());
        totalSize += info.m_sizeBytes;

        AZ::Outcome<AZStd::vector<AZ::Data::ProductDependency>, AZStd::string> result = AZ::Failure<AZStd::string>("No response");
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(result, &AZ::Data::AssetCatalogRequestBus::Events::GetDirectProductDependencies, image->GetAssetId());

        if (result.IsSuccess())
        {
            auto& dependencies = result.GetValue();
            for (const auto& dependency : dependencies)
            {
                info.m_sizeBytes = 0;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(info, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, dependency.m_assetId);
                if (info.m_assetType == azrtti_typeid<RPI::ImageMipChainAsset>())
                {
                    totalSize += info.m_sizeBytes;
                }
            }
        }
        return totalSize;
    }

    void StreamingImageExampleComponent::QueueForLoad(const AZStd::string& filePath)
    {
        AZ::Data::AssetId imageAssetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            imageAssetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, filePath.c_str(),
            azrtti_typeid <AZ::RPI::StreamingImageAsset>(), false);

        AZ_Assert(imageAssetId.IsValid(), "Unable to load file %s", filePath.c_str());
        if (imageAssetId.IsValid())
        {
            m_numImageAssetQueued++;
            ImageToDraw img;
            img.m_asset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(imageAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
            img.m_srg = RPI::ShaderResourceGroup::Create(m_shaderAsset, m_srgLayout->GetName());
            img.m_assetId = imageAssetId;
            m_images.push_back(img);

            AZ::Data::AssetBus::MultiHandler::BusConnect(imageAssetId);
        }
    }

    void StreamingImageExampleComponent::Create3dimages()
    {
        // Use this pool to create the streaming images from
        const Data::Instance<RPI::StreamingImagePool>& imagePool = RPI::ImageSystemInterface::Get()->GetSystemStreamingPool();

        // Fetch the shader input indices
        const RHI::ShaderInputImageIndex image3DInputIndex = m_image3dSrgLayout->FindShaderInputImageIndex(Name("m_texture3D"));
        const RHI::ShaderInputConstantIndex sliceCountInputIndex = m_image3dSrgLayout->FindShaderInputConstantIndex(Name("m_sliceCount"));
        const RHI::ShaderInputConstantIndex positionInputIndex = m_image3dSrgLayout->FindShaderInputConstantIndex(Name("m_position"));
        const RHI::ShaderInputConstantIndex sizeInputIndex = m_image3dSrgLayout->FindShaderInputConstantIndex(Name("m_size"));

        // Create a small 3D image, where the row count of an image isn't the same as the height (e.g BC formats). A single slice of the image
        // will be uploaded with a single command
        {
            AZStd::vector<uint8_t> imageData;
            RHI::DeviceImageSubresourceLayout layout;
            RHI::Format format = {};
            BasicRHIComponent::CreateImage3dData(imageData, layout, format, {
                                                            "textures/streaming/streaming13.dds.streamingimage",
                                                            "textures/streaming/streaming14.dds.streamingimage",
                                                            "textures/streaming/streaming15.dds.streamingimage",
                                                            "textures/streaming/streaming16.dds.streamingimage",
                                                            "textures/streaming/streaming17.dds.streamingimage",
                                                            "textures/streaming/streaming19.dds.streamingimage",
                                                            "textures/streaming/streaming20.dds.streamingimage" });

            auto image = RPI::StreamingImage::CreateFromCpuData(*imagePool.get(),
                RHI::ImageDimension::Image3D,
                layout.m_size,
                format, imageData.data(), imageData.size());

            // The image may not be created successfully if the memory budget was set to a very low value
            if (image)
            {
                // Create the srg
                Image3dToDraw image3d;
                image3d.m_srg = RPI::ShaderResourceGroup::Create(m_image3dShaderAsset, m_image3dSrgLayout->GetName());
                image3d.m_srg->SetImage(image3DInputIndex, image.get(), 0);
                image3d.m_sliceCount = layout.m_size.m_depth;

                m_3dImages.emplace_back(image3d);
            }

        };

        // Create large 3D images, where a single image slice is staged/uploaded with multiple commands
        {
            struct V4u32
            {
                float x, y, z, w;
            };

            const auto createColorImageData = [](V4u32 color, uint32_t width, uint32_t height, AZStd::vector<uint8_t>& data)
            {
                const uint32_t imageSize = width * height * sizeof(color);
                data.resize(data.size() + imageSize);
                V4u32* sourceData = reinterpret_cast<V4u32*>(data.data() + data.size() - imageSize);

                for (uint32_t y = 0; y < height; y++)
                {
                    for (uint32_t x = 0; x < width; x++)
                    {
                        memcpy(sourceData, &color, sizeof(color));
                        sourceData++;
                    }
                }
            };

            AZStd::vector<uint8_t> imageData;
            const uint32_t side = 1001;
            const uint32_t depth = 3;
            createColorImageData({ 1.0f,0.0f,0.0f,0.0f }, side, side, imageData);
            createColorImageData({ 0.0f,1.0f,0.0f,0.0f }, side, side, imageData);
            createColorImageData({ 0.0f,0.0f,1.0f,0.0f }, side, side, imageData);

            auto image = RPI::StreamingImage::CreateFromCpuData(*imagePool.get(),
                RHI::ImageDimension::Image3D,
                RHI::Size(side, side, depth),
                RHI::Format::R32G32B32A32_FLOAT, imageData.data(), imageData.size());

            // Create the srg
            Image3dToDraw image3d;
            image3d.m_srg = RPI::ShaderResourceGroup::Create(m_image3dShaderAsset, m_image3dSrgLayout->GetName());
            image3d.m_srg->SetImage(image3DInputIndex, image.get(), 0);
            image3d.m_sliceCount = depth;

            m_3dImages.emplace_back(image3d);
        }

        // Set the constants for all 3d images
        {
            // Rendering area, use the same surface area of the streaming images
            const float ratioYtoX = 1;
            const float seam = 0.01f;
            const uint32_t rows = 6;

            for (uint32_t i = 0; i < m_3dImages.size(); i++)
            {
                Image3dToDraw& image3d = m_3dImages[i];

                const uint32_t columns = image3d.m_sliceCount;
                // Calculate the size
                const float xOffset = AreaWidth / static_cast<float>(columns);
                const float yOffset = AreaHeight / static_cast<float>(rows);

                const float minSize = AZStd::min(xOffset, yOffset);
                const AZStd::array<float, 2> size = { {minSize * ratioYtoX, minSize } };

                // Calculate the position
                AZStd::array<float, 2> position;
                position[0] = AreaLeft;
                position[1] = static_cast<float>(i) * (yOffset + seam) + AreaBottom;

                image3d.m_srg->SetConstant<uint32_t>(sliceCountInputIndex, image3d.m_sliceCount);
                image3d.m_srg->SetConstant(positionInputIndex, position);
                image3d.m_srg->SetConstant(sizeInputIndex, size);

                image3d.m_srg->Compile();
            }

        }
    }
}
