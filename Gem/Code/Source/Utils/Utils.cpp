/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Utils/Utils.h>

#include <AzCore/IO/SystemFile.h>
#include <AzCore/Settings/SettingsRegistryMergeUtils.h>

#include <AtomCore/Instance/InstanceDatabase.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAssetCreator.h>
#include <Atom/RPI.Reflect/Image/ImageMipChainAssetCreator.h>
#include <Atom/Utils/DdsFile.h>

#ifdef AZ_PROFILE_TELEMETRY
#include <RADTelemetry/ProfileTelemetryBus.h>
#endif

#include <AzFramework/CommandLine/CommandLine.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Windowing/NativeWindow.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/PlatformIncl.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/Path/Path.h>

#include <Automation/ScriptRepeaterBus.h>

namespace AtomSampleViewer
{
    namespace Utils
    {
        AZ::RHI::Ptr<AZ::RHI::Device> GetRHIDevice()
        {
            using namespace AZ;

            auto* rhiSystem = RHI::RHISystemInterface::Get();
            AZ_Assert(rhiSystem, "Failed to retrieve rpi system.");

            return rhiSystem->GetDevice();
        }

        bool AssetEntryNameGetter(void* data, int index, const char** outName)
        {
            const AssetEntry* assetEntries = reinterpret_cast<const AssetEntry*>(data);

            *outName = assetEntries[index].m_name.c_str();
            return true;
        }

        void ToggleRadTMCapture()
        {
#ifdef AZ_PROFILE_TELEMETRY
            using namespace RADTelemetry;
            using MaskType = AZ::Debug::ProfileCategoryPrimitiveType;

            // Set all the category bits "below" Detailed by default
            static const MaskType defaultCaptureMask = AZ_PROFILE_CAT_TO_RAD_CAPFLAGS(FirstDetailedCategory) - 1;

            static const char* s_telemetryAddress = "127.0.0.1";
            static int32_t s_telemetryPort = 4719;
            static MaskType s_telemetryCaptureMask = defaultCaptureMask;
            static int32_t s_memCaptureEnabled = 0;

            ProfileTelemetryRequestBus::Broadcast(&ProfileTelemetryRequestBus::Events::SetAddress, s_telemetryAddress, s_telemetryPort);

            const MaskType fullCaptureMask = s_telemetryCaptureMask | (s_memCaptureEnabled ? AZ_PROFILE_CAT_TO_RAD_CAPFLAGS(MemoryReserved) : 0);
            ProfileTelemetryRequestBus::Broadcast(&ProfileTelemetryRequestBus::Events::SetCaptureMask, fullCaptureMask);

            ProfileTelemetryRequestBus::Broadcast(&ProfileTelemetryRequestBus::Events::ToggleEnabled);
#endif // AZ_PROFILE_TELEMETRY
        }

        DefaultIBL::~DefaultIBL()
        {
            Reset();
        }

        void DefaultIBL::PreloadAssets()
        {
            const constexpr char* DiffuseAssetPath = "textures/sampleenvironment/examplespecularhdr_cm_ibldiffuse.dds.streamingimage";
            const constexpr char* SpecularAssetPath = "textures/sampleenvironment/examplespecularhdr_cm_iblspecular.dds.streamingimage";

            auto assertTraceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
            if (!m_diffuseImageAsset.IsReady())
            {
                m_diffuseImageAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>(DiffuseAssetPath, assertTraceLevel);
                m_diffuseImageAsset.QueueLoad();
                m_diffuseImageAsset.BlockUntilLoadComplete();
            }

            if (!m_specularImageAsset.IsReady())
            {
                m_specularImageAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::StreamingImageAsset>(SpecularAssetPath, assertTraceLevel);
                m_specularImageAsset.QueueLoad();
                m_specularImageAsset.BlockUntilLoadComplete();
            }
        }

        void DefaultIBL::Init(AZ::RPI::Scene* scene)
        {
            PreloadAssets();

            m_featureProcessor = scene->GetFeatureProcessor<AZ::Render::ImageBasedLightFeatureProcessorInterface>();
            AZ_Assert(m_featureProcessor, "Unabled to find ImageBasedLightFeatureProcessorInterface on scene.");

            m_featureProcessor->SetDiffuseImage(m_diffuseImageAsset);
            m_featureProcessor->SetSpecularImage(m_specularImageAsset);
        }

