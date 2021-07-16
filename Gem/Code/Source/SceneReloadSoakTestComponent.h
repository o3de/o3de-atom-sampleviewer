/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <EntityLatticeTestComponent.h>
#include <ExampleComponentBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>

namespace AtomSampleViewer
{
    //! This test repeatedly loads and unloads a set of models and materials at various intervals with
    //! the specific goal of exposing race conditions in the renderer and the asset system. Some of the 
    //! intervals are intentionally too short, such that assets and instances will be shut down and released
    //! before they are fully loaded, initialized, and sent to the GPU.
    class SceneReloadSoakTestComponent final
        : public EntityLatticeTestComponent
        , public AZ::TickBus::Handler
        , public ExampleComponentRequestBus::Handler
    {
        using Base = EntityLatticeTestComponent;
    public:
        AZ_COMPONENT(SceneReloadSoakTestComponent, "{5D0B03A6-F7B9-4952-90AB-C91306229964}", EntityLatticeTestComponent);
        SceneReloadSoakTestComponent() = default;

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:

        AZ_DISABLE_COPY_MOVE(SceneReloadSoakTestComponent);

        struct TimeSetting
        {
            float resetDelay;   //!< How many seconds to wait before resetting the scene
            uint32_t count;     //!< How many times to use this resetDelay
        };

        // EntityLatticeTestComponent overrides...
        void PrepareCreateLatticeInstances(uint32_t instanceCount) override;
        void CreateLatticeInstance(const AZ::Transform& transform) override;
        void DestroyLatticeInstances() override;

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        // ExampleComponentRequestBus::Handler overrides...
        void ResetCamera() override;

        AZ::SimpleLcgRandom m_random;

        float m_countdown = 0;
        float m_totalTime = 0;

        AZStd::vector<TimeSetting> m_timeSettings;
        uint32_t m_currentSettingIndex = 0; //!< Which m_timeSettings entry is currently being used
        uint32_t m_currentCount = 0;        //!< How many times the current TimeSetting has been used
        uint32_t m_totalResetCount = 0;     //!< Total number of times the scene has been reset since activation

        AZ::Data::AssetId m_materialAssetId;
        AZ::Data::AssetId m_modelAssetId;
        AZStd::vector<bool> m_materialIsUnique; //!< Tracks whether each entity in the lattice uses its own unique material instance
        AZStd::vector<AZ::Render::MeshFeatureProcessorInterface::MeshHandle> m_meshHandles;
    };
} // namespace AtomSampleViewer
