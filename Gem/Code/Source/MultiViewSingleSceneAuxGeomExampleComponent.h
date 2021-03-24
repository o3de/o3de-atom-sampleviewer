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

#include <Atom/RPI.Public/Base.h>
#include <Atom/RPI.Public/WindowContext.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Windowing/WindowBus.h>
#include <AzFramework/Windowing/NativeWindow.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    
    //! A sample component to demonstrate multiple scenes.
    class MultiViewSingleSceneAuxGeomExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MultiViewSingleSceneAuxGeomExampleComponent, "{B5B97744-407C-467B-AE21-23323454F988}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MultiViewSingleSceneAuxGeomExampleComponent();
        ~MultiViewSingleSceneAuxGeomExampleComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        void OnChildWindowClosed();

    private:        
        // AZ::TickBus::Handler overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void OpenSecondSceneWindow();

        void DrawAuxGeom() const;

        AZ::Component* m_mainCameraControlComponent = nullptr;

        AZStd::unique_ptr<class WindowedView> m_windowedView;
    };

} // namespace AtomSampleViewer
