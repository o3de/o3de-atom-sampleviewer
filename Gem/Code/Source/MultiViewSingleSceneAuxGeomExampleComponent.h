/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
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
