/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <AtomCore/Instance/InstanceId.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>

#include <Utils/Utils.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    class BistroBenchmarkComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public AZ::Data::AssetBus::MultiHandler
    {
    public:
        AZ_COMPONENT(BistroBenchmarkComponent, "{2AFFAA6B-1795-4635-AFAD-C2A98163832F}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        BistroBenchmarkComponent() = default;
        ~BistroBenchmarkComponent() override = default;

        //AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset);

        void BenchmarkLoadStart();
        void FinalizeLoadBenchmarkData();
        void BenchmarkLoadEnd();

        void BenchmarkRunStart();
        void CollectRunBenchmarkData(float deltaTime, AZ::ScriptTimePoint timePoint);
        void FinalizeRunBenchmarkData();
        void BenchmarkRunEnd();

        void DisplayLoadingDialog();
        void DisplayResults();

        struct LoadBenchmarkData
        {
            AZ_TYPE_INFO(LoadBenchmarkData, "{8677FEAB-EBA6-468A-97CB-AAF1B16F5FE2}");

            static void Reflect(AZ::ReflectContext* context);

            struct FileLoadedData
            {
                AZ_TYPE_INFO(FileLoadedData, "{F4E5728D-C526-4C28-83BA-D4B07DC894E8}");

                static void Reflect(AZ::ReflectContext* context);

                AZStd::string m_relativePath;
                AZ::u64 m_bytesLoaded = 0;
            };

            AZStd::string m_name = "";
            double m_timeInSeconds = 0.0;
            double m_totalMBLoaded = 0.0;
            double m_mbPerSec = 0.0;
            AZ::u64 m_numFilesLoaded = 0;
            AZStd::vector<FileLoadedData> m_filesLoaded;
        };

        AZStd::vector<AZ::Data::AssetId> m_trackedAssets;

        struct RunBenchmarkData
        {
            AZ_TYPE_INFO(RunBenchmarkData, "{45FFA85B-1224-4558-B833-3D8A4404041C}");

            static void Reflect(AZ::ReflectContext* context);

            AZStd::string m_name;
            AZStd::vector<float> m_frameTimes; // not serialized
            AZStd::vector<float> m_frameRates; // not serialized
            AZ::u64 m_frameCount;
            double m_timeInSeconds = 0.0;
            double m_timeToFirstFrame = 0.0;
            double m_averageFrameTime = 0.0;
            float m_50pFramesUnder = 0.0f;
            float m_90pFramesUnder = 0.0f;
            float m_minFrameTime = FLT_MAX;
            float m_maxFrameTime = FLT_MIN;

            float m_averageFrameRate = 0.0;
            float m_minFrameRate = FLT_MAX;
            float m_maxFrameRate = FLT_MIN;
        };

        double m_currentTimePointInSeconds = 0.0;

        double m_benchmarkStartTimePoint = 0.0;

        double m_timeToFirstFrame = 0.0;

        LoadBenchmarkData m_currentLoadBenchmarkData;
        RunBenchmarkData m_currentRunBenchmarkData;

        struct CameraPathPoint
        {
            AZ::u64 m_framePoint;
            AZ::Vector3 m_position;
            AZ::Vector3 m_target;
            AZ::Vector3 m_up;
        };
        using CameraPath = AZStd::vector<CameraPathPoint>;

        const CameraPath m_exteriorPath = {
            {0,     AZ::Vector3(8.0f, 0.0, 3.0f), AZ::Vector3(-100.0f, 0.0f, 10.0f), AZ::Vector3(0.0f, 0.0f, 1.0f)},
            {10000,     AZ::Vector3(-8.0f, 0.0, 3.0f), AZ::Vector3(-100.0f, 0.0f, 10.0f), AZ::Vector3(0.0f, 0.0f, 1.0f)},
        };

        bool m_firstResultsDisplay = true;

        bool m_startBenchmarkCapture = true;
        bool m_endBenchmarkCapture = true;

        AZ::u64 m_frameCount = 0;

        CameraPathPoint m_currentCameraPoint;
        CameraPathPoint m_lastCameraPoint;
        

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_sponzaExteriorAsset;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_sponzaInteriorAsset;

        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;
        MeshHandle m_sponzaExteriorMeshHandle;
        MeshHandle m_sponzaInteriorMeshHandle;
        Utils::DefaultIBL m_defaultIbl;

        bool m_sponzaExteriorLoaded = false;
        bool m_sponzaInteriorLoaded = false;

        AZ::Component* m_cameraControlComponent = nullptr;

        AZStd::vector<uint64_t> m_framesToCapture;
        AZStd::string m_screenshotFolder;

        // Lights
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;

        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyboxFeatureProcessor = nullptr;
    };
} // namespace AtomSampleViewer
