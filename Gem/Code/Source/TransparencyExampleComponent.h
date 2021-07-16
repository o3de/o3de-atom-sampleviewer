/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/TickBus.h>
#include <CommonSampleComponentBase.h>
#include <Utils/Utils.h>

namespace AtomSampleViewer
{
    //! Depth sort testing for Transparency object
    class TransparencyExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(TransparencyExampleComponent, "{36988524-5911-45F1-970C-32C889BAB1F1}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        TransparencyExampleComponent();
        ~TransparencyExampleComponent() override = default;

        void Activate() override;
        void Deactivate() override;

    private:

        AZ_DISABLE_COPY_MOVE(TransparencyExampleComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        void Prepare();
        void LoadMesh(AZ::Transform transform);

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandles;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler> m_meshLoadEventHandlers;
        Utils::DefaultIBL m_defaultIbl;

        bool m_waitingForMeshes = false;
        uint32_t m_loadedMeshCounter = 0;
        float total = 0;
        
    };
} // namespace AtomSampleViewer
