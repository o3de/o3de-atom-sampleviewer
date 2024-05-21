/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <TonemappingExampleComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Settings/SettingsRegistryMergeUtils.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContextBus.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <EntityUtilityFunctions.h>

#include <Atom/Feature/Utils/FrameCaptureBus.h>

#include <Atom/RHI/DrawPacketBuilder.h>

#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    const char* TonemappingExampleComponent::s_colorSpaceLabels[] =
    {
        "SRGB",
        "LinearSRGB",
        "ACEScg",
        "ACES2065"
    };

    void TonemappingExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TonemappingExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    TonemappingExampleComponent::TonemappingExampleComponent()
        : m_imageBrowser("@user@/TonemappingExampleComponent/image_browser.xml")
        , m_imguiFrameCaptureSaver("@user@/TonemappingExampleComponent/frame_capture.xml")
    {
    }

    void TonemappingExampleComponent::Activate()
    {
        using namespace AZ;

        m_dynamicDraw = RPI::DynamicDrawInterface::Get();

        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();

        PrepareRenderData();

        m_imageBrowser.SetFilter([this](const AZ::Data::AssetInfo& assetInfo)
        {
            if (!AzFramework::StringFunc::Path::IsExtension(assetInfo.m_relativePath.c_str(), "streamingimage"))
            {
                return false;
            }
            AZStd::string assetPath(assetInfo.m_relativePath);
            if (!AzFramework::StringFunc::Path::Normalize(assetPath))
            {
                return false;
            }

            if(!Utils::IsFileUnderFolder(assetPath, InputImageFolder))
            {
                return false;
            }

            return true;
        });
        m_imageBrowser.Activate();

        // Load a default image
        QueueAssetPathForLoad("textures/tonemapping/hdr_test_pattern.exr.streamingimage");

        auto settingsRegistry = AZ::SettingsRegistry::Get();
        AZ::IO::Path writableStoragePath;
        settingsRegistry->Get(writableStoragePath.Native(), AZ::SettingsRegistryMergeUtils::FilePathKey_DevWriteStorage);
        AZ::IO::Path screenshotFolder = writableStoragePath / "Screenshots";

        Data::Asset<RPI::AnyAsset> displayMapperAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::AnyAsset>("passes/DisplayMapperConfiguration.azasset", RPI::AssetUtils::TraceLevel::Error);
        const Render::DisplayMapperConfigurationDescriptor* displayMapperConfigurationDescriptor = RPI::GetDataFromAnyAsset<Render::DisplayMapperConfigurationDescriptor>(displayMapperAsset);
        if (displayMapperConfigurationDescriptor == nullptr)
        {
            AZ_Error("DisplayMapperPass", false, "Failed to load display mapper configuration file.");
            return;
        }
        m_displayMapperConfiguration = *displayMapperConfigurationDescriptor;
        UpdateCapturePassHierarchy();

        m_imguiFrameCaptureSaver.SetDefaultFolder(screenshotFolder.Native());
        m_imguiFrameCaptureSaver.SetDefaultFileName("tonemap_capture");
        m_imguiFrameCaptureSaver.SetAvailableExtensions({ "dds" });
        m_imguiFrameCaptureSaver.Activate();
    }

    void TonemappingExampleComponent::Deactivate()
    {
        using namespace AZ;

        AZ::TickBus::Handler::BusDisconnect();

        m_imguiFrameCaptureSaver.Deactivate();
        m_imguiSidebar.Deactivate();
        m_imageBrowser.Deactivate();
    }

    void TonemappingExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint)
    {
        if (m_drawImage.m_image && !m_drawImage.m_wasStreamed)
        {
            m_drawImage.m_wasStreamed = m_drawImage.m_image->GetResidentMipLevel() == 0;

            m_drawImage.m_srg->Compile();
        }

        DrawSidebar();
        DrawImage(&m_drawImage);
    }

    void TonemappingExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGuiAssetBrowser::WidgetSettings assetBrowserSettings;
        bool imageChanged = false;

        ImGui::Indent();
        {
            assetBrowserSettings.m_labels.m_root = "Tonemapping Test Images";
            imageChanged = m_imageBrowser.Tick(assetBrowserSettings);
        }
        ImGui::Unindent();

        ImGui::Separator();

        if (ImGui::Combo(
            "Input Colorspace",
            &m_inputColorSpaceIndex,
            s_colorSpaceLabels,
            aznumeric_cast<int>(AZStd::size(s_colorSpaceLabels))))
        {
            m_inputColorSpace = GetColorSpaceIdForIndex(static_cast<uint8_t>(m_inputColorSpaceIndex));
            m_drawImage.m_srg->SetConstant<int>(m_colorSpaceIndex, static_cast<int>(m_inputColorSpace));
            m_drawImage.m_srg->Compile();
        }

        // Display the configuration of the display mapper.
        // If a grading LUT is being used, also display the path
        ImGui::Separator();

        ImGui::Text("Display Mapper Settings");
        ImGui::Text("Operation Mode: %s", GetDisplayMapperOperationTypeLabel());

        ImGui::Separator();

        ImGui::Begin("Frame Capture");
        {
            ImGuiSaveFilePath::WidgetSettings settings;
            settings.m_labels.m_filePath = "File Path (.dds):";
            m_imguiFrameCaptureSaver.Tick(settings);

            if (ImGui::Button("Capture"))
            {
                SaveCapture();
            }
        }
        ImGui::End();

        m_imguiSidebar.End();

        if (imageChanged)
        {
            ImageChanged();
        }
    }

    void TonemappingExampleComponent::PrepareRenderData()
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
            CreatePipeline("Shaders/tonemappingexample/renderimage.azshader", "RenderImageSrg", m_shaderAsset, m_srgLayout, m_pipelineState, m_drawListTag, m_scene);

            // Set the input indices
            m_imageInputIndex = m_srgLayout->FindShaderInputImageIndex(Name("m_texture"));
            m_positionInputIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_position"));
            m_sizeInputIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_size"));
            m_colorSpaceIndex = m_srgLayout->FindShaderInputConstantIndex(Name("m_colorSpace"));
        }

        m_drawImage.m_srg = RPI::ShaderResourceGroup::Create(m_shaderAsset, m_srgLayout->GetName());
        m_drawImage.m_wasStreamed = false;

        // Set the image to occupy the full screen.
        // The window's left bottom is (-1, -1). The window size is (2, 2)
        AZStd::array<float, 2> position, size;
        position[0] = -1.f;
        position[1] = -1.f;
        size[0] = 2.0f;
        size[1] = 2.0f;

        m_drawImage.m_srg->SetConstant(m_positionInputIndex, position);
        m_drawImage.m_srg->SetConstant(m_sizeInputIndex, size);
        m_drawImage.m_srg->SetConstant<int>(m_colorSpaceIndex, static_cast<int>(m_inputColorSpace));
    }

    void TonemappingExampleComponent::DrawImage(const ImageToDraw* imageInfo)
    {
        // Build draw packet
        RHI::DrawPacketBuilder drawPacketBuilder{RHI::MultiDevice::DefaultDevice};
        drawPacketBuilder.Begin(nullptr);
        RHI::DrawLinear drawLinear;
        drawLinear.m_vertexCount = 4;
        drawPacketBuilder.SetDrawArguments(drawLinear);

        RHI::DrawPacketBuilder::DrawRequest drawRequest;
        drawRequest.m_listTag = m_drawListTag;
        drawRequest.m_pipelineState = m_pipelineState.get();
        drawRequest.m_sortKey = 0;
        drawRequest.m_uniqueShaderResourceGroup = imageInfo->m_srg->GetRHIShaderResourceGroup();
        drawPacketBuilder.AddDrawItem(drawRequest);

        // Submit draw packet
        auto drawPacket{drawPacketBuilder.End()};
        m_dynamicDraw->AddDrawPacket(m_scene, AZStd::move(drawPacket));
    }

    void TonemappingExampleComponent::UpdateCapturePassHierarchy()
    {
        AZ_Assert(static_cast<Render::DisplayMapperOperationType>(m_displayMapperConfiguration.m_operationType)
            <= Render::DisplayMapperOperationType::Reinhard, "Invalid display mapper operation mode");
        AZStd::array<AZStd::string, 5> displayMapperPasses = {
            "AcesOutputTransform",
            "AcesLutPass",
            "DisplayMapperPassthrough",
            "DisplayMapperOnlyGammaCorrection",
            "OutputTransform"
        };

        m_capturePassHierarchy.clear();
        m_capturePassHierarchy = { "Root", "MainPipeline", "MainPipeline", "DisplayMapperPass", displayMapperPasses[static_cast<int>(m_displayMapperConfiguration.m_operationType)] };
    }

    void TonemappingExampleComponent::ImageChanged()
    {
        AZ::Data::AssetId selectedImageAssetId = m_imageBrowser.GetSelectedAssetId();
        QueueAssetIdForLoad(selectedImageAssetId);
    }

    void TonemappingExampleComponent::SaveCapture()
    {
        AZStd::string filePath = m_imguiFrameCaptureSaver.GetSaveFilePath();
        AZStd::string slot = "Output";
        AZ::Render::FrameCaptureRequestBus::Broadcast(
            &AZ::Render::FrameCaptureRequestBus::Events::CapturePassAttachment,
            filePath,
            m_capturePassHierarchy,
            slot,
            AZ::RPI::PassAttachmentReadbackOption::Output);
    }

    RPI::ColorSpaceId TonemappingExampleComponent::GetColorSpaceIdForIndex(uint8_t colorSpaceIndex) const
    {
        const AZStd::vector<AZ::RPI::ColorSpaceId> colorSpaces =
        {
            RPI::ColorSpaceId::SRGB,
            RPI::ColorSpaceId::LinearSRGB,
            RPI::ColorSpaceId::ACEScg,
            RPI::ColorSpaceId::ACES2065
        };
        if (colorSpaceIndex >= colorSpaces.size())
        {
            AZ_Assert(false, "Invalid colorSpaceIndex");
            return RPI::ColorSpaceId::SRGB;
        }
        return colorSpaces[colorSpaceIndex];
    }
    const char* TonemappingExampleComponent::GetDisplayMapperOperationTypeLabel() const
    {
        AZ_Assert(static_cast<Render::DisplayMapperOperationType>(m_displayMapperConfiguration.m_operationType)
            <= Render::DisplayMapperOperationType::Reinhard, "Invalid display mapper operation mode");
        const char* displayMapperOperationTypeLabels[] =
        {
            "ACES",
            "ACES LUT",
            "Passthrough",
            "Gamma sRGB",
            "Reinhard"
        };
        return displayMapperOperationTypeLabels[static_cast<int>(m_displayMapperConfiguration.m_operationType)];
    }

    void TonemappingExampleComponent::QueueAssetPathForLoad(const AZStd::string& filePath)
    {
        AZ::Data::AssetId imageAssetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            imageAssetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, filePath.c_str(),
            azrtti_typeid <AZ::RPI::StreamingImageAsset>(), false);
        AZ_Assert(imageAssetId.IsValid(), "Unable to load file %s", filePath.c_str());
        QueueAssetIdForLoad(imageAssetId);
    }

    void TonemappingExampleComponent::QueueAssetIdForLoad(const AZ::Data::AssetId& imageAssetId)
    {
        if (imageAssetId.IsValid())
        {
            Data::Asset<AZ::RPI::StreamingImageAsset> imageAsset;
            if (!imageAsset.Create(imageAssetId))
            {
                auto assetId = imageAssetId.ToString<AZStd::string>();
                AZ_Assert(false, "Unable to create image asset for asset ID %s", assetId.c_str());
                return;
            }

            auto image = AZ::RPI::StreamingImage::FindOrCreate(imageAsset);
            if (image == nullptr)
            {
                auto imageAssetName = imageAssetId.ToString<AZStd::string>();
                AZ_Assert(false, "Failed to find or create an image instance from image asset %s", imageAssetName.c_str());
                return;
            }

            m_drawImage.m_assetId = imageAssetId;
            m_drawImage.m_image = image;
            m_drawImage.m_wasStreamed = false;
            m_drawImage.m_srg->SetImage(m_imageInputIndex, m_drawImage.m_image);
        }
    }
} // namespace AtomSampleViewer
