/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/HighInstanceExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/Component/DebugCamera/NoClipControllerBus.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>

#include <Automation/ScriptRunnerBus.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Windowing/WindowBus.h>

#include <RHI/BasicRHIComponent.h>

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


    HighInstanceTestComponent::HighInstanceTestComponent() 
        : m_materialBrowser("@user@/HighInstanceTestComponent/material_browser.xml")
        , m_modelBrowser("@user@/HighInstanceTestComponent/model_browser.xml")
        , m_imguiSidebar("@user@/HighInstanceTestComponent/sidebar.xml")
    {
        m_materialBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::MaterialAsset>();
        });

        m_modelBrowser.SetFilter([](const AZ::Data::AssetInfo& assetInfo)
        {
            return assetInfo.m_assetType == azrtti_typeid<AZ::RPI::ModelAsset>();
        });

        // Only use a diffuse white material so light colors are easily visible.
        const AZStd::vector<AZStd::string> materialAllowlist =
        {
            "materials/presets/macbeth/19_white_9-5_0-05d.azmaterial",
        };
        m_materialBrowser.SetDefaultPinnedAssets(materialAllowlist);

        m_pinnedMaterialCount = static_cast<uint32_t>(m_materialBrowser.GetPinnedAssets().size());

        m_expandedModelList =
        {
            "materialeditor/viewportmodels/cone.fbx.azmodel",
            "materialeditor/viewportmodels/cube.fbx.azmodel",
            "materialeditor/viewportmodels/cylinder.fbx.azmodel",
            "materialeditor/viewportmodels/platonicsphere.fbx.azmodel",
            "materialeditor/viewportmodels/polarsphere.fbx.azmodel",
            "materialeditor/viewportmodels/quadsphere.fbx.azmodel",
            "materialeditor/viewportmodels/torus.fbx.azmodel",
            "objects/cube.fbx.azmodel",
            "objects/cylinder.fbx.azmodel",
        };
        m_simpleModelList =
        {
            "objects/cube.fbx.azmodel"
        };
        m_modelBrowser.SetDefaultPinnedAssets(m_simpleModelList);
    }

    void HighInstanceTestComponent::Activate()
    {
        BuildDiskLightParameters();

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
            m_testParameters.m_latticeSize[0], 
            m_testParameters.m_latticeSize[1], 
            m_testParameters.m_latticeSize[2]);
        SetLatticeSpacing(
            m_testParameters.m_latticeSpacing[0],
            m_testParameters.m_latticeSpacing[1],
            m_testParameters.m_latticeSpacing[2]);
        SetLatticeEntityScale(m_testParameters.m_entityScale);

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

            SetIBLExposure(m_testParameters.m_iblExposure);
    }

    void HighInstanceTestComponent::Deactivate()
    {
        DestroyLights();
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
        DestroyLights();
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

        if (m_testParameters.m_numShadowCastingSpotLights > 0 && m_diskLightsEnabled)
        {
            CreateSpotLights();
        }

        if (m_testParameters.m_activateDirectionalLight && m_directionalLightEnabled)
        {
            CreateDirectionalLight();
        }
    }

    void HighInstanceTestComponent::OnAllAssetsReadyActivate()
    {
        for (ModelInstanceData& instanceData : m_modelInstanceData)
        {
            AZ::Data::Instance<AZ::RPI::Material> materialInstance;
            if (instanceData.m_materialAssetId.IsValid())
            {
                AZ::Data::Asset<RPI::MaterialAsset> materialAsset;
                materialAsset.Create(instanceData.m_materialAssetId);
                materialInstance = AZ::RPI::Material::FindOrCreate(materialAsset);

                // cache the material when its loaded
                m_cachedMaterials.insert(materialAsset);
            }

            if (instanceData.m_modelAssetId.IsValid())
            {
                AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset;
                modelAsset.Create(instanceData.m_modelAssetId);

                instanceData.m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ modelAsset }, materialInstance);
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

    void HighInstanceTestComponent::DestroyLights()
    {
        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        m_directionalLightHandle = {};
        for (int index = 0; index < m_diskLights.size(); ++index)
        {
            DiskLightHandle& handle = m_diskLights[index].m_handle;
            m_diskLightFeatureProcessor->ReleaseLight(handle);
            handle = {};
        }
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
            return AZ::RPI::AssetUtils::GetAssetIdForProductPath("testdata/objects/cube/cube.fbx.azmodel", AZ::RPI::AssetUtils::TraceLevel::Error);
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
        bool diskLightsEnabled = m_diskLightsEnabled;
        bool directionalLightEnabled = m_directionalLightEnabled;
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

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Display SpotLight Debug", &m_drawDiskLightDebug);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (m_testParameters.m_numShadowCastingSpotLights > 0)
            {
                ImGui::Checkbox("Enable SpotLights", &m_diskLightsEnabled);
            }
            if (m_testParameters.m_activateDirectionalLight)
            {
                ImGui::Checkbox("Enable Directional Light", &m_directionalLightEnabled);
            }

            m_imguiSidebar.End();
        }

        if(diskLightsEnabled != m_diskLightsEnabled || directionalLightEnabled != m_directionalLightEnabled)
        {
            DestroyLights();
            if (m_testParameters.m_numShadowCastingSpotLights > 0 && m_diskLightsEnabled)
            {
                CreateSpotLights();
            }
            if(m_testParameters.m_activateDirectionalLight && m_directionalLightEnabled)
            {
                CreateDirectionalLight();
            }
        }

        if (currentUseSimpleModels != m_useSimpleModels)
        {
            m_modelBrowser.SetPinnedAssets(m_useSimpleModels?m_simpleModelList:m_expandedModelList);
            for (ModelInstanceData& instanceData : m_modelInstanceData)
            {
                instanceData.m_modelAssetId = GetRandomModelId();
            }
            DestroyHandles();
            DestroyLights();
            FinalizeLatticeInstances();
        }

        DrawDiskLightDebugObjects();
    }

    void HighInstanceTestComponent::ResetNoClipController()
    {
        using namespace AZ;
        using namespace AZ::Debug;
        AZ::Vector3 camPos = AZ::Vector3::CreateFromFloat3(m_testParameters.m_cameraPosition);
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, 2000.0f);
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetPosition, camPos);
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetHeading, AZ::DegToRad(m_testParameters.m_cameraHeadingDeg));
        NoClipControllerRequestBus::Event(GetCameraEntityId(), &NoClipControllerRequestBus::Events::SetPitch, AZ::DegToRad(m_testParameters.m_cameraPitchDeg));
    }

    void HighInstanceTestComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(m_originalFarClipDistance, GetCameraEntityId(), &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void HighInstanceTestComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(GetCameraEntityId(), &Camera::CameraRequestBus::Events::SetFarClipDistance, m_originalFarClipDistance);
    }

    void HighInstanceTestComponent::CreateSpotLights()
    {
        for (int index = 0; index < m_testParameters.m_numShadowCastingSpotLights; ++index)
        {
            CreateSpotLight(index);
        }
   }

    void HighInstanceTestComponent::CreateSpotLight(int index)
    {
        Render::DiskLightFeatureProcessorInterface* const featureProcessor = m_diskLightFeatureProcessor;
        const DiskLightHandle handle = featureProcessor->AcquireLight();

        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(m_diskLights[index].m_color * m_testParameters.m_shadowSpotlightIntensity);
        featureProcessor->SetRgbIntensity(handle, lightColor);
        featureProcessor->SetAttenuationRadius(handle, m_testParameters.m_shadowSpotlightMaxDistance);
        featureProcessor->SetConeAngles(
            handle, 
            DegToRad(m_testParameters.m_shadowSpotlightInnerAngleDeg), 
            DegToRad(m_testParameters.m_shadowSpotlightOuterAngleDeg));
        featureProcessor->SetShadowsEnabled(handle, true);
        featureProcessor->SetShadowmapMaxResolution(handle, m_testParameters.m_shadowmapSize);
        featureProcessor->SetShadowFilterMethod(handle, m_testParameters.m_shadowFilterMethod);
        featureProcessor->SetFilteringSampleCount(handle, 1);

        const Vector3 aabbOffset = m_diskLights[index].m_relativePosition.GetNormalized() * (0.5f * m_testParameters.m_shadowSpotlightMaxDistance);
        const Vector3 position = m_worldAabb.GetCenter() + aabbOffset;

        featureProcessor->SetPosition(handle, position);
        featureProcessor->SetDirection(handle, (-aabbOffset).GetNormalized());

        m_diskLights[index].m_handle = handle;
    }

    const AZ::Color& HighInstanceTestComponent::GetNextLightColor()
    {
        static uint16_t colorIndex = 0;
        static const AZStd::vector<AZ::Color> colors =
        {
            AZ::Colors::Red,
            AZ::Colors::Green,
            AZ::Colors::Blue,
            AZ::Colors::Cyan,
            AZ::Colors::Fuchsia,
            AZ::Colors::Yellow,
            AZ::Colors::SpringGreen
        };

        const AZ::Color& result = colors[colorIndex];
        colorIndex = (colorIndex + 1) % colors.size();
        return result;
    }

    AZ::Vector3 HighInstanceTestComponent::GetRandomDirection()
    {
        return AZ::Vector3(
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f).GetNormalized();
    }

    void HighInstanceTestComponent::BuildDiskLightParameters()
    {
        m_random.SetSeed(0);
        m_diskLights.clear();
        m_diskLights.reserve(m_testParameters.m_numShadowCastingSpotLights);
        for (int index = 0; index < m_testParameters.m_numShadowCastingSpotLights; ++index)
        {
            m_diskLights.emplace_back(
                GetNextLightColor(),
                GetRandomDirection(),
                m_testParameters.m_shadowmapSize);
        }
    }

    void HighInstanceTestComponent::CreateDirectionalLight()
    {
        Render::DirectionalLightFeatureProcessorInterface* featureProcessor = m_directionalLightFeatureProcessor;
        const DirectionalLightHandle handle = featureProcessor->AcquireLight();

        const auto lightTransform = Transform::CreateLookAt(
            -m_worldAabb.GetMax(),
            Vector3::CreateZero());
        featureProcessor->SetDirection(
            handle,
            lightTransform.GetBasis(1));

        featureProcessor->SetRgbIntensity(handle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(AZ::Color::CreateOne() * m_testParameters.m_directionalLightIntensity));
        featureProcessor->SetCascadeCount(handle, m_testParameters.m_numDirectionalLightShadowCascades);
        featureProcessor->SetShadowmapSize(handle, m_testParameters.m_shadowmapSize);
        featureProcessor->SetViewFrustumCorrectionEnabled(handle, false);
        featureProcessor->SetShadowFilterMethod(handle, m_testParameters.m_shadowFilterMethod);
        featureProcessor->SetFilteringSampleCount(handle, 1);
        featureProcessor->SetGroundHeight(handle, 0.f);

        m_directionalLightHandle = handle;
    }



    void HighInstanceTestComponent::DrawDiskLightDebugObjects()
    {
        if (auto auxGeom = AZ::RPI::AuxGeomFeatureProcessorInterface::GetDrawQueueForScene(m_scene); 
            m_drawDiskLightDebug && auxGeom)
        {
            for (int i = 0; i < m_diskLights.size(); ++i)
            {
                const auto& light = m_diskLights[i];
                if (light.m_handle.IsValid())
                {
                    const Render::DiskLightData& lightData = m_diskLightFeatureProcessor->GetDiskData(light.m_handle);
                    const float lightDistance = sqrt(1.0f/lightData.m_invAttenuationRadiusSquared);
                    AZ::Vector3 position = AZ::Vector3::CreateFromFloat3(lightData.m_position.data());
                    AZ::Vector3 direction = AZ::Vector3::CreateFromFloat3(lightData.m_direction.data()).GetNormalized();
                    position += direction * lightDistance;
                    direction *= -1.0f;
                    float coneSlantLength = lightDistance / lightData.m_cosOuterConeAngle;
                    float farEndCapRadius = sqrt(coneSlantLength * coneSlantLength - lightDistance * lightDistance);
                    auxGeom->DrawCone(position, direction, farEndCapRadius, lightDistance, light.m_color, AZ::RPI::AuxGeomDraw::DrawStyle::Line);
                }
            }
        }
    }

} // namespace AtomSampleViewer
