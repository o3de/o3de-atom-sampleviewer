/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <BistroBenchmarkComponent.h>

#include <Atom/RHI/Device.h>
#include <Atom/RHI/Factory.h>

#include <Atom/Feature/ImGui/SystemBus.h>

#include <Atom/Feature/Utils/FrameCaptureBus.h>

#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/sort.h>

#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Components/TransformComponent.h>

#include <Utils/Utils.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <ctime>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void BistroBenchmarkComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BistroBenchmarkComponent, AZ::Component>()
                ->Version(0)
                ;
        }
        BistroBenchmarkComponent::LoadBenchmarkData::Reflect(context);
        BistroBenchmarkComponent::LoadBenchmarkData::FileLoadedData::Reflect(context);

        BistroBenchmarkComponent::RunBenchmarkData::Reflect(context);
    }

    void BistroBenchmarkComponent::LoadBenchmarkData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BistroBenchmarkComponent::LoadBenchmarkData>()
                ->Version(0)
                ->Field("Name", &BistroBenchmarkComponent::LoadBenchmarkData::m_name)
                ->Field("TimeInSeconds", &BistroBenchmarkComponent::LoadBenchmarkData::m_timeInSeconds)
                ->Field("TotalMBLoaded", &BistroBenchmarkComponent::LoadBenchmarkData::m_totalMBLoaded)
                ->Field("MB/s", &BistroBenchmarkComponent::LoadBenchmarkData::m_mbPerSec)
                ->Field("# of Files Loaded", &BistroBenchmarkComponent::LoadBenchmarkData::m_numFilesLoaded)
                ->Field("FilesLoaded", &BistroBenchmarkComponent::LoadBenchmarkData::m_filesLoaded)
                ;
        }
    }

    void BistroBenchmarkComponent::LoadBenchmarkData::FileLoadedData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BistroBenchmarkComponent::LoadBenchmarkData::FileLoadedData>()
                ->Version(0)
                ->Field("RelativePath", &BistroBenchmarkComponent::LoadBenchmarkData::FileLoadedData::m_relativePath)
                ->Field("BytesLoaded", &BistroBenchmarkComponent::LoadBenchmarkData::FileLoadedData::m_bytesLoaded)
                ;
        }
    }

    void BistroBenchmarkComponent::RunBenchmarkData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BistroBenchmarkComponent::RunBenchmarkData>()
                ->Version(0)
                ->Field("Name", &BistroBenchmarkComponent::RunBenchmarkData::m_name)
                ->Field("FrameCount", &BistroBenchmarkComponent::RunBenchmarkData::m_frameCount)
                ->Field("TimeToFirstFrame", &BistroBenchmarkComponent::RunBenchmarkData::m_timeToFirstFrame)
                ->Field("TimeInSeconds", &BistroBenchmarkComponent::RunBenchmarkData::m_timeInSeconds)
                ->Field("AverageFrameTime", &BistroBenchmarkComponent::RunBenchmarkData::m_averageFrameTime)
                ->Field("50% of FrameTimes Under", &BistroBenchmarkComponent::RunBenchmarkData::m_50pFramesUnder)
                ->Field("90% of FrameTimes Under", &BistroBenchmarkComponent::RunBenchmarkData::m_90pFramesUnder)
                ->Field("MinFrameTime", &BistroBenchmarkComponent::RunBenchmarkData::m_minFrameTime)
                ->Field("MaxFrameTime", &BistroBenchmarkComponent::RunBenchmarkData::m_maxFrameTime)
                ->Field("AverageFrameRate", &BistroBenchmarkComponent::RunBenchmarkData::m_averageFrameRate)
                ->Field("MinFrameRate", &BistroBenchmarkComponent::RunBenchmarkData::m_minFrameRate)
                ->Field("MaxFrameRate", &BistroBenchmarkComponent::RunBenchmarkData::m_maxFrameRate)
                ;
        }
    }

    void BistroBenchmarkComponent::Activate()
    {
        auto traceLevel = AZ::RPI::AssetUtils::TraceLevel::Assert;
        m_sponzaInteriorAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>
            ("Objects/Sponza.azmodel", traceLevel);

        m_sponzaInteriorMeshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_sponzaInteriorAsset });

        // rotate the entities 180 degrees about Z (the vertical axis)
        // This makes it consistent with how it was positioned in the world when the world was Y-up.
        GetMeshFeatureProcessor()->SetTransform(m_sponzaInteriorMeshHandle, AZ::Transform::CreateRotationZ(AZ::Constants::Pi));

        BenchmarkLoadStart();

        // Capture screenshots on specific frames.
        const AzFramework::CommandLine* commandLine = nullptr;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(commandLine, &AzFramework::ApplicationRequests::GetCommandLine);

        static const char* screenshotFlagName = "screenshot";
        if (commandLine && commandLine->HasSwitch(screenshotFlagName))
        {
            size_t capturesCount = commandLine->GetNumSwitchValues(screenshotFlagName);
            for (size_t i = 0; i < capturesCount; ++i)
            {
                AZStd::string frameNumberStr = commandLine->GetSwitchValue(screenshotFlagName, i);
                uint64_t frameNumber = strtoull(frameNumberStr.begin(), nullptr, 0);
                if (frameNumber > 0)
                {
                    m_framesToCapture.push_back(frameNumber);
                }
            }
            AZStd::sort(m_framesToCapture.begin(), m_framesToCapture.end(), AZStd::greater<uint64_t>());
        }

        AZ::TickBus::Handler::BusConnect();

        const char* engineRoot = nullptr;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(engineRoot, &AzFramework::ApplicationRequests::GetEngineRoot);
        if (engineRoot)
        {
            AzFramework::StringFunc::Path::Join(engineRoot, "Screenshots", m_screenshotFolder, true, false);
        }

        auto defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        m_directionalLightFeatureProcessor = defaultScene->GetFeatureProcessor<AZ::Render::DirectionalLightFeatureProcessorInterface>();

        const auto handle = m_directionalLightFeatureProcessor->AcquireLight();

        AZ::Vector3 sunDirection = AZ::Vector3(1.0f, -1.0f, -3.0f);
        sunDirection.Normalize();
        m_directionalLightFeatureProcessor->SetDirection(handle, sunDirection);

        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> sunColor(AZ::Color(1.0f, 1.0f, 0.97f, 1.0f) * 20.f);
        m_directionalLightFeatureProcessor->SetRgbIntensity(handle, sunColor);
        m_directionalLightFeatureProcessor->SetCascadeCount(handle, 4);
        m_directionalLightFeatureProcessor->SetShadowmapSize(handle, AZ::Render::ShadowmapSizeNamespace::ShadowmapSize::Size2048);
        m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(handle, true);
        m_directionalLightFeatureProcessor->SetShadowFilterMethod(handle, AZ::Render::ShadowFilterMethod::EsmPcf);
        m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(handle, 0.03);
        m_directionalLightFeatureProcessor->SetPredictionSampleCount(handle, 4);
        m_directionalLightFeatureProcessor->SetFilteringSampleCount(handle, 16);
        m_directionalLightFeatureProcessor->SetGroundHeight(handle, 0.f);
        m_directionalLightHandle = handle;

        // Enable physical sky
        m_skyboxFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::SkyBoxFeatureProcessorInterface>(GetEntityContextId());
        AZ_Assert(m_skyboxFeatureProcessor, "BistroBenchmarkComponent unable to find SkyBoxFeatureProcessorInterface.");
        m_skyboxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::PhysicalSky);
        m_skyboxFeatureProcessor->Enable(true);

        float azimuth = atan2(-sunDirection.GetZ(), -sunDirection.GetX());
        float altitude = asin(-sunDirection.GetY() / sunDirection.GetLength());

        m_skyboxFeatureProcessor->SetSunPosition(azimuth, altitude);

        // Create IBL
        m_defaultIbl.Init(AZ::RPI::RPISystemInterface::Get()->GetDefaultScene().get());
        m_defaultIbl.SetExposure(-3.0f);
    }

    void BistroBenchmarkComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        // If there are any assets that haven't finished loading yet, and thus haven't been disconnected, disconnect now.
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();
        
        m_defaultIbl.Reset();

        m_skyboxFeatureProcessor->Enable(false);

        GetMeshFeatureProcessor()->ReleaseMesh(m_sponzaInteriorMeshHandle);

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        m_directionalLightFeatureProcessor = nullptr;
    }

    void BistroBenchmarkComponent::OnTick(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        AZ_PROFILE_DATAPOINT(AZ::Debug::ProfileCategory::AzRender, deltaTime, "Frame Time");

        // Camera Configuration
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(
                config,
                GetCameraEntityId(),
                &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(
                m_directionalLightHandle,
                config);
        }

        // Camera Transform
        {
            AZ::Transform transform = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(
                transform,
                GetCameraEntityId(),
                &AZ::TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(
                m_directionalLightHandle, transform);
        }

        m_currentTimePointInSeconds = timePoint.GetSeconds();

        if (m_sponzaInteriorLoaded == false)
        {
            DisplayLoadingDialog();
        }
        else
        {
            if (m_frameCount >= m_exteriorPath.back().m_framePoint)
            {
                if (m_endBenchmarkCapture)
                {
                    BenchmarkRunEnd();

                    m_endBenchmarkCapture = false;
                }

                DisplayResults();
            }
            else
            {
                if (m_startBenchmarkCapture)
                {
                    BenchmarkRunStart();

                    m_startBenchmarkCapture = false;
                }

                bool screenshotRequested = false;
                // Check if a screenshot was requested for this frame. Loop in case there were multiple requests for the same frame.
                while (m_framesToCapture.size() > 0 && m_frameCount == m_framesToCapture.back())
                {
                    screenshotRequested = true;
                    m_framesToCapture.pop_back();
                }

                if (screenshotRequested)
                {
                    AZStd::string filePath;
                    AZStd::string fileName = AZStd::string::format("screenshot_sponza_%llu.dds", m_frameCount);
                    AzFramework::StringFunc::Path::Join(m_screenshotFolder.c_str(), fileName.c_str(), filePath, true, false);
                    AZ::Render::FrameCaptureRequestBus::Broadcast(&AZ::Render::FrameCaptureRequestBus::Events::CaptureScreenshot, filePath);
                }

                if (m_frameCount == 1)
                {
                    m_timeToFirstFrame = m_currentTimePointInSeconds - m_benchmarkStartTimePoint;
                } 

                CollectRunBenchmarkData(deltaTime, timePoint);

                // Find current working camera point
                size_t currentCameraPointIndex = 0;
                while (m_exteriorPath[currentCameraPointIndex + 1].m_framePoint < m_frameCount && currentCameraPointIndex < (m_exteriorPath.size() - 2))
                {
                    currentCameraPointIndex++;
                }
                const CameraPathPoint& cameraPathPoint = m_exteriorPath[currentCameraPointIndex];
                const CameraPathPoint& nextCameraPathPoint = m_exteriorPath[currentCameraPointIndex + 1];

                // Lerp to get intermediate position
                {
                    const float percentToNextPoint = static_cast<float>((m_frameCount - cameraPathPoint.m_framePoint)) / static_cast<float>(nextCameraPathPoint.m_framePoint - cameraPathPoint.m_framePoint);

                    const AZ::Vector3 position = cameraPathPoint.m_position.Lerp(nextCameraPathPoint.m_position, percentToNextPoint);
                    const AZ::Vector3 target = cameraPathPoint.m_target.Lerp(nextCameraPathPoint.m_target, percentToNextPoint);
                    const AZ::Vector3 up = cameraPathPoint.m_up.Lerp(nextCameraPathPoint.m_up, percentToNextPoint);

                    const AZ::Transform transform = AZ::Transform::CreateLookAt(position, target, AZ::Transform::Axis::YPositive);

                    // Apply transform
                    AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTM, transform);
                }

                m_frameCount++;
            }
        }
    }

    void BistroBenchmarkComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset.GetId() == m_sponzaInteriorAsset.GetId())
        {
            m_sponzaInteriorLoaded = true;
        }

        // Benchmark the count and total size of files loaded
        static double invBytesToMB = 1 / (1024.0 * 1024.0);

        AZ::Data::AssetInfo info;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(info, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, asset.GetId());

        LoadBenchmarkData::FileLoadedData fileLoadedData;
        fileLoadedData.m_relativePath = info.m_relativePath;
        fileLoadedData.m_bytesLoaded = info.m_sizeBytes;

        m_currentLoadBenchmarkData.m_totalMBLoaded += static_cast<double>(info.m_sizeBytes) * invBytesToMB;
        m_currentLoadBenchmarkData.m_filesLoaded.emplace_back(AZStd::move(fileLoadedData));

        if (m_sponzaInteriorLoaded)
        {
            BenchmarkLoadEnd();
        }

        AZ_PROFILE_DATAPOINT(AZ::Debug::ProfileCategory::AzRender, m_currentLoadBenchmarkData.m_totalMBLoaded, "MB Loaded Off Disk");
    }

    void BistroBenchmarkComponent::BenchmarkLoadStart()
    {
        AZStd::vector<AZ::Data::AssetId> unloadedAssetsInCatalog;

        unloadedAssetsInCatalog.push_back(m_sponzaInteriorAsset.GetId());

        // Get a vector of all assets that haven't been loaded
        auto startCB = []() {};
        auto enumerateCB = [&unloadedAssetsInCatalog](const AZ::Data::AssetId id, [[maybe_unused]] const AZ::Data::AssetInfo& assetInfo)
        {
            // Don't "get" the asset and load it, we just want to query its status
            AZ::Data::Asset<AZ::Data::AssetData> asset = AZ::Data::AssetManager::Instance().FindAsset(id, AZ::Data::AssetLoadBehavior::PreLoad);

            if (asset.GetData() == nullptr || asset.GetStatus() == AZ::Data::AssetData::AssetStatus::NotLoaded)
            {
                unloadedAssetsInCatalog.push_back(id);
            }
        };
        auto endCB = []() {};

        AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequestBus::Events::EnumerateAssets, startCB, enumerateCB, endCB);

        // Connect specifically to all assets in the catalog that haven't been loaded yet
        // Otherwise if we just connect to *every* asset the ones that have already been loaded
        // will still trigger OnAssetReady events
        for (const AZ::Data::AssetId& id : unloadedAssetsInCatalog)
        {
            // OnAssetReady will keep track of number and size of all the files that are loaded during this benchmark
            AZ::Data::AssetBus::MultiHandler::BusConnect(id);
        }

        m_currentLoadBenchmarkData = LoadBenchmarkData();
        m_currentLoadBenchmarkData.m_name = "Bistro Load";

        Utils::ToggleRadTMCapture();
        m_benchmarkStartTimePoint = static_cast<double>(AZStd::GetTimeUTCMilliSecond());
    }

    void BistroBenchmarkComponent::FinalizeLoadBenchmarkData()
    {
        m_currentLoadBenchmarkData.m_timeInSeconds = (static_cast<double>(AZStd::GetTimeUTCMilliSecond()) - m_benchmarkStartTimePoint) / 1000.0f;
        m_currentLoadBenchmarkData.m_mbPerSec = m_currentLoadBenchmarkData.m_totalMBLoaded / m_currentLoadBenchmarkData.m_timeInSeconds;
        m_currentLoadBenchmarkData.m_numFilesLoaded = m_currentLoadBenchmarkData.m_filesLoaded.size();
    }

    void BistroBenchmarkComponent::BenchmarkLoadEnd()
    {
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();

        Utils::ToggleRadTMCapture();

        FinalizeLoadBenchmarkData();

        const AZStd::string unresolvedPath = AZStd::string::format("@user@/benchmarks/sponzaLoad_%ld.xml", time(0));
        char sponzaLoadBenchmarkDataFilePath[AZ_MAX_PATH_LEN] = { 0 };
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(unresolvedPath.c_str(), sponzaLoadBenchmarkDataFilePath, AZ_MAX_PATH_LEN);

        if (!AZ::Utils::SaveObjectToFile(sponzaLoadBenchmarkDataFilePath, AZ::DataStream::ST_XML, &m_currentLoadBenchmarkData))
        {
            AZ_Error("BistroBenchmarkComponent", false, "Failed to save sponza benchmark load data to file %s", sponzaLoadBenchmarkDataFilePath);
        }
    }

    void BistroBenchmarkComponent::BenchmarkRunStart()
    {
        m_currentRunBenchmarkData = RunBenchmarkData();
        m_currentRunBenchmarkData.m_name = "Bistro Run";

        Utils::ToggleRadTMCapture();
        m_benchmarkStartTimePoint = m_currentTimePointInSeconds;
    }

    void BistroBenchmarkComponent::CollectRunBenchmarkData(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        const float dtInMS = deltaTime * 1000.0f;

        m_currentRunBenchmarkData.m_frameTimes.push_back(dtInMS);
        m_currentRunBenchmarkData.m_frameCount++;
        m_currentRunBenchmarkData.m_timeInSeconds = timePoint.GetSeconds() - m_benchmarkStartTimePoint;
        if (dtInMS < m_currentRunBenchmarkData.m_minFrameTime)
        {
            m_currentRunBenchmarkData.m_minFrameTime = dtInMS;
        }
        if (dtInMS > m_currentRunBenchmarkData.m_maxFrameTime)
        {
            m_currentRunBenchmarkData.m_maxFrameTime = dtInMS;
        }
        if (m_frameCount == 1)
        {
            m_currentRunBenchmarkData.m_timeToFirstFrame = m_timeToFirstFrame;
        }
    }

    void BistroBenchmarkComponent::FinalizeRunBenchmarkData()
    {
        m_currentRunBenchmarkData.m_averageFrameTime =
            (m_currentRunBenchmarkData.m_timeInSeconds / m_currentRunBenchmarkData.m_frameCount) * 1000.0f;

        // Need to sort the frame times so we can find the 50th and 90th percentile
        AZStd::vector<float> sortedFrameTimes = m_currentRunBenchmarkData.m_frameTimes;
        AZStd::sort(sortedFrameTimes.begin(), sortedFrameTimes.end());

        const size_t frameTimeCount = m_currentRunBenchmarkData.m_frameTimes.size();

        const bool evenNumberOfFrames = frameTimeCount & 1;
        if (!evenNumberOfFrames)
        {
            const size_t medianIndex = frameTimeCount / 2;

            m_currentRunBenchmarkData.m_50pFramesUnder = sortedFrameTimes[medianIndex];
        }
        else
        {
            const size_t medianIndex1 = frameTimeCount / 2;
            const size_t medianIndex2 = medianIndex1 + 1;

            const float median = (sortedFrameTimes[medianIndex1] + sortedFrameTimes[medianIndex2]) / 2.0f;

            m_currentRunBenchmarkData.m_50pFramesUnder = median;
        }

        const float p90Indexf = ceilf(static_cast<float>(frameTimeCount) * .9f);
        const size_t p90Index = static_cast<size_t>(p90Indexf);

        m_currentRunBenchmarkData.m_90pFramesUnder = sortedFrameTimes[p90Index];
        m_currentRunBenchmarkData.m_timeToFirstFrame = m_timeToFirstFrame;

        float averageFrameRate = 0.0f;
        for (auto frame : m_currentRunBenchmarkData.m_frameTimes)
        {
            float frameRate = 1.0f / (frame / 1000);
            if (frameRate > m_currentRunBenchmarkData.m_maxFrameRate)
            {
                m_currentRunBenchmarkData.m_maxFrameRate = frameRate;
            }
            if (frameRate < m_currentRunBenchmarkData.m_minFrameRate)
            {
                m_currentRunBenchmarkData.m_minFrameRate = frameRate;
            }
            m_currentRunBenchmarkData.m_frameRates.push_back(frameRate);
            averageFrameRate += frameRate;
        }
        m_currentRunBenchmarkData.m_averageFrameRate = averageFrameRate / m_currentRunBenchmarkData.m_frameRates.size();
    }

    void BistroBenchmarkComponent::BenchmarkRunEnd()
    {
        Utils::ToggleRadTMCapture();

        FinalizeRunBenchmarkData();

        const AZStd::string unresolvedPath = AZStd::string::format("@user@/benchmarks/sponzaRun_%ld.xml", time(0));

        char sponzaRunBenchmarkDataFilePath[AZ_MAX_PATH_LEN] = { 0 };
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(unresolvedPath.c_str(), sponzaRunBenchmarkDataFilePath, AZ_MAX_PATH_LEN);

        if (!AZ::Utils::SaveObjectToFile(sponzaRunBenchmarkDataFilePath, AZ::DataStream::ST_XML, &m_currentRunBenchmarkData))
        {
            AZ_Error("BistroBenchmarkComponent", false, "Failed to save sponza benchmark run data to file %s", sponzaRunBenchmarkDataFilePath);
        }

    }

    void BistroBenchmarkComponent::DisplayLoadingDialog()
    {
        const ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove;

        AzFramework::NativeWindowHandle windowHandle = nullptr;
        AzFramework::WindowSystemRequestBus::BroadcastResult(
            windowHandle,
            &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);

        AzFramework::WindowSize windowSize;
        AzFramework::WindowRequestBus::EventResult(
            windowSize,
            windowHandle,
            &AzFramework::WindowRequestBus::Events::GetClientAreaSize);

        const float loadingWindowWidth = 225.0f;
        const float loadingWindowHeight = 65.0f;

        const float halfLoadingWindowWidth = loadingWindowWidth * 0.5f;
        const float halfLoadingWindowHeight = loadingWindowHeight * 0.5f;

        const float halfWindowWidth = windowSize.m_width * 0.5f;
        const float halfWindowHeight = windowSize.m_height * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(halfWindowWidth - halfLoadingWindowWidth, halfWindowHeight - halfLoadingWindowHeight));
        ImGui::SetNextWindowSize(ImVec2(loadingWindowWidth, loadingWindowHeight));

        if (ImGui::Begin("Loading", nullptr, windowFlags))
        {
            const size_t loadingIndicatorSize = static_cast<size_t>(fmod(m_currentTimePointInSeconds, 2.0) / 0.5);
            char* loadingIndicator = new char[loadingIndicatorSize + 1];
            memset(loadingIndicator, '.', loadingIndicatorSize);
            loadingIndicator[loadingIndicatorSize] = '\0';

            if (m_sponzaInteriorLoaded)
            {
                ImGui::Text("Bistro Interior: Loaded!");
            }
            else
            {
                ImGui::Text("Bistro Interior: Loading%s", loadingIndicator);
            }

            delete[] loadingIndicator;
        }
        ImGui::End();
    }

    void BistroBenchmarkComponent::DisplayResults()
    {
        AzFramework::NativeWindowHandle windowHandle = nullptr;
        AzFramework::WindowSystemRequestBus::BroadcastResult(
            windowHandle,
            &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);

        AzFramework::WindowSize windowSize;
        AzFramework::WindowRequestBus::EventResult(
            windowSize,
            windowHandle,
            &AzFramework::WindowRequestBus::Events::GetClientAreaSize);

        const float halfWindowWidth = windowSize.m_width * 0.5f;
        const float halfWindowHeight = windowSize.m_height * 0.5f;

        const float frameTimeWindowWidth = static_cast<float>(windowSize.m_width);
        const float frameTimeWindowHeight = 200.0f;

        if (m_firstResultsDisplay)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, windowSize.m_height - frameTimeWindowHeight));
            ImGui::SetNextWindowSize(ImVec2(frameTimeWindowWidth, frameTimeWindowHeight));
        }

        if (ImGui::Begin("Frame Times"))
        {
            ImGui::PlotHistogram("##FrameTimes",
                m_currentRunBenchmarkData.m_frameTimes.data(),
                static_cast<int32_t>(m_currentRunBenchmarkData.m_frameTimes.size()),
                0, nullptr,
                0,
                m_currentRunBenchmarkData.m_90pFramesUnder * 2.0f,
                ImGui::GetContentRegionAvail());
        }
        ImGui::End();

        const float resultWindowWidth = 500.0f;
        const float resultWindowHeight = 250.0f;

        const float halfResultWindowWidth = resultWindowWidth * 0.5f;
        const float halfResultWindowHeight = resultWindowHeight * 0.5f;

        if (m_firstResultsDisplay)
        {
            ImGui::SetNextWindowPos(ImVec2(halfWindowWidth - halfResultWindowWidth,
                halfWindowHeight - halfResultWindowHeight));
            ImGui::SetNextWindowSize(ImVec2(resultWindowWidth, resultWindowHeight));

            m_firstResultsDisplay = false;
        }

        if (ImGui::Begin("Results"))
        {
            ImGui::Columns(2);
            
            ImGui::Text("Load");
            ImGui::NextColumn();
            ImGui::Text("Run");
            ImGui::Separator();

            ImGui::NextColumn();

            ImGui::Text("File Count: %llu", m_currentLoadBenchmarkData.m_numFilesLoaded);
            ImGui::Text("Total Time: %f seconds", m_currentLoadBenchmarkData.m_timeInSeconds);
            ImGui::Text("Loaded: %f MB", m_currentLoadBenchmarkData.m_totalMBLoaded);
            ImGui::Text("Throughput: %f MB/s", m_currentLoadBenchmarkData.m_mbPerSec);

            ImGui::NextColumn();

            ImGui::Text("Frame Count: %llu", m_currentRunBenchmarkData.m_frameCount);
            ImGui::Text("Total Time: %f seconds", m_currentRunBenchmarkData.m_timeInSeconds);
            ImGui::Text("Time to First Frame: %f ms", m_currentRunBenchmarkData.m_timeToFirstFrame);
            ImGui::Text("Average Frame Time: %f ms", m_currentRunBenchmarkData.m_averageFrameTime);
            ImGui::Text("50%% Frames Under: %f ms", m_currentRunBenchmarkData.m_50pFramesUnder);
            ImGui::Text("90%% Frames Under: %f ms", m_currentRunBenchmarkData.m_90pFramesUnder);
            ImGui::Text("Min Frame Time: %f ms", m_currentRunBenchmarkData.m_minFrameTime);
            ImGui::Text("Max Frame Time: %f ms", m_currentRunBenchmarkData.m_maxFrameTime);
            ImGui::Text("Average Frame Rate: %f Hz", m_currentRunBenchmarkData.m_averageFrameRate);
            ImGui::Text("Min Frame Rate: %f Hz", m_currentRunBenchmarkData.m_minFrameRate);
            ImGui::Text("Max Frame Rate: %f Hz", m_currentRunBenchmarkData.m_maxFrameRate);
        }
        ImGui::Columns(1);
        ImGui::End();
    }
} // namespace AtomSampleViewer