        void DefaultIBL::SetExposure(float exposure)
        {
            m_featureProcessor->SetExposure(exposure);
        }

        void DefaultIBL::Reset()
        {
            if (m_featureProcessor)
            {
                m_featureProcessor->Reset();
                m_featureProcessor = nullptr;
            }
        }

        void ResizeClientArea(uint32_t width, uint32_t height, const AzFramework::WindowPosOptions& options)
        {
            AzFramework::NativeWindowHandle windowHandle = nullptr;
            AzFramework::WindowSystemRequestBus::BroadcastResult(
                windowHandle,
                &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);

            AzFramework::WindowSize clientAreaSize = {width, height};
            AzFramework::WindowRequestBus::Event(windowHandle, &AzFramework::WindowRequestBus::Events::ResizeClientArea, clientAreaSize, options);
            AzFramework::WindowSize newWindowSize;
            AzFramework::WindowRequestBus::EventResult(newWindowSize, windowHandle, &AzFramework::WindowRequests::GetClientAreaSize);
            AZ_Error("ResizeClientArea", newWindowSize.m_width == width && newWindowSize.m_height == height,
                "Requested window resize to %ux%u but got %ux%u. This display resolution is too low or desktop scaling is too high.",
                width, height, newWindowSize.m_width, newWindowSize.m_height);
        }

        bool SupportsToggleFullScreenOfDefaultWindow()
        {
            return AzFramework::NativeWindow::CanToggleFullScreenStateOfDefaultWindow();
        }

        void ToggleFullScreenOfDefaultWindow()
        {
            AzFramework::NativeWindow::ToggleFullScreenStateOfDefaultWindow();
        }

        void ReportScriptableAction(const char* formatStr, ...)
        {
            va_list args;
            va_start(args, formatStr);
            ScriptRepeaterRequestBus::Broadcast(&ScriptRepeaterRequests::ReportScriptableAction, AZStd::string::format_arg(formatStr, args));
            va_end(args);
        }

        AZ::Data::Instance<AZ::RPI::StreamingImage> GetSolidColorCubemap(uint32_t color)
        {
            constexpr uint32_t width = 4;
            constexpr uint32_t height = 4;

            const AZStd::string assetName = AZStd::string::format("SolidColorBackground_%u", color);
            const AZ::Data::InstanceId instanceId = AZ::Data::InstanceId::CreateName(assetName.c_str());

            // Check for existing image of the same color
            AZ::Data::Instance<AZ::RPI::StreamingImage> existingImage =
                AZ::Data::InstanceDatabase<AZ::RPI::StreamingImage>::Instance().Find(instanceId);
            if (existingImage)
            {
                return existingImage;
            }

            // Build a new cubemap.
            AZ::RHI::Size imageSize = AZ::RHI::Size(width, height, 1);
            const uint32_t pixelSize = sizeof(uint32_t);
            const uint32_t pixelDataSize = width * height * pixelSize;
            AZStd::array<uint32_t, width* height> pixels;
            for (uint32_t& pixelColor : pixels)
            {
                pixelColor = color;
            }

            // Create a new streaming image

            AZ::RPI::StreamingImageAssetCreator imageCreator;
            imageCreator.Begin(AZ::Data::AssetId(instanceId.GetGuid(), 0));

            int32_t arraySize = 6;
            AZ::RHI::Format format = AZ::RHI::Format::R8G8B8A8_UNORM_SRGB;
            AZ::RHI::ImageBindFlags bindFlag = AZ::RHI::ImageBindFlags::ShaderRead;

            AZ::RHI::ImageDescriptor imageDesc = AZ::RHI::ImageDescriptor::Create2DArray(bindFlag, width, height, static_cast<uint16_t>(arraySize), format);
            imageDesc.m_mipLevels = 1;
            imageDesc.m_isCubemap = true;

            imageCreator.SetImageDescriptor(imageDesc);
            imageCreator.SetImageViewDescriptor(AZ::RHI::ImageViewDescriptor::CreateCubemap());

            // Create the mip chain

            AZ::RPI::ImageMipChainAssetCreator mipChainCreator;
            mipChainCreator.Begin(AZ::Data::AssetId(instanceId.GetGuid(), 1), 1, 6);

            uint32_t pitch = width * pixelSize;

            AZ::RHI::DeviceImageSubresourceLayout layout;
            layout.m_bytesPerImage = pixelDataSize;
            layout.m_rowCount = layout.m_bytesPerImage / pitch;
            layout.m_size = AZ::RHI::Size(width, height, 1);
            layout.m_bytesPerRow = pitch;

            mipChainCreator.BeginMip(layout);
            for (uint32_t i = 0; i < 6; ++i)
            {
                mipChainCreator.AddSubImage(pixels.data(), pixelDataSize);
            }
            mipChainCreator.EndMip();
            AZ::Data::Asset<AZ::RPI::ImageMipChainAsset> mipChainAsset;
            mipChainCreator.End(mipChainAsset);

            imageCreator.AddMipChainAsset(*mipChainAsset);

            // Finalize streaming image asset

            AZ::Data::Asset<AZ::RPI::StreamingImageAsset> imageAsset;
            imageCreator.End(imageAsset);

            return AZ::Data::InstanceDatabase<AZ::RPI::StreamingImage>::Instance().FindOrCreate(instanceId, imageAsset);
        }

