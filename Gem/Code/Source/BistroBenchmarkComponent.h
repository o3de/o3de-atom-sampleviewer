/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
            {0,     AZ::Vector3(13.924f, 42.245f, 2.761f), AZ::Vector3(13.661f, 41.280f, 2.748f), AZ::Vector3(0.006f, 0.0636f, 0.998f)},
            {500,   AZ::Vector3(11.651f, 35.829f, 2.761f), AZ::Vector3(11.389f, 34.864f, 2.748f), AZ::Vector3(0.006f, 0.0636f, 0.998f)},
            {1000,  AZ::Vector3(6.611f, 25.962f, 2.737f),  AZ::Vector3(6.037f, 25.143f, 2.733f),  AZ::Vector3(0.045f, 0.056f, 0.997f)},
            {1500,  AZ::Vector3(2.655f, 21.641f, 2.737f),  AZ::Vector3(2.081f, 20.822f, 2.733f),  AZ::Vector3(0.045f, 0.056f, 0.997f)},
            {2000,  AZ::Vector3(-1.235f, 14.989f, 1.450f), AZ::Vector3(-0.897f, 14.054f, 1.346f), AZ::Vector3(0.017f, -0.024f, 0.999f)},
            {2500,  AZ::Vector3(-4.379f, 10.890f, 1.589f), AZ::Vector3(-3.842f, 10.058f, 1.454f), AZ::Vector3(0.025f, -0.055f, 0.998f)},
            {3000,  AZ::Vector3(-7.152f, 4.652f, 1.369f),  AZ::Vector3(-6.159f, 4.749f, 1.317f),  AZ::Vector3(-0.024, -0.010f, 0.999f)},
            {3500,  AZ::Vector3(-3.468f, -1.993f, 3.570f),   AZ::Vector3(-2.898f, -1.187f, 3.406f),   AZ::Vector3(0.052f, 0.071f, 0.996f)},
            {4000,  AZ::Vector3(1.469f, -4.083f, 3.076f),    AZ::Vector3(1.924f, -3.197f, 2.982f),    AZ::Vector3(0.009f, 0.016f, 0.999f)},
            {4500,  AZ::Vector3(7.631f, -5.290f, 3.476f),    AZ::Vector3(8.571f, -5.631f, 3.501f),    AZ::Vector3(-0.096f, 0.032f, 0.995f)},
            {5000,  AZ::Vector3(28.078f, -13.430f, 3.165f),  AZ::Vector3(29.019f, -13.766f, 3.145f),  AZ::Vector3(-0.055f, 0.015f, 0.998f)},
            {5500,  AZ::Vector3(37.032f, -21.699f, 3.082f),  AZ::Vector3(38.013f, -21.507f, 3.068f),  AZ::Vector3(-0.067f, 0.015f, 0.998f)},
            {6000,  AZ::Vector3(40.579f, -27.793f, 2.948f),  AZ::Vector3(41.019f, -26.895f, 2.954f),  AZ::Vector3(-0.054f, -0.065f, 0.996f)},
            {6500,  AZ::Vector3(52.912f, -27.212f, 3.632f),  AZ::Vector3(52.160f, -26.554f, 3.593f),  AZ::Vector3(0.026f, -0.027f, 0.999f)},
            {7000,  AZ::Vector3(51.839f, -13.224f, 4.111f),  AZ::Vector3(51.241f, -14.023f, 4.044f),  AZ::Vector3(0.013f, 0.002f, 0.999f)},
            {7500,  AZ::Vector3(36.274f, -14.114f, 3.613f),  AZ::Vector3(35.314f, -13.836f, 3.566f),  AZ::Vector3(0.026f, -0.016f, 0.999f)},
            {8000,  AZ::Vector3(19.405f, -9.554f, 3.259f),   AZ::Vector3(18.459f, -9.229f, 3.248f),   AZ::Vector3(0.059f, -0.025f, 0.997f)},
            {8500,  AZ::Vector3(7.768f, -4.673f, 2.460f),    AZ::Vector3(7.320f, -3.786f, 2.345f),    AZ::Vector3(-0.047f, 0.019f, 0.998f)},
            {9000,  AZ::Vector3(5.322f, 0.631f, 1.789f),   AZ::Vector3(4.787f, 1.470f, 1.684f),   AZ::Vector3(-0.043f, 0.006f, 0.999f)},
            {9500,  AZ::Vector3(-0.990f, 2.766f, 1.955f),  AZ::Vector3(-1.060f, 3.759f, 1.867f),  AZ::Vector3(-0.029f, 0.009f, 0.999f)},
            {10000, AZ::Vector3(-6.400f, 6.291f, 1.320f),  AZ::Vector3(-5.431f, 6.222f, 1.086f),  AZ::Vector3(0.160f, 0.007f, 0.987f)},
        };

        bool m_firstResultsDisplay = true;

        bool m_startBenchmarkCapture = true;
        bool m_endBenchmarkCapture = true;

        AZ::u64 m_frameCount = 0;

        CameraPathPoint m_currentCameraPoint;
        CameraPathPoint m_lastCameraPoint;
        

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_bistroExteriorAsset;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_bistroInteriorAsset;

        using MeshHandle = AZ::Render::MeshFeatureProcessorInterface::MeshHandle;
        MeshHandle m_bistroExteriorMeshHandle;
        MeshHandle m_bistroInteriorMeshHandle;
        Utils::DefaultIBL m_defaultIbl;

        bool m_bistroExteriorLoaded = false;
        bool m_bistroInteriorLoaded = false;

        AZ::Component* m_cameraControlComponent = nullptr;

        AZStd::vector<uint64_t> m_framesToCapture;
        AZStd::string m_screenshotFolder;

        // Lights
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;

        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyboxFeatureProcessor = nullptr;
    };
} // namespace AtomSampleViewer
