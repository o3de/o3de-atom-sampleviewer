/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>
#include <AzCore/Component/TickBus.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/ImGui/ImGuiUtils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    //!
    //! This component creates a simple scene that tests Screen Space Reflections.
    //!
    class SSRExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AtomSampleViewer::SSRExampleComponent, "{B537E911-48B7-4883-A0F1-4CAAB968BAD4}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        SSRExampleComponent() = default;
        ~SSRExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(SSRExampleComponent);

        void CreateModels();
        void CreateGroundPlane();
        
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;

        void OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model);
        void DrawSidebar();
        void EnableSSR(bool enabled);

        // meshes
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_statueMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_boxMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_shaderBallMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_groundMeshHandle;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_groundMaterialAsset;

        // ground plane material setting
        int m_groundPlaneMaterial = 0;

        // IBL and skybox
        Utils::DefaultIBL m_defaultIbl;
        AZ::Data::Asset<AZ::RPI::StreamingImageAsset> m_skyboxImageAsset;

        ImGuiSidebar m_imguiSidebar;
        bool m_enableSSR = true;
        bool m_resetCamera = true;
        float m_originalFarClipDistance = 0.0f;
    };
} // namespace AtomSampleViewer
