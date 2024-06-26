/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <BloomExampleComponent.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/IO/Path/Path.h>
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

    void BloomExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BloomExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    BloomExampleComponent::BloomExampleComponent()
        : m_imageBrowser("@user@/BloomExampleComponent/image_browser.xml")
    {
    }

    void BloomExampleComponent::Activate()
    {
        m_dynamicDraw = RPI::GetDynamicDraw();

        m_postProcessFeatureProcessor = m_scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessorInterface>();

        CreateBloomEntity();

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

            if (!Utils::IsFileUnderFolder(assetPath, InputImageFolder))
            {
                return false;
            }

            return true;
        });
        m_imageBrowser.Activate();

        // Load a default image
        QueueAssetPathForLoad("textures/tonemapping/hdr_test_pattern.exr.streamingimage");

        Data::Asset<RPI::AnyAsset> displayMapperAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::AnyAsset>("passes/DisplayMapperConfiguration.azasset", RPI::AssetUtils::TraceLevel::Error);
        const Render::DisplayMapperConfigurationDescriptor* displayMapperConfigurationDescriptor = RPI::GetDataFromAnyAsset<Render::DisplayMapperConfigurationDescriptor>(displayMapperAsset);
        if (displayMapperConfigurationDescriptor == nullptr)
        {
            AZ_Error("DisplayMapperPass", false, "Failed to load display mapper configuration file.");
            return;
        }
        m_displayMapperConfiguration = *displayMapperConfigurationDescriptor;
    }

    void BloomExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        AZ::EntityBus::MultiHandler::BusDisconnect();

        if (m_bloomEntity)
        {
            DestroyEntity(m_bloomEntity, GetEntityContextId());
            m_postProcessFeatureProcessor = nullptr;
        }

        m_imguiSidebar.Deactivate();
        m_imageBrowser.Deactivate();
    }

    void BloomExampleComponent::OnEntityDestruction(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::MultiHandler::BusDisconnect(entityId);

        if (m_bloomEntity && m_bloomEntity->GetId() == entityId)
        {
            m_postProcessFeatureProcessor->RemoveSettingsInterface(m_bloomEntity->GetId());
            m_bloomEntity = nullptr;
        }
        else
        {
            AZ_Assert(false, "unexpected entity destruction is signaled.");
        }
    }

    void BloomExampleComponent::OnTick([[maybe_unused]] float deltaTime, AZ::ScriptTimePoint)
    {
        if (m_drawImage.m_image && !m_drawImage.m_wasStreamed)
        {
            m_drawImage.m_wasStreamed = m_drawImage.m_image->GetResidentMipLevel() == 0;

            m_drawImage.m_srg->Compile();
        }

        DrawSidebar();
        DrawImage(&m_drawImage);
    }

    void BloomExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        const char* items[] = { "White", "Red", "Green", "Blue" };
        static int item_current0 = 0;
        static int item_current1 = 0;
        static int item_current2 = 0;
        static int item_current3 = 0;
        static int item_current4 = 0;

        auto ColorPicker = [](int index)->Vector3 {
            switch (index)
            {
            case 0: return Vector3(1.0, 1.0, 1.0); break;
            case 1: return Vector3(1.0, 0.5, 0.5); break;
            case 2: return Vector3(0.5, 1.0, 0.5); break;
            case 3: return Vector3(0.5, 0.5, 1.0); break;
            default: return Vector3(1.0, 1.0, 1.0);
            }
        };

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        ImGui::Spacing();

        bool enabled = m_bloomSettings->GetEnabled();
        if (ImGui::Checkbox("Enable", &enabled) || m_isInitParameters)
        {
            m_bloomSettings->SetEnabled(enabled);
            m_bloomSettings->OnConfigChanged();
        }

        ImGui::Spacing();

        float threshold = m_bloomSettings->GetThreshold();
        if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 20.0f, "%0.1f") || !m_isInitParameters)
        {
            m_bloomSettings->SetThreshold(threshold);
            m_bloomSettings->OnConfigChanged();
        }

        float knee = m_bloomSettings->GetKnee();
        if (ImGui::SliderFloat("Knee", &knee, 0.0f, 1.0f) || m_isInitParameters)
        {
            m_bloomSettings->SetKnee(knee);
            m_bloomSettings->OnConfigChanged();
        }

        ImGui::Spacing();

        float sizeScale = m_bloomSettings->GetKernelSizeScale();
        if (ImGui::SliderFloat("KernelSizeScale", &sizeScale, 0.0f, 2.0f) || m_isInitParameters)
        {
            m_bloomSettings->SetKernelSizeScale(sizeScale);
            m_bloomSettings->OnConfigChanged();
        }

        if (ImGui::CollapsingHeader("KernelSize", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            float kernelSize0 = m_bloomSettings->GetKernelSizeStage0();
            if (ImGui::SliderFloat("Size0", &kernelSize0, 0.0f, 1.0f) || m_isInitParameters)
            {
                m_bloomSettings->SetKernelSizeStage0(kernelSize0);
                m_bloomSettings->OnConfigChanged();
            }

            float kernelSize1 = m_bloomSettings->GetKernelSizeStage1();
            if (ImGui::SliderFloat("Size1", &kernelSize1, 0.0f, 1.0f) || m_isInitParameters)
            {
                m_bloomSettings->SetKernelSizeStage1(kernelSize1);
                m_bloomSettings->OnConfigChanged();
            }

            float kernelSize2 = m_bloomSettings->GetKernelSizeStage2();
            if (ImGui::SliderFloat("Size2", &kernelSize2, 0.0f, 1.0f) || m_isInitParameters)
            {
                m_bloomSettings->SetKernelSizeStage2(kernelSize2);
                m_bloomSettings->OnConfigChanged();
            }

            float kernelSize3 = m_bloomSettings->GetKernelSizeStage3();
            if (ImGui::SliderFloat("Size3", &kernelSize3, 0.0f, 1.0f) || m_isInitParameters)
            {
                m_bloomSettings->SetKernelSizeStage3(kernelSize3);
                m_bloomSettings->OnConfigChanged();
            }

            float kernelSize4 = m_bloomSettings->GetKernelSizeStage4();
            if (ImGui::SliderFloat("Size4", &kernelSize4, 0.0f, 1.0f) || m_isInitParameters)
            {
                m_bloomSettings->SetKernelSizeStage4(kernelSize4);
                m_bloomSettings->OnConfigChanged();
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();

        float intensity = m_bloomSettings->GetIntensity();
        if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f) || m_isInitParameters)
        {
            m_bloomSettings->SetIntensity(intensity);
            m_bloomSettings->OnConfigChanged();
        }

        if (ImGui::CollapsingHeader("Tint", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            if (ImGui::ListBox("Tint0", &item_current0, items, IM_ARRAYSIZE(items), 4) || m_isInitParameters)
            {
                m_bloomSettings->SetTintStage0(ColorPicker(item_current0));
                m_bloomSettings->OnConfigChanged();
            }

            if (ImGui::ListBox("Tint1", &item_current1, items, IM_ARRAYSIZE(items), 4) || m_isInitParameters)
            {
                m_bloomSettings->SetTintStage1(ColorPicker(item_current1));
                m_bloomSettings->OnConfigChanged();
            }

            if (ImGui::ListBox("Tint2", &item_current2, items, IM_ARRAYSIZE(items), 4) || m_isInitParameters)
            {
                m_bloomSettings->SetTintStage2(ColorPicker(item_current2));
                m_bloomSettings->OnConfigChanged();
            }

            if (ImGui::ListBox("Tint3", &item_current3, items, IM_ARRAYSIZE(items), 4) || m_isInitParameters)
            {
                m_bloomSettings->SetTintStage3(ColorPicker(item_current3));
                m_bloomSettings->OnConfigChanged();
            }

            if (ImGui::ListBox("Tint4", &item_current4, items, IM_ARRAYSIZE(items), 4) || m_isInitParameters)
            {
                m_bloomSettings->SetTintStage4(ColorPicker(item_current4));
                m_bloomSettings->OnConfigChanged();
            }

            ImGui::Unindent();
        }

        ImGui::Spacing();

        bool bicubicEnabled = m_bloomSettings->GetBicubicEnabled();
        if (ImGui::Checkbox("Bicubic upsampling", &bicubicEnabled) || m_isInitParameters)
        {
            m_bloomSettings->SetBicubicEnabled(bicubicEnabled);
            m_bloomSettings->OnConfigChanged();
        }

        m_imguiSidebar.End();
    }

    void BloomExampleComponent::PrepareRenderData()
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
            m_imageInputIndex.Reset();
            m_positionInputIndex.Reset();
            m_sizeInputIndex.Reset();
            m_colorSpaceIndex.Reset();
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

    void BloomExampleComponent::DrawImage(const ImageToDraw* imageInfo)
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

    RPI::ColorSpaceId BloomExampleComponent::GetColorSpaceIdForIndex(uint8_t colorSpaceIndex) const
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

    void BloomExampleComponent::CreateBloomEntity()
    {
        m_bloomEntity = CreateEntity("Bloom", GetEntityContextId());

        // Bloom
        auto* bloomSettings = m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_bloomEntity->GetId());
        m_bloomSettings = bloomSettings->GetOrCreateBloomSettingsInterface();
        m_bloomSettings->SetEnabled(false);

        m_bloomEntity->Activate();
        AZ::EntityBus::MultiHandler::BusConnect(m_bloomEntity->GetId());
    }

    void BloomExampleComponent::QueueAssetPathForLoad(const AZStd::string& filePath)
    {
        AZ::Data::AssetId imageAssetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            imageAssetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, filePath.c_str(),
            azrtti_typeid <AZ::RPI::StreamingImageAsset>(), false);
        AZ_Assert(imageAssetId.IsValid(), "Unable to load file %s", filePath.c_str());
        QueueAssetIdForLoad(imageAssetId);
    }

    void BloomExampleComponent::QueueAssetIdForLoad(const AZ::Data::AssetId& imageAssetId)
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
