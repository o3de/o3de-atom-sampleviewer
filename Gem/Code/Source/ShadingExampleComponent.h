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

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>

#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    //!
    //! This component creates a simple scene to test Atom's shading model.
    //! On Windows, by pressing 'L' it will launch luxcoreui.exe for PBR validation.
    //!
    class ShadingExampleComponent final
        : public CommonSampleComponentBase
        , public AzFramework::InputChannelEventListener
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(ShadingExampleComponent, "{91ED709C-1114-46E3-823C-C0B59FB8E4B6}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        ShadingExampleComponent();
        ~ShadingExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        
        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void UseArcBallCameraController();
        void UseNoClipCameraController();
        void RemoveController();

        void SetArcBallControllerParams();

        void LoadIBLImage(const char* imagePath);

        enum class CameraControllerType : int32_t
        {
            ArcBall = 0,
            NoClip,
            Count
        };

        static const uint32_t CameraControllerCount = static_cast<uint32_t>(CameraControllerType::Count);
        static const char* CameraControllerNameTable[CameraControllerCount];
        CameraControllerType m_currentCameraControllerType = CameraControllerType::ArcBall;

        // Owned by this sample
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;

        // Not owned by this sample, we look this up
        AZ::Component* m_cameraControlComponent = nullptr;
        Utils::DefaultIBL m_defaultIbl;

        static constexpr float m_arcballRadiusMinModifier = 0.01f;
        static constexpr float m_arcballRadiusMaxModifier = 2.0f;

        bool m_cameraControllerDisabled = false;

        bool m_renderSent = false;
        bool m_dataLoaded = false;
        bool m_textureReady = false;
        AZ::Data::Instance<AZ::RPI::Image> IBLImage;

    };
} // namespace AtomSampleViewer