        AZStd::string ResolvePath(const AZStd::string& path)
        {
            char resolvedPath[AZ_MAX_PATH_LEN] = {0};
            AZ::IO::FileIOBase::GetInstance()->ResolvePath(path.c_str(), resolvedPath, AZ_MAX_PATH_LEN);
            return resolvedPath;
        }

        bool IsFileUnderFolder(AZStd::string filePath, AZStd::string folder)
        {
            AzFramework::StringFunc::Path::Normalize(filePath);
            AzFramework::StringFunc::Path::Normalize(folder);

            AZStd::to_lower(filePath.begin(), filePath.end());
            AZStd::to_lower(folder.begin(), folder.end());
            auto relativePath = AZ::IO::FixedMaxPath(filePath.c_str()).LexicallyRelative(folder.c_str());
            if (!relativePath.empty() && !relativePath.Native().starts_with(".."))
            {
                return true;
            }
            return false;
        }

        bool RunDiffTool(const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            // First let's try to use the user's favorite diff tool. 
            static constexpr AZStd::string_view DiffToolPathKey = "/O3DE/External/DiffTool";

            AZStd::string diffToolPath;
            if (auto registry = AZ::SettingsRegistry::Get())
            {
                registry->Get(diffToolPath, DiffToolPathKey);
            }

            if (!diffToolPath.empty())
            {
                // Does the executable exist?
                if (AZ::IO::SystemFile::Exists(diffToolPath.c_str()))
                {
                    return RunDiffTool_Impl(diffToolPath, filePathA, filePathB);
                }

                AZ_Warning(
                    "ASV::RunDiffTool", false, "The user's diff tool <%s> doesn't exist. Will use the platform default <%s>.\n",
                    diffToolPath.c_str(), GetDefaultDiffToolPath_Impl().c_str());

            }

            // If the key doesn't exist in the registry, or the user specified executable doesn't exist, then
            // use the platform default.
            diffToolPath = GetDefaultDiffToolPath_Impl();
            if (!diffToolPath.empty())
            {
                // Does the executable exist?
                if (AZ::IO::SystemFile::Exists(diffToolPath.c_str()))
                {
                    return RunDiffTool_Impl(diffToolPath, filePathA, filePathB);
                }

                AZ_Warning(
                    "ASV::RunDiffTool", false,
                    "The platform default diff tool <%s> doesn't exist.\n"
                    "You can customize the tool path in the settings registry key: %s.\n",
                    diffToolPath.c_str(),
                    DiffToolPathKey.data());
            }
            else
            {
                AZ_Warning(
                    "ASV::RunDiffTool", false,
                    "No default diff tool has been defined for the current platform.\n"
                    "You can customize the tool path in the settings registry key: %s.\n",
                    DiffToolPathKey.data());
            }

            return false;
        }

    } // namespace Utils
} // namespace AtomSampleViewer
