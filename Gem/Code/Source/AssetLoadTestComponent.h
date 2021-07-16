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
#include <Utils/ImGuiAssetBrowser.h>
#include <AzCore/Component/TickBus.h>

namespace AtomSampleViewer
{
    /*
        This test loads a configurable lattice of entities and swaps out each entity's model and material
        at given time steps. Each entity can have its assets swapped very rapidly (10ths of seconds).

        The assets that are applied to the entities are chosen from a configurable 
        "allow-list" of models and materials. The allow-list is saved to the cache's user folder and 
        loaded on startup. This makes it easy to chose "working" assets to use in the test vs more development 
        assets that may not be working properly. It also allows you to build cases where we want
        to test instancing more than loading. UI to modify allow-list is a core part of this component.
    */
    class AssetLoadTestComponent final
        : public EntityLatticeTestComponent
        , public AZ::TickBus::Handler
    {
        using Base = EntityLatticeTestComponent;

    public:
        AZ_COMPONENT(AssetLoadTestComponent, "{30E6EE46-2CD5-4903-801F-56AE70A33656}", EntityLatticeTestComponent);

        static void Reflect(AZ::ReflectContext* context);

        AssetLoadTestComponent();
        
        //! AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(AssetLoadTestComponent);

        // CommonSampleComponentBase overrides...
        void OnAllAssetsReadyActivate() override;

        //! EntityLatticeTestComponent overrides...
        void PrepareCreateLatticeInstances(uint32_t instanceCount) override;
        void CreateLatticeInstance(const AZ::Transform& transform) override;
        void FinalizeLatticeInstances() override;
        void DestroyLatticeInstances() override;

        void DestroyHandles();

        AZ::Data::AssetId GetRandomModelId() const;
        AZ::Data::AssetId GetRandomMaterialId() const;

        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;

        struct ModelInstanceData
        {
            AZ::Transform m_transform;
            AZ::Data::AssetId m_modelAssetId;
            AZ::Data::AssetId m_materialAssetId;
            AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        };

        ImGuiSidebar m_imguiSidebar;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiAssetBrowser m_modelBrowser;
        
        AZStd::vector<ModelInstanceData> m_modelInstanceData;

        struct Compare
        {
            bool operator()(const AZ::Data::Asset<AZ::RPI::MaterialAsset>& lhs, const AZ::Data::Asset<AZ::RPI::MaterialAsset>& rhs) const
            {
                if (lhs.GetId().m_guid == rhs.GetId().m_guid)
                {
                    return lhs.GetId().m_subId > rhs.GetId().m_subId;
                }
                return lhs.GetId().m_guid > rhs.GetId().m_guid;
            }
        };

        using MaterialAssetSet = AZStd::set<AZ::Data::Asset<AZ::RPI::MaterialAsset>, Compare>;
        MaterialAssetSet m_cachedMaterials;
        uint32_t m_pinnedMaterialCount = 0;
        
        size_t m_lastPinnedModelCount = 0;
        float m_lastMaterialSwitchInSeconds = 0;
        float m_lastModelSwitchInSeconds = 0;
        float m_materialSwitchTimeInSeconds = 5.0f;
        float m_modelSwitchTimeInSeconds = 3.0f;
        bool m_materialSwitchEnabled = true;
        bool m_modelSwitchEnabled = true;
        bool m_updateTransformEnabled = false;
    };
} // namespace AtomSampleViewer
