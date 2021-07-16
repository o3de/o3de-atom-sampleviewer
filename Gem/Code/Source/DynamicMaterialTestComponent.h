/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <EntityLatticeTestComponent.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiHistogramQueue.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertyDescriptor.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/functional.h>

namespace AtomSampleViewer
{
    //! This test loads a configurable lattice of entities, gives them all a unique Material instance, and
    //! changes a material property value every frame. UI to configure the size of the lattice is included.
    class DynamicMaterialTestComponent final
        : public EntityLatticeTestComponent
        , public AZ::TickBus::Handler
    {
        using Base = EntityLatticeTestComponent;

    public:
        AZ_COMPONENT(DynamicMaterialTestComponent, "{14E4AD00-BB56-4771-809F-1AF7BD3611D9}", EntityLatticeTestComponent);
        DynamicMaterialTestComponent();

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:

        AZ_DISABLE_COPY_MOVE(DynamicMaterialTestComponent);

        void InitMaterialConfigs();

        //! EntityLatticeTestComponent overrides...
        void PrepareCreateLatticeInstances(uint32_t instanceCount) override;
        void CreateLatticeInstance(const AZ::Transform& transform) override;
        void DestroyLatticeInstances() override;
        
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        void CompileWithTimer(AZ::Data::Instance<AZ::RPI::Material> material);

        void UpdateStandardPbrColors();
        void UpdateEmissiveMaterialIntensity();
        void CompileMaterials();

        ImGuiSidebar m_imguiSidebar;
        bool m_pause = false;

        struct MaterialConfig
        {
            AZStd::string m_name;
            AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
            AZStd::function<void()> m_updateLatticeMaterials;
        };
        AZStd::vector<MaterialConfig> m_materialConfigs;
        int m_currentMaterialConfig;

        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandles;
        AZStd::vector<AZ::Data::Instance<AZ::RPI::Material>> m_materials;

        static constexpr AZStd::size_t CompileTimerQueueSize = 30;
        ImGuiHistogramQueue m_compileTimer;

        bool m_waitingForMeshes = false;
        uint32_t m_loadedMeshCounter = 0;
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler> m_meshLoadEventHandlers;

        float m_currentTime = 0.0f;
    };
} // namespace AtomSampleViewer
