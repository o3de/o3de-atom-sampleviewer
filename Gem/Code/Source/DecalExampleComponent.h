/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <Atom/Feature/Decals/DecalFeatureProcessorInterface.h>
#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include "DecalContainer.h"

namespace AtomSampleViewer
{
    class DecalContainer;

    //! This component creates a simple scene to test Atom's decal system.
    class DecalExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:

        AZ_COMPONENT(DecalExampleComponent, "{91CFCFFC-EDD9-47EB-AE98-4BE9617D6F2F}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;


        void Deactivate() override;

    private:

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void CreateDecalContainer();
        void CreatePlaneObject();
        void ScaleObjectToFitDecals();
        void EnableArcBallCameraController();
        void ConfigureCameraToLookDownAtObject();
        void AddImageBasedLight();
        void DrawSidebar();
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        Utils::DefaultIBL m_defaultIbl;
        AZStd::unique_ptr<DecalContainer> m_decalContainer;
        // Used to test the DecalFeatureProcessor::Clone() function
        AZStd::unique_ptr<DecalContainer> m_decalContainerClone;
        ImGuiSidebar m_imguiSidebar;
        bool m_cloneDecalsEnabled = false;

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;
    };
} // namespace AtomSampleViewer
