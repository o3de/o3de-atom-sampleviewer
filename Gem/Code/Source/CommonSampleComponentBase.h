/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/Utils/LightingPreset.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/Feature/ImageBasedLights/ImageBasedLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Utils/AssetCollectionAsyncLoader.h>

#include <Utils/ImGuiProgressList.h>

namespace AtomSampleViewer
{
    class CommonSampleComponentBase
        : public AZ::Component
        , public AZ::TransformNotificationBus::MultiHandler
        , public AZ::EntityBus::MultiHandler
    {
    public:
        AZ_TYPE_INFO(MaterialHotReloadTestComponent, "{7EECDF09-B774-46C1-AD6E-060CE5717C05}");

        // AZ::Component overrides...
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;

    protected:
        //! Init and shut down should be called in derived components' Activate() and Deactivate().
        //! @param loadDefaultLightingPresets if true, it will scan all lighting presets in the project and load them.
        void InitLightingPresets(bool loadDefaultLightingPresets = false);
        void ShutdownLightingPresets();

        //! Add a drop down list to select lighting preset for this sample.
        //! Lighting presets must be loaded before calling this function, otherwise the list will be hide.
        //! It should be called between ImGui::Begin() and ImGui::End().
        //! e.g. Calling it between ImGuiSidebar::Begin() and ImGuiSidebar::End() will embed this list into the side bar.
        void ImGuiLightingPreset();

        //! Load lighting presets from an asset.
        //! It will clear any presets loaded previously.
        void LoadLightingPresetsFromAsset(const AZStd::string& assetPath);

        //! Load lighting presets from an asset.
        //! Append the presets to the current existing presets.
        void AppendLightingPresetsFromAsset(const AZStd::string& assetPath);

        //! Clear all lighting presets.
        void ClearLightingPresets();

        //! Reset internal scene related data
        void ResetScene();

        //! Apply lighting presets to the scene.
        //! Derived samples can override this function to have custom behaviors.
        virtual void OnLightingPresetSelected(const AZ::Render::LightingPreset& preset, bool useAltSkybox);

        //! Return the AtomSampleViewer EntityContextId, retrieved from the ComponentConfig
        AzFramework::EntityContextId GetEntityContextId() const;

        //! Return the AtomSampleViewer camera EntityId, retrieved from the ComponentConfig
        AZ::EntityId GetCameraEntityId() const;

        AZ::Render::MeshFeatureProcessorInterface* GetMeshFeatureProcessor() const;

        void OnLightingPresetEntityShutdown(const AZ::EntityId& entityId);

        // Preload assets 
        void PreloadAssets(const AZStd::vector<AZ::AssetCollectionAsyncLoader::AssetToLoadInfo>& assetList);

        //! Async asset load
        AZ::AssetCollectionAsyncLoader m_assetLoadManager;

        //! Showing the loading progress of preload assets
        ImGuiProgressList m_imguiProgressList;

        // The callback might be called more than one time if there are more than one asset are ready in one frame.
        // Use this flag to prevent OnAllAssetsReadyActivate be called more than one time.
        bool m_isAllAssetsReady = false;

        AZStd::string m_sampleName;

    private:
        // AZ::TransformNotificationBus::MultiHandler overrides...
        void OnTransformChanged(const AZ::Transform&, const AZ::Transform&) override;

        // virtual call back function which is called when all preloaded assets are loaded.
        virtual void OnAllAssetsReadyActivate() {};

        AzFramework::EntityContextId m_entityContextId;
        AZ::EntityId m_cameraEntityId;
        mutable AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;

        //! All loaded lighting presets.
        AZStd::vector<AZ::Render::LightingPreset> m_lightingPresets;

        //! Lights created by lighting presets.
        AZStd::vector<AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle> m_lightHandles;

        //! Post process entity to handle ExposureControlSettings.
        AZ::Entity* m_postProcessEntity = nullptr;

        //! Dirty flag is set to true when m_lightingPresets is modified.
        bool m_lightingPresetsDirty = true;

        //! Current active lighting preset.
        constexpr static int32_t InvalidLightingPresetIndex = -1;
        int32_t m_currentLightingPresetIndex = InvalidLightingPresetIndex;
        bool m_useAlternateSkybox = false; //!< LightingPresets have an alternate skybox that can be used, when this is true. This is usually a blurred version of the primary skybox.
    };

} // namespace AtomSampleViewer
