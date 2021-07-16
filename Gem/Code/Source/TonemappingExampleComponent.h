/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/Feature/DisplayMapper/DisplayMapperConfigurationDescriptor.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/ColorManagement/TransformColor.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <AzCore/Component/TickBus.h>

#include <AzFramework/Entity/EntityContextBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiSaveFilePath.h>

namespace AtomSampleViewer
{
    //! This component creates a simple scene to demonstrate the tonemapping feature by displaying a fullscreen image.
    //! The output from the DisplayMapper pass can also be captured to image.
    class TonemappingExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(TonemappingExampleComponent, "{06777F38-E0DA-490E-9F1C-97274826A0EF}");

        static void Reflect(AZ::ReflectContext* context);

        TonemappingExampleComponent();
        ~TonemappingExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // GUI
        void DrawSidebar();
        const char* GetDisplayMapperOperationTypeLabel() const;

        // Image loading and capture
        void UpdateCapturePassHierarchy();
        void SaveCapture();
        AZ::RPI::ColorSpaceId GetColorSpaceIdForIndex(uint8_t colorSpaceIndex) const;
        void ImageChanged();

        struct ImageToDraw
        {
            AZ::Data::AssetId m_assetId;
            AZ::Data::Instance<AZ::RPI::StreamingImage> m_image;
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_srg;
            bool m_wasStreamed = false;
        };

        // Submit draw package to draw image
        void DrawImage(const ImageToDraw* imageInfo);

        // Creates resources, resource views, pipeline state, etc. for rendering
        void PrepareRenderData();
        void QueueAssetPathForLoad(const AZStd::string& filePath);
        void QueueAssetIdForLoad(const AZ::Data::AssetId& imageAssetId);
        
        // non-owning entity
        AzFramework::EntityContextId m_entityContextId;

        // GUI
        ImGuiSidebar m_imguiSidebar;

        // Cache the DynamicDraw Interface
        AZ::RPI::DynamicDrawInterface* m_dynamicDraw = nullptr;

        // render related data
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::RHI::DrawListTag m_drawListTag;
        AZ::Data::Asset<AZ::RPI::ShaderAsset> m_shaderAsset;
        AZ::RHI::Ptr<AZ::RHI::ShaderResourceGroupLayout> m_srgLayout;

        // shader input indices
        AZ::RHI::ShaderInputImageIndex m_imageInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_positionInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_sizeInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_colorSpaceIndex;

        // Image to display
        ImageToDraw m_drawImage;

        AZ::RPI::ColorSpaceId m_inputColorSpace = AZ::RPI::ColorSpaceId::ACEScg;
        static const char* s_colorSpaceLabels[];
        int m_inputColorSpaceIndex = 0;

        const AZStd::string InputImageFolder = "textures\\tonemapping\\";

        ImGuiSaveFilePath m_imguiFrameCaptureSaver;

        AZ::Render::DisplayMapperConfigurationDescriptor m_displayMapperConfiguration;

        AZStd::vector<AZStd::string> m_capturePassHierarchy;

        ImGuiAssetBrowser m_imageBrowser;
    };
} // namespace AtomSampleViewer
