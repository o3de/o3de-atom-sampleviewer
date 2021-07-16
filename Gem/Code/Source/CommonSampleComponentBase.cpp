/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <CommonSampleComponentBase.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <RHI/BasicRHIComponent.h>
#include <EntityUtilityFunctions.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    bool CommonSampleComponentBase::ReadInConfig(const ComponentConfig* baseConfig)
    {
        auto config = azrtti_cast<const SampleComponentConfig*>(baseConfig);
        if (config && config->IsValid())
        {
            m_cameraEntityId = config->m_cameraEntityId;
            m_entityContextId = config->m_entityContextId;
            return true;
        }
        else
        {
            AZ_Assert(false, "SampleComponentConfig required for sample component configuration.");
            return false;
        }
    }

    AzFramework::EntityContextId CommonSampleComponentBase::GetEntityContextId() const
    {
        return m_entityContextId;
    }

    AZ::EntityId CommonSampleComponentBase::GetCameraEntityId() const
    {
        return m_cameraEntityId;
    }

    AZ::Render::MeshFeatureProcessorInterface* CommonSampleComponentBase::GetMeshFeatureProcessor() const
    {
        if (!m_meshFeatureProcessor)
        {
            m_meshFeatureProcessor = Scene::GetFeatureProcessorForEntityContextId<Render::MeshFeatureProcessorInterface>(m_entityContextId);
            AZ_Assert(m_meshFeatureProcessor, "MeshFeatureProcessor not available.");
        }

        return m_meshFeatureProcessor;
    }

    void CommonSampleComponentBase::InitLightingPresets(bool loadDefaultLightingPresets)
    {
        AZ::Render::SkyBoxFeatureProcessorInterface* skyboxFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::SkyBoxFeatureProcessorInterface>(GetEntityContextId());
        AZ_Assert(skyboxFeatureProcessor, "Unable to find SkyBoxFeatureProcessorInterface.");
        skyboxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::Cubemap);
        skyboxFeatureProcessor->Enable(true);

        // We don't necessarily need an entity but PostProcessFeatureProcessorInterface needs an ID to retrieve ExposureControlSettingsInterface.
        AzFramework::EntityContextRequestBus::EventResult(m_postProcessEntity, m_entityContextId, &AzFramework::EntityContextRequestBus::Events::CreateEntity, "postProcessEntity");
        AZ_Assert(m_postProcessEntity != nullptr, "Failed to create post process entity.");
        m_postProcessEntity->Activate();

        if (loadDefaultLightingPresets)
        {
            AZStd::list<AZ::Data::AssetInfo> lightingAssetInfoList;
            AZ::Data::AssetCatalogRequests::AssetEnumerationCB enumerateCB = [&lightingAssetInfoList]([[maybe_unused]] const AZ::Data::AssetId id, const AZ::Data::AssetInfo& info)
            {
                if (AzFramework::StringFunc::EndsWith(info.m_relativePath.c_str(), ".lightingpreset.azasset"))
                {
                    lightingAssetInfoList.push_front(info);
                }
            };

            AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequestBus::Events::EnumerateAssets, nullptr, enumerateCB, nullptr);

            for (const auto& info : lightingAssetInfoList)
            {
                AppendLightingPresetsFromAsset(info.m_relativePath);
            }
        }

        AZ::TransformNotificationBus::MultiHandler::BusConnect(m_cameraEntityId);
        AZ::EntityBus::MultiHandler::BusConnect(m_postProcessEntity->GetId());
    }

    void CommonSampleComponentBase::ShutdownLightingPresets()
    {
        AZ::Render::SkyBoxFeatureProcessorInterface* skyboxFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::SkyBoxFeatureProcessorInterface>(m_entityContextId);
        AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::DirectionalLightFeatureProcessorInterface>(m_entityContextId);

        for (AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle& handle : m_lightHandles)
        {
            directionalLightFeatureProcessor->ReleaseLight(handle);
        }
        m_lightHandles.clear();

        ClearLightingPresets();

        if (m_postProcessEntity)
        {
            DestroyEntity(m_postProcessEntity, GetEntityContextId());
        }

        skyboxFeatureProcessor->Enable(false);

        AZ::TransformNotificationBus::MultiHandler::BusDisconnect();
        AZ::EntityBus::MultiHandler::BusDisconnect();
    }

    void CommonSampleComponentBase::LoadLightingPresetsFromAsset(const AZStd::string& assetPath)
    {
        ClearLightingPresets();
        AppendLightingPresetsFromAsset(assetPath);
    }

    void CommonSampleComponentBase::AppendLightingPresetsFromAsset(const AZStd::string& assetPath)
    {
        Data::Asset<AnyAsset> asset = AssetUtils::LoadAssetByProductPath<AnyAsset>(assetPath.c_str(), AssetUtils::TraceLevel::Error);
        if (asset)
        {
            const AZ::Render::LightingPreset* preset = asset->GetDataAs<AZ::Render::LightingPreset>();
            if (preset)
            {
                m_lightingPresets.push_back(*preset);
            }
            m_lightingPresetsDirty = true;
            if (!m_lightingPresets.empty() && m_currentLightingPresetIndex == InvalidLightingPresetIndex)
            {
                m_currentLightingPresetIndex = 0;
                OnLightingPresetSelected(m_lightingPresets[0], m_useAlternateSkybox);
            }
        }
        else
        {
            AZ_Error("Lighting Preset", false, "Lighting preset file failed to load from path: %s.", assetPath.c_str());
        }
    }

    void CommonSampleComponentBase::ClearLightingPresets()
    {
        m_currentLightingPresetIndex = InvalidLightingPresetIndex;
        m_useAlternateSkybox = false;
        m_lightingPresets.clear();
        m_lightingPresetsDirty = true;
    }

    void CommonSampleComponentBase::ImGuiLightingPreset()
    {
        AZ_Assert((m_currentLightingPresetIndex == InvalidLightingPresetIndex) == m_lightingPresets.empty(),
            "m_currentLightingPresetIndex should be invalid if and only if no lighting presets are loaded.");

        if (m_currentLightingPresetIndex == InvalidLightingPresetIndex)
        {
            AZ_Warning("Lighting Preset", false, "Lighting presets must be loaded before calling ImGui.");
            return;
        }
        
        AZStd::string selectedPresetLabel = m_lightingPresets[m_currentLightingPresetIndex].m_displayName;
        if (m_useAlternateSkybox)
        {
            selectedPresetLabel += " (Alt)";
        }

        // Note that before we were using ScriptableImGui::Combo but there were issues (at least on some systems) when the number of items
        // exceeded the max visible item count (which defaults to 8 in ImGui). In this case you'd expect to see a scroll bar in the combo box,
        // but the combo box just never popped up. This only happened in profile, not debug builds. It seems to be a bug in ImGui. So by using
        // BeginCombo with ImGuiComboFlags_HeightLargest, we just make sure the combo box is always big enough for all the items.
        if (ScriptableImGui::BeginCombo("Lighting Preset##SampleBase", selectedPresetLabel.c_str(), ImGuiComboFlags_HeightLargest))
        {
            for (uint32_t i = 0; i < m_lightingPresets.size(); ++i)
            {
                AZStd::string& name = m_lightingPresets[i].m_displayName;
                AZStd::string nameForAlternateSkybox = name + " (Alt)";

                // Each LightingPreset can have an alternate skybox (usually a blurred version of the primary skybox), so we expose both here as separate items in the UI.

                bool useThisPresetWithNormalSkybox = (i == m_currentLightingPresetIndex) && !m_useAlternateSkybox;
                bool useThisPresetWithAlternateSkybox = (i == m_currentLightingPresetIndex) && m_useAlternateSkybox;

                useThisPresetWithNormalSkybox = ScriptableImGui::Selectable(name.c_str(), useThisPresetWithNormalSkybox);
                useThisPresetWithAlternateSkybox = ScriptableImGui::Selectable(nameForAlternateSkybox.c_str(), useThisPresetWithAlternateSkybox);

                if (useThisPresetWithNormalSkybox || useThisPresetWithAlternateSkybox)
                {
                    m_currentLightingPresetIndex = i;
                    m_useAlternateSkybox = useThisPresetWithAlternateSkybox;
                    OnLightingPresetSelected(m_lightingPresets[i], m_useAlternateSkybox);
                }
            }
            ScriptableImGui::EndCombo();
        }
    }

    void CommonSampleComponentBase::OnLightingPresetSelected(const AZ::Render::LightingPreset& preset, bool useAlternateSkybox)
    {
        AZ::Render::SkyBoxFeatureProcessorInterface* skyboxFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::SkyBoxFeatureProcessorInterface>(m_entityContextId);
        AZ::Render::PostProcessFeatureProcessorInterface* postProcessFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::PostProcessFeatureProcessorInterface>(m_entityContextId);
        AZ::Render::ImageBasedLightFeatureProcessorInterface* iblFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::ImageBasedLightFeatureProcessorInterface>(m_entityContextId);
        AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::DirectionalLightFeatureProcessorInterface>(m_entityContextId);

        AZ::Render::ExposureControlSettingsInterface* exposureControlSettingInterface = postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_postProcessEntity->GetId())->GetOrCreateExposureControlSettingsInterface();

        Camera::Configuration cameraConfig;
        Camera::CameraRequestBus::EventResult(cameraConfig, m_cameraEntityId, &Camera::CameraRequestBus::Events::GetCameraConfiguration);

        preset.ApplyLightingPreset(
            iblFeatureProcessor,
            skyboxFeatureProcessor,
            exposureControlSettingInterface,
            directionalLightFeatureProcessor,
            cameraConfig,
            m_lightHandles,
            nullptr,
            AZ::RPI::MaterialPropertyIndex{},
            useAlternateSkybox);
    }

    void CommonSampleComponentBase::OnTransformChanged(const AZ::Transform&, const AZ::Transform&)
    {
        const AZ::EntityId* currentBusId = AZ::TransformNotificationBus::GetCurrentBusId();
        AZ::Render::DirectionalLightFeatureProcessorInterface* directionalLightFeatureProcessor = AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::DirectionalLightFeatureProcessorInterface>(m_entityContextId);
        if (currentBusId && *currentBusId == m_cameraEntityId && directionalLightFeatureProcessor)
        {
            auto transform = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(
                transform,
                m_cameraEntityId,
                &AZ::TransformBus::Events::GetWorldTM);
            for (const AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle& handle : m_lightHandles)
            {
                directionalLightFeatureProcessor->SetCameraTransform(handle, transform);
            }
        }
    }

    void CommonSampleComponentBase::OnLightingPresetEntityShutdown(const AZ::EntityId& entityId)
    {
        if (m_postProcessEntity && m_postProcessEntity->GetId() == entityId)
        {
            m_postProcessEntity = nullptr;
        }
    }

    void CommonSampleComponentBase::PreloadAssets(const AZStd::vector<AssetCollectionAsyncLoader::AssetToLoadInfo>& assetList)
    {
        m_isAllAssetsReady = false;

        // Configure the imgui progress list widget.
        auto onUserCancelledAction = [&]()
        {
            AZ_TracePrintf(m_sampleName.c_str() , "Cancelled by user.\n");
            m_assetLoadManager.Cancel();
            SampleComponentManagerRequestBus::Broadcast(&SampleComponentManagerRequests::Reset);
        };
        m_imguiProgressList.OpenPopup("Waiting For Assets...", "Assets pending for processing:", {}, onUserCancelledAction, true /*automaticallyCloseOnAction*/, "Cancel");

        AZStd::for_each(assetList.begin(), assetList.end(),
            [&](const AssetCollectionAsyncLoader::AssetToLoadInfo& item) { m_imguiProgressList.AddItem(item.m_assetPath); });

        m_assetLoadManager.LoadAssetsAsync(assetList, [&](AZStd::string_view assetName, [[maybe_unused]] bool success, size_t pendingAssetCount)
            {
                AZ_Error(m_sampleName.c_str(), success, "Error loading asset %s, a crash will occur when OnAllAssetsReadyActivate() is called!", assetName.data());
                AZ_TracePrintf(m_sampleName.c_str(), "Asset %s loaded %s. Wait for %zu more assets before full activation\n", assetName.data(), success ? "successfully" : "UNSUCCESSFULLY", pendingAssetCount);
                m_imguiProgressList.RemoveItem(assetName);
                if (!pendingAssetCount && !m_isAllAssetsReady)
                {
                    m_isAllAssetsReady = true;
                    OnAllAssetsReadyActivate();
                }
            });
    }

    void CommonSampleComponentBase::ResetScene()
    {
        m_meshFeatureProcessor = nullptr;
    }

} // namespace AtomSampleViewer
