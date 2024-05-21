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
#include <Atom/RPI.Public/ColorManagement/TransformColor.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/EntityBus.h>

#include <AzFramework/Entity/EntityContextBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiAssetBrowser.h>
#include <Utils/ImGuiSaveFilePath.h>

namespace AtomSampleViewer
{
    //! This component reuses the scene of tonemapping example to demonstrate the bloom feature
    class BloomExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(BloomExampleComponent, "{F1957895-D74D-4A82-BF0C-055B57AB698A}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        BloomExampleComponent();
        ~BloomExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::EntityBus::MultiHandler...
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        // GUI
        void DrawSidebar();
        
        // Image loading and capture
        AZ::RPI::ColorSpaceId GetColorSpaceIdForIndex(uint8_t colorSpaceIndex) const;
        
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

        // Create bloom entity for rendering
        void CreateBloomEntity();

        // non-owning entity
        AzFramework::EntityContextId m_entityContextId;

        // owning entity
        AZ::Entity* m_bloomEntity = nullptr;

        // Init flag
        bool m_isInitParameters = false;

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
        AZ::RHI::ShaderInputNameIndex m_imageInputIndex = "m_texture";
        AZ::RHI::ShaderInputNameIndex m_positionInputIndex = "m_position";
        AZ::RHI::ShaderInputNameIndex m_sizeInputIndex = "m_size";
        AZ::RHI::ShaderInputNameIndex m_colorSpaceIndex = "m_colorSpace";

        // post processing feature processor
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Render::BloomSettingsInterface* m_bloomSettings = nullptr;

        // Image to display
        ImageToDraw m_drawImage;

        AZ::RPI::ColorSpaceId m_inputColorSpace = AZ::RPI::ColorSpaceId::ACEScg;
        static const char* s_colorSpaceLabels[];
        int m_inputColorSpaceIndex = 0;

        const AZStd::string InputImageFolder = "textures\\bloom\\";

        AZ::Render::DisplayMapperConfigurationDescriptor m_displayMapperConfiguration;

        AZStd::vector<AZStd::string> m_capturePassHierarchy;

        ImGuiAssetBrowser m_imageBrowser;
    };
} // namespace AtomSampleViewer
