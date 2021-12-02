/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AssetLoadTestComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Debug/EventTrace.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    void AssetLoadTestComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AssetLoadTestComponent, EntityLatticeTestComponent>()
                ->Version(0)
                ;
        }
    }


    AssetLoadTestComponent::AssetLoadTestComponent() 
        : m_materialBrowser("@user@/AssetLoadTestComponent/material_browser.xml")
        , m_modelBrowser("@user@/AssetLoadTestComponent/model_browser.xml")
        , m_imguiSidebar("@user@/AssetLoadTestComponent/sidebar.xml")
    {
        m_sampleName = "AssetLoadTestComponent";

        m_materialBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::MaterialAsset>();
        });

        m_modelBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
        });

        const AZStd::vector<AZStd::string> defaultMaterialAllowlist =
        {
            "materials/defaultpbr.azmaterial",
            "materials/presets/pbr/metal_aluminum_polished.azmaterial",
            "shaders/staticmesh_colorr.azmaterial",
            "shaders/staticmesh_colorg.azmaterial",
            "shaders/staticmesh_colorb.azmaterial"
        };
        m_materialBrowser.SetDefaultPinnedAssets(defaultMaterialAllowlist);

        m_pinnedMaterialCount = static_cast<uint32_t>(m_materialBrowser.GetPinnedAssets().size());

        const AZStd::vector<AZStd::string> defaultModelAllowist =
        {
            "Objects/bunny.azmodel",
            "Objects/Shaderball_simple.azmodel",
            "Objects/suzanne.azmodel",
        };
        m_modelBrowser.SetDefaultPinnedAssets(defaultModelAllowist);
    }

    void AssetLoadTestComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();

        m_imguiSidebar.Activate();
        m_materialBrowser.Activate();
        m_modelBrowser.Activate();

        if (!m_materialBrowser.IsConfigFileLoaded())
        {
            AZ_TracePrintf("AssetLoadTestComponent", "Material allow list not loaded. Defaulting to built in list.\n");

            m_materialBrowser.ResetPinnedAssetsToDefault();
        }

        if (!m_modelBrowser.IsConfigFileLoaded())
        {
            AZ_TracePrintf("AssetLoadTestComponent", "Model allow list not loaded. Defaulting to built in list.\n");

            m_modelBrowser.ResetPinnedAssetsToDefault();
        }

        Base::Activate();
    }

    void AssetLoadTestComponent::Deactivate()
    {
        m_materialBrowser.Deactivate();
        m_modelBrowser.Deactivate();

        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        Base::Deactivate();
    }
    
    void AssetLoadTestComponent::PrepareCreateLatticeInstances(uint32_t instanceCount)
    {
        m_modelInstanceData.reserve(instanceCount);
    }

    void AssetLoadTestComponent::CreateLatticeInstance(const AZ::Transform& transform)
    {
        m_modelInstanceData.emplace_back<ModelInstanceData>({});
        ModelInstanceData& data = m_modelInstanceData.back();
        data.m_modelAssetId = GetRandomModelId();
        data.m_materialAssetId = GetRandomMaterialId();
        data.m_transform = transform;
    }

    void AssetLoadTestComponent::FinalizeLatticeInstances()
    {
        AZStd::set<AZ::Data::AssetId> assetIds;

        for (ModelInstanceData& instanceData : m_modelInstanceData)
        {
            if (instanceData.m_materialAssetId.IsValid())
            {
                assetIds.insert(instanceData.m_materialAssetId);
            }

            if (instanceData.m_modelAssetId.IsValid())
            {
                assetIds.insert(instanceData.m_modelAssetId);
            }
        }

        AZStd::vector<AZ::AssetCollectionAsyncLoader::AssetToLoadInfo> assetList;

        for (auto& assetId : assetIds)
        {
            AZ::Data::AssetInfo assetInfo;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, assetId);
            if (assetInfo.m_assetId.IsValid())
            {
                AZ::AssetCollectionAsyncLoader::AssetToLoadInfo info;
                info.m_assetPath = assetInfo.m_relativePath;
                info.m_assetType = assetInfo.m_assetType;
                assetList.push_back(info);
            }
        }

        PreloadAssets(assetList);

        // pause script and tick until assets are ready
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);
        AZ::TickBus::Handler::BusDisconnect();
    }

    void AssetLoadTestComponent::OnAllAssetsReadyActivate()
    {
        AZ::Render::MaterialAssignmentMap materials;
        for (ModelInstanceData& instanceData : m_modelInstanceData)
        {
            AZ::Render::MaterialAssignment& defaultAssignment = materials[AZ::Render::DefaultMaterialAssignmentId];
            defaultAssignment = {};

            if (instanceData.m_materialAssetId.IsValid())
            {
                defaultAssignment.m_materialAsset.Create(instanceData.m_materialAssetId);
                defaultAssignment.m_materialInstance = AZ::RPI::Material::FindOrCreate(defaultAssignment.m_materialAsset);

                // cache the material when its loaded
                m_cachedMaterials.insert(defaultAssignment.m_materialAsset);
            }

            if (instanceData.m_modelAssetId.IsValid())
            {
                AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset;
                modelAsset.Create(instanceData.m_modelAssetId);

                instanceData.m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, materials);
                GetMeshFeatureProcessor()->SetTransform(instanceData.m_meshHandle, instanceData.m_transform);
            }
        }

        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
        AZ::TickBus::Handler::BusConnect();
    }

    void AssetLoadTestComponent::DestroyLatticeInstances()
    {
        DestroyHandles();
        m_modelInstanceData.clear();
    }

    void AssetLoadTestComponent::DestroyHandles()
    {
        for (ModelInstanceData& instanceData : m_modelInstanceData)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(instanceData.m_meshHandle);
            instanceData.m_meshHandle = {};
        }
    }

    AZ::Data::AssetId AssetLoadTestComponent::GetRandomModelId() const
    {
        auto& modelAllowlist = m_modelBrowser.GetPinnedAssets();

        if (modelAllowlist.size())
        {
            const size_t randomModelIndex = rand() % modelAllowlist.size();
            return modelAllowlist[randomModelIndex].m_assetId;
        }
        else
        {
            return AZ::RPI::AssetUtils::GetAssetIdForProductPath("testdata/objects/cube/cube.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
        }
    }

    AZ::Data::AssetId AssetLoadTestComponent::GetRandomMaterialId() const
    {
        auto& materialAllowlist = m_materialBrowser.GetPinnedAssets();

        if (materialAllowlist.size())
        {
            const size_t randomMaterialIndex = rand() % materialAllowlist.size();
            return materialAllowlist[randomMaterialIndex].m_assetId;
        }
        else
        {
            return AZ::RPI::AssetUtils::GetAssetIdForProductPath(DefaultPbrMaterialPath, AZ::RPI::AssetUtils::TraceLevel::Error);
        }
    }


    void AssetLoadTestComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint scriptTime)
    {
        AZ_TRACE_METHOD();

        const float timeSeconds = static_cast<float>(scriptTime.GetSeconds());

        if (m_lastMaterialSwitchInSeconds == 0 || m_lastModelSwitchInSeconds == 0)
        {
            m_lastMaterialSwitchInSeconds = timeSeconds;
            m_lastModelSwitchInSeconds = timeSeconds;
            return;
        }

        bool materialSwitchRequested = m_materialSwitchEnabled && timeSeconds - m_lastMaterialSwitchInSeconds >= m_materialSwitchTimeInSeconds;
        bool modelSwitchRequested = m_modelSwitchEnabled && timeSeconds - m_lastModelSwitchInSeconds >= m_modelSwitchTimeInSeconds;

        if (m_updateTransformEnabled)
        {
            float radians = static_cast<float>(fmod(scriptTime.GetSeconds(), AZ::Constants::TwoPi));
            AZ::Vector3 rotation(radians, radians, radians);
            AZ::Transform rotationTransform;
            rotationTransform.SetFromEulerRadians(rotation);

            for (ModelInstanceData& instanceData : m_modelInstanceData)
            {
                GetMeshFeatureProcessor()->SetTransform(instanceData.m_meshHandle, instanceData.m_transform * rotationTransform);
            }
        }

        bool materialsChanged = false;
        bool modelsChanged = false;

        if (m_imguiSidebar.Begin())
        {
            ImGui::Checkbox("Switch Materials Every N Seconds", &m_materialSwitchEnabled);
            ImGui::SliderFloat("##MaterialSwitchTime", &m_materialSwitchTimeInSeconds, 0.1f, 10.0f);

            ImGui::Spacing();

            ImGui::Checkbox("Switch Models Every N Seconds", &m_modelSwitchEnabled);
            ImGui::SliderFloat("##ModelSwitchTime", &m_modelSwitchTimeInSeconds, 0.1f, 10.0f);

            ImGui::Spacing();
            ImGui::Checkbox("Update Transforms Every Frame", &m_updateTransformEnabled);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            RenderImGuiLatticeControls();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGuiAssetBrowser::WidgetSettings assetBrowserSettings;
            assetBrowserSettings.m_labels.m_pinnedAssetList = "Allow List";
            assetBrowserSettings.m_labels.m_pinButton = "Add To Allow List";
            assetBrowserSettings.m_labels.m_unpinButton = "Remove From Allow List";

            assetBrowserSettings.m_labels.m_root = "Materials";
            m_materialBrowser.Tick(assetBrowserSettings);

            auto& pinnedMaterials = m_materialBrowser.GetPinnedAssets();
            materialsChanged = pinnedMaterials.size() != m_pinnedMaterialCount;

            if (materialsChanged)
            {
                m_pinnedMaterialCount = static_cast<uint32_t>(pinnedMaterials.size());
                // Keep the current m_cachedMaterials to avoid release-load the same material
                MaterialAssetSet newCache;
                // clean up cached material which refcount is 1
                for (auto& pinnedMaterial : pinnedMaterials)
                {
                    AZ::Data::AssetId materialAssetid = pinnedMaterial.m_assetId;
                    // Cache the asset if it's loaded
                    AZ::Data::Asset<AZ::RPI::MaterialAsset> asset = AZ::Data::AssetManager::Instance().FindAsset<AZ::RPI::MaterialAsset>(materialAssetid, AZ::Data::AssetLoadBehavior::PreLoad);
                    if (asset.IsReady())
                    {
                        newCache.insert(asset);
                    }
                }
                m_cachedMaterials = newCache;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            assetBrowserSettings.m_labels.m_root = "Models";
            m_modelBrowser.Tick(assetBrowserSettings);
            modelsChanged = m_lastPinnedModelCount != m_modelBrowser.GetPinnedAssets().size();

            m_imguiSidebar.End();
        }

        if (materialSwitchRequested || materialsChanged)
        {
            for (ModelInstanceData& instanceData : m_modelInstanceData)
            {
                instanceData.m_materialAssetId = GetRandomMaterialId();
            }
            m_lastMaterialSwitchInSeconds = timeSeconds;
        }

        if (modelSwitchRequested || modelsChanged)
        {
            m_lastPinnedModelCount = m_modelBrowser.GetPinnedAssets().size();
            for (ModelInstanceData& instanceData : m_modelInstanceData)
            {
                instanceData.m_modelAssetId = GetRandomModelId();
            }
            m_lastModelSwitchInSeconds = timeSeconds;
        }

        if (materialSwitchRequested || materialsChanged || modelSwitchRequested || modelsChanged)
        {
            DestroyHandles();
            FinalizeLatticeInstances();
        }
    }

} // namespace AtomSampleViewer
