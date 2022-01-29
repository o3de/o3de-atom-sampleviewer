/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <HighInstanceExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/Component/DebugCamera/NoClipControllerBus.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Windowing/WindowBus.h>

#include <RHI/BasicRHIComponent.h>

#include <HighInstanceTestComponent_Traits_Platform.h>

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer
{
    using namespace AZ;

    void HighInstanceTestComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HighInstanceTestComponent, EntityLatticeTestComponent>()
                ->Version(0)
                ;
        }
    }


    HighInstanceTestComponent::HighInstanceTestComponent(const HighInstanceTestParameters& params) 
        : m_materialBrowser("@user@/HighInstanceTestComponent/material_browser.xml")
        , m_modelBrowser("@user@/HighInstanceTestComponent/model_browser.xml")
        , m_imguiSidebar("@user@/HighInstanceTestComponent/sidebar.xml")
    {
        m_sampleName = params.m_sampleName;
        for (int i = 0; i < 3; ++i)
        {
            m_latticeSize[i] = params.m_latticeSize[i];
            m_latticeSpacing[i] = params.m_latticeSpacing[i];
        }

        m_materialBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::MaterialAsset>();
        });

        m_modelBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
        });

        const AZStd::vector<AZStd::string> materialAllowlist =
        {
            "materials/presets/pbr/metal_aluminum.azmaterial",
            "materials/presets/pbr/metal_aluminum_matte.azmaterial",
            "materials/presets/pbr/metal_aluminum_polished.azmaterial",
            "materials/presets/pbr/metal_brass.azmaterial",
            "materials/presets/pbr/metal_brass_matte.azmaterial",
            "materials/presets/pbr/metal_brass_polished.azmaterial",
            "materials/presets/pbr/metal_chrome.azmaterial",
            "materials/presets/pbr/metal_chrome_matte.azmaterial",
            "materials/presets/pbr/metal_chrome_polished.azmaterial",
            "materials/presets/pbr/metal_cobalt.azmaterial",
            "materials/presets/pbr/metal_cobalt_matte.azmaterial",
            "materials/presets/pbr/metal_cobalt_polished.azmaterial",
            "materials/presets/pbr/metal_copper.azmaterial",
            "materials/presets/pbr/metal_copper_matte.azmaterial",
            "materials/presets/pbr/metal_copper_polished.azmaterial",
            "materials/presets/pbr/metal_gold.azmaterial",
            "materials/presets/pbr/metal_gold_matte.azmaterial",
            "materials/presets/pbr/metal_gold_polished.azmaterial",
            "materials/presets/pbr/metal_iron.azmaterial",
            "materials/presets/pbr/metal_iron_matte.azmaterial",
            "materials/presets/pbr/metal_iron_polished.azmaterial",
            "materials/presets/pbr/metal_mercury.azmaterial",
            "materials/presets/pbr/metal_nickel.azmaterial",
            "materials/presets/pbr/metal_nickel_matte.azmaterial",
            "materials/presets/pbr/metal_nickel_polished.azmaterial",
            "materials/presets/pbr/metal_palladium.azmaterial",
            "materials/presets/pbr/metal_palladium_matte.azmaterial",
            "materials/presets/pbr/metal_palladium_polished.azmaterial",
            "materials/presets/pbr/metal_platinum.azmaterial",
            "materials/presets/pbr/metal_platinum_matte.azmaterial",
            "materials/presets/pbr/metal_platinum_polished.azmaterial",
            "materials/presets/pbr/metal_silver.azmaterial",
            "materials/presets/pbr/metal_silver_matte.azmaterial",
            "materials/presets/pbr/metal_silver_polished.azmaterial",
            "materials/presets/pbr/metal_titanium.azmaterial",
            "materials/presets/pbr/metal_titanium_matte.azmaterial",
            "materials/presets/pbr/metal_titanium_polished.azmaterial",
        };
        m_materialBrowser.SetDefaultPinnedAssets(materialAllowlist);

        m_pinnedMaterialCount = static_cast<uint32_t>(m_materialBrowser.GetPinnedAssets().size());

        m_expandedModelList =
        {
            "materialeditor/viewportmodels/cone.azmodel",
            "materialeditor/viewportmodels/cube.azmodel",
            "materialeditor/viewportmodels/cylinder.azmodel",
            "materialeditor/viewportmodels/platonicsphere.azmodel",
            "materialeditor/viewportmodels/polarsphere.azmodel",
            "materialeditor/viewportmodels/quadsphere.azmodel",
            "materialeditor/viewportmodels/torus.azmodel",
            "objects/cube.azmodel",
            "objects/cylinder.azmodel",
        };
        m_simpleModelList =
        {
            "objects/cube.azmodel"
        };
        m_modelBrowser.SetDefaultPinnedAssets(m_simpleModelList);
    }

    void HighInstanceTestComponent::Activate()
    {

        m_directionalLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();

        AZ::TickBus::Handler::BusConnect();

        m_imguiSidebar.Activate();
        m_imguiSidebar.SetHideSidebar(true);
        m_materialBrowser.Activate();
        m_modelBrowser.Activate();

        m_materialBrowser.ResetPinnedAssetsToDefault();

        m_modelBrowser.ResetPinnedAssetsToDefault();

        SetLatticeDimensions(
            m_latticeSize[0], 
            m_latticeSize[1], 
            m_latticeSize[2]);
        SetLatticeSpacing(
            m_latticeSpacing[0],
            m_latticeSpacing[1],
            m_latticeSpacing[2]);
        Base::Activate();

        AzFramework::NativeWindowHandle windowHandle = nullptr;
        AzFramework::WindowSystemRequestBus::BroadcastResult(
            windowHandle,
            &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);

        AzFramework::WindowRequestBus::EventResult(
            m_preActivateVSyncInterval, 
            windowHandle, 
            &AzFramework::WindowRequestBus::Events::GetSyncInterval);

        AzFramework::WindowRequestBus::Event(
            windowHandle, 
            &AzFramework::WindowRequestBus::Events::SetSyncInterval,
            0);

            SaveCameraConfiguration();
            ResetNoClipController();
    }

    void HighInstanceTestComponent::Deactivate()
    {
        RestoreCameraConfiguration();
        AzFramework::NativeWindowHandle windowHandle = nullptr;
        AzFramework::WindowSystemRequestBus::BroadcastResult(
            windowHandle,
            &AzFramework::WindowSystemRequestBus::Events::GetDefaultWindowHandle);

        AzFramework::WindowRequestBus::Event(
            windowHandle, 
            &AzFramework::WindowRequestBus::Events::SetSyncInterval,
            m_preActivateVSyncInterval);

        m_materialBrowser.Deactivate();
        m_modelBrowser.Deactivate();

        AZ::TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        Base::Deactivate();
    }
    
    void HighInstanceTestComponent::PrepareCreateLatticeInstances(uint32_t instanceCount)
    {
        m_modelInstanceData.reserve(instanceCount);
    }

    void HighInstanceTestComponent::CreateLatticeInstance(const AZ::Transform& transform)
    {
        m_modelInstanceData.emplace_back<ModelInstanceData>({});
        ModelInstanceData& data = m_modelInstanceData.back();
        data.m_modelAssetId = GetRandomModelId();
        data.m_materialAssetId = GetRandomMaterialId();
        data.m_transform = transform;
    }

    void HighInstanceTestComponent::FinalizeLatticeInstances()
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

    void HighInstanceTestComponent::OnAllAssetsReadyActivate()
    {
        m_directionalLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_diskLightFeatureProcessor = m_scene->GetFeatureProcessor<Render::DiskLightFeatureProcessorInterface>();

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

    void HighInstanceTestComponent::DestroyLatticeInstances()
    {
        DestroyHandles();
        m_modelInstanceData.clear();
    }

    void HighInstanceTestComponent::DestroyHandles()
    {
        for (ModelInstanceData& instanceData : m_modelInstanceData)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(instanceData.m_meshHandle);
            instanceData.m_meshHandle = {};
        }
    }

    AZ::Data::AssetId HighInstanceTestComponent::GetRandomModelId() const
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

    AZ::Data::AssetId HighInstanceTestComponent::GetRandomMaterialId() const
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


    void HighInstanceTestComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint scriptTime)
    {
		AZ_PROFILE_FUNCTION(AtomSampleViewer);

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

        bool currentUseSimpleModels = m_useSimpleModels;
        if (m_imguiSidebar.Begin())
        {
            ImGui::Checkbox("Update Transforms Every Frame", &m_updateTransformEnabled);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            RenderImGuiLatticeControls();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Use simple models", &m_useSimpleModels);

            m_imguiSidebar.End();
        }

        if (currentUseSimpleModels != m_useSimpleModels)
        {
            m_modelBrowser.SetPinnedAssets(m_useSimpleModels?m_simpleModelList:m_expandedModelList);
            for (ModelInstanceData& instanceData : m_modelInstanceData)
            {
                instanceData.m_modelAssetId = GetRandomModelId();
            }
            DestroyHandles();
            FinalizeLatticeInstances();
        }
    }

    void HighInstanceTestComponent::ResetNoClipController()
    {
        using namespace AZ;
        using namespace AZ::Debug;
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 2000.0f);
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(-44.722542));
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetPitch, AZ::DegToRad(24.987326));
    }

    void HighInstanceTestComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void HighInstanceTestComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
    }

    void HighInstanceTestComponent::CreateSpotLight(int index)
    {
        auto& light = m_diskLights[index];
        light.m_lightHandle = m_diskLightFeatureProcessor->AcquireLight();

        const LightSettings& settings = m_settings[(int)LightType::Disk];

        m_diskLightFeatureProcessor->SetDiskRadius(light.m_lightHandle, m_diskRadius);
        m_diskLightFeatureProcessor->SetPosition(light.m_lightHandle, light.m_position);
        m_diskLightFeatureProcessor->SetRgbIntensity(light.m_lightHandle, PhotometricColor<PhotometricUnit::Candela>(settings.m_intensity * light.m_color));
        m_diskLightFeatureProcessor->SetDirection(light.m_lightHandle, light.m_direction);
        
        m_diskLightFeatureProcessor->SetConstrainToConeLight(light.m_lightHandle, m_diskConesEnabled);
        if (m_diskConesEnabled)
        {
            m_diskLightFeatureProcessor->SetConeAngles(light.m_lightHandle, DegToRad(m_diskInnerConeDegrees), DegToRad(m_diskOuterConeDegrees));
        }

        m_diskLightFeatureProcessor->SetAttenuationRadius(light.m_lightHandle, m_settings[(int)LightType::Disk].m_attenuationRadius);
    }

} // namespace AtomSampleViewer
