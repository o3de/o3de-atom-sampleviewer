/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <Atom/RHI/IndexBufferView.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/DrawList.h>

#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <AzFramework/Asset/AssetCatalogBus.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    // This AtomSampleViewer example is to test, profile and visualize StreamingImage streaming process as well as 
    // testing the StreamingImage hot reloading function.
    // It starts loading 36 StreamingImageAssets and create the StreamingImages when each asset is ready.
    // After a StreamingImage is created, it will be drawn on the screen with all the its mips.
    // The mips which are not streamed in are showing are white blocks.
    // When all StreamingImages' mipmaps are streamed in, a profile result would be showing on the screen.
    // For StreamingImage hot reloading test, the example will add a new image file in the 
    // AtomSampleViewer project asset's texture/streaming/ folder. 
    // The file will be loaded and displayed on the top right side of screen. 
    // A switch button under it will overwrite the image with another one. When the changed image got processed by AP, 
    // the new content will be rendered on the screen. 
    class StreamingImageExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::Data::AssetBus::MultiHandler
        , public AZ::TickBus::Handler
        , public AzFramework::AssetCatalogEventBus::Handler
    {
    public:
        AZ_COMPONENT(StreamingImageExampleComponent, "{9AF86786-8B14-4C3E-92CD-3152768E417C}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        StreamingImageExampleComponent() = default;
        ~StreamingImageExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        struct ImageToDraw
        {
            AZ::Data::AssetId m_assetId;
            AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_asset;
            AZ::Data::Instance<AZ::RPI::StreamingImage> m_image;
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_srg;

            void Reset()
            {
                m_assetId = AZ::Data::AssetId{};
                m_asset.Reset();
                m_image.reset();
                m_srg.reset();
            }
        };

        struct Image3dToDraw
        {
            AZ::Data::Instance<AZ::RPI::StreamingImage> m_image;
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_srg;
            uint32_t m_sliceCount;
        };

        // AZ::TickBus::Handler...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;
        
        // AssetBus::Handler...
        /// Used to accept image mip chain asset events.
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // AssetCatalogEventBus::Handler
        void OnCatalogAssetAdded(const AZ::Data::AssetId& /*assetId*/) override;
        void OnCatalogAssetChanged(const AZ::Data::AssetId& /*assetId*/) override;

        // Draw profiling data with Imgui
        void DisplayStreamingProfileData();

        // Submit draw packages for each streaming image
        void DrawImages();

        // Submit draw packages for each 3d streaming image
        void Draw3dImages();

        // Submit draw package for one image
        void DrawImage(const ImageToDraw* imageInfo);

        // Creates resources, resource views, pipeline state, etc. for rendering
        void PrepareRenderData();
        
        void SwitchHotReloadImage();
        void DeleteHotReloadImage();

        // Create 3d images from Cpu Data and prepare them for draw
        void Create3dimages();

        // Helper function to get total file size of a streaming image asset and its sub assets
        AZ::u64 GetImageAssetSize(AZ::RPI::StreamingImage* image);

        // Copy the sourceFile and to destFile. If the destFile already exists, overwrite it. 
        bool CopyFile(const AZStd::string& destFile, const AZStd::string& sourceFile);
        void QueueForLoad(const AZStd::string& filePath);
        
        //[GFX TODO][ATOM-4040] PAL-ify AtomSampleViewer. We disable m_enableHotReloadTest for following platforms as
        //they dont have access to the root dev folder, only the asset cache folder and the hot reload
        //logic cant work without it. This is because these platforms use AtomSampleViewerLauncher instead of AtomSampleViewerApplication
        static bool IsHotReloadTestSupported();

        const static uint32_t TestDDSCount = 36; // For image streaming and profile
        const static uint32_t TestPNGCount = 4; // For non-power-of-two textures
        const AZ::IO::Path TestImageFolder = "Textures/Streaming";
        const AZ::IO::Path ReloadTestImageName = "reloadtest.png";
        // Constants of display area for showing all streaming images.
        // As reference, the window's left bottom is (-1, -1). The window size is (2, 2)
        const float AreaWidth = 1.5f;
        const float AreaHeight = 1.9f;
        const float AreaBottom = -1.0f;
        const float AreaLeft = -1.0f;

        uint32_t m_numImageAssetQueued = 0; // The number of StreamingImageAsset were queued to load
        uint32_t m_numImageCreated = 0;     // The number of StreamingImage objects were created
        uint32_t m_numImageStreamed = 0;    // The number of StreamingImages are fully streamed in.

        // images for streaming profiling
        AZStd::vector<ImageToDraw> m_images;

        // image for hot reloading test
        ImageToDraw m_imageHotReload;
        int m_curSourceImage = 0;
        bool m_enableHotReloadTest = true;
        AZ::Data::AssetId m_reloadingAsset;

        // images for 3D streaming
        AZStd::vector<Image3dToDraw> m_3dImages;
        bool m_viewStreamedImages = true;

        // profile data
        AZ::u64 m_loadImageStart = 0;
        AZ::u64 m_loadImageEnd = 0;
        // The total time of create all the images (include upload their tail mipmaps to device)
        AZ::u64 m_createImageTime = 0;
        // The first time when all image were streamed to their target mip
        AZ::u64 m_streamingImageEnd = 0;
        // The total size of all the .streamingimage assets
        AZ::u64 m_initialImageAssetSize = 0;
        // The total size of all the streaming image assets as well as their mipchain assets
        AZ::u64 m_imageAssetSize = 0;
        
        ImGuiSidebar m_imguiSidebar;

        // Cache the DynamicDraw Interface
        AZ::RPI::DynamicDrawInterface* m_dynamicDraw = nullptr;

        // render related data
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_image3dPipelineState;

        AZ::RHI::DrawListTag m_drawListTag;
        AZ::RHI::DrawListTag m_image3dDrawListTag;

        AZ::Data::Asset<AZ::RPI::ShaderAsset> m_shaderAsset;
        AZ::RHI::Ptr<AZ::RHI::ShaderResourceGroupLayout> m_srgLayout;

        AZ::Data::Asset<AZ::RPI::ShaderAsset> m_image3dShaderAsset;
        AZ::RHI::Ptr<AZ::RHI::ShaderResourceGroupLayout> m_image3dSrgLayout;

        // shader input indices
        AZ::RHI::ShaderInputImageIndex m_imageInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_positionInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_sizeInputIndex;
        AZ::RHI::ShaderInputConstantIndex m_residentMipInputIndex;

        AzFramework::WindowSize m_windowSize;

        // for pause or resume script automation
        bool m_automationPaused = false;

        // save previous settings which need to be recovered when exit the sample.
        size_t m_cachedPoolBudget = 0;
        int16_t m_cachedMipBias = 0;
    };
} // namespace AtomSampleViewer
