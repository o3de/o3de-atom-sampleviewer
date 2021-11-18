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
        This test loads a 22x22x22 lattice of entities with randomized meshes and materials.

        The assets that are applied to the entities are chosen from the 
        "allow-list" of models and materials.
    */
    class HighInstanceTestComponent final
        : public EntityLatticeTestComponent
        , public AZ::TickBus::Handler
    {
        using Base = EntityLatticeTestComponent;

    public:
        AZ_COMPONENT(HighInstanceTestComponent, "{DAA2B63B-7CC0-4696-A44F-49E53C6390B9}", EntityLatticeTestComponent);

        static void Reflect(AZ::ReflectContext* context);

        HighInstanceTestComponent();
        
        //! AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:
        AZ_DISABLE_COPY_MOVE(HighInstanceTestComponent);

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
        bool m_updateTransformEnabled = false;
    };
} // namespace AtomSampleViewer
