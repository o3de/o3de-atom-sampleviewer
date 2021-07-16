/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SceneReloadSoakTestComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>

#include <Atom/Component/DebugCamera/NoClipControllerBus.h>

#include <AzCore/Component/Entity.h>

#include <RHI/BasicRHIComponent.h>

#include <SceneReloadSoakTestComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    void SceneReloadSoakTestComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<SceneReloadSoakTestComponent, EntityLatticeTestComponent>()
                ->Version(0)
                ;
        }
    }

    void SceneReloadSoakTestComponent::Activate()
    {
        m_timeSettings.clear();
        m_timeSettings.push_back(TimeSetting{2.0f, 1});
        m_timeSettings.push_back(TimeSetting{0.2f, 50});
        m_timeSettings.push_back(TimeSetting{0.5f, 10});
        m_timeSettings.push_back(TimeSetting{1.0f, 5});

        m_countdown = 0;
        m_totalTime = 0;
        m_currentSettingIndex = 0;
        m_currentCount = 0;
        m_totalResetCount = 0;

        SetLatticeDimensions(ATOMSAMPLEVIEWER_TRAIT_SCENE_RELOAD_SOAK_TEST_COMPONENT_LATTICE_SIZE, ATOMSAMPLEVIEWER_TRAIT_SCENE_RELOAD_SOAK_TEST_COMPONENT_LATTICE_SIZE, ATOMSAMPLEVIEWER_TRAIT_SCENE_RELOAD_SOAK_TEST_COMPONENT_LATTICE_SIZE);
        Base::Activate();

        TickBus::Handler::BusConnect();
        ExampleComponentRequestBus::Handler::BusConnect(GetEntityId());
    }

    void SceneReloadSoakTestComponent::Deactivate()
    {
        ExampleComponentRequestBus::Handler::BusDisconnect();
        TickBus::Handler::BusDisconnect();
        Base::Deactivate();
    }
    
    void SceneReloadSoakTestComponent::ResetCamera()
    {
        Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetHeading, DegToRad(-25));
        Debug::NoClipControllerRequestBus::Event(GetCameraEntityId(), &Debug::NoClipControllerRequestBus::Events::SetPitch, DegToRad(25));
    }

    void SceneReloadSoakTestComponent::PrepareCreateLatticeInstances(uint32_t instanceCount)
    {
        m_materialIsUnique.reserve(instanceCount);
        m_meshHandles.reserve(instanceCount);

        const char* materialPath = DefaultPbrMaterialPath;
        const char* modelPath = "objects/shaderball_simple.azmodel";

        Data::AssetCatalogRequestBus::BroadcastResult(
            m_materialAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            materialPath, azrtti_typeid<MaterialAsset>(), false);

        Data::AssetCatalogRequestBus::BroadcastResult(
            m_modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            modelPath, azrtti_typeid<ModelAsset>(), false);

        AZ_Assert(m_materialAssetId.IsValid(), "Failed to get material asset id: %s", materialPath);
        AZ_Assert(m_modelAssetId.IsValid(), "Failed to get model asset id: %s", modelPath);
    }

    void SceneReloadSoakTestComponent::CreateLatticeInstance(const Transform& transform)
    {
        Render::MaterialAssignmentMap materials;
        Render::MaterialAssignment& defaultMaterial = materials[Render::DefaultMaterialAssignmentId];
        defaultMaterial.m_materialAsset = Data::AssetManager::Instance().GetAsset<MaterialAsset>(m_materialAssetId, AZ::Data::AssetLoadBehavior::PreLoad);
        defaultMaterial.m_materialAsset.BlockUntilLoadComplete();

        // We have a mixture of both unique and shared instance to give more variety and therefore more opportunity for things to break.
        bool materialIsUnique = (m_materialIsUnique.size() % 2) == 0;
        defaultMaterial.m_materialInstance = materialIsUnique ?
            Material::Create(defaultMaterial.m_materialAsset) :
            Material::FindOrCreate(defaultMaterial.m_materialAsset);
        m_materialIsUnique.push_back(materialIsUnique);

        Data::Asset<ModelAsset> modelAsset;
        modelAsset.Create(m_modelAssetId);

        auto meshHandle = GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor{ modelAsset }, materials);
        GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);
        m_meshHandles.push_back(AZStd::move(meshHandle));
    }

    void SceneReloadSoakTestComponent::DestroyLatticeInstances()
    {
        m_materialIsUnique.clear();
        for (auto& meshHandle : m_meshHandles)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(meshHandle);
        }
        m_meshHandles.clear();
    }

    void SceneReloadSoakTestComponent::OnTick(float deltaTime, [[maybe_unused]] ScriptTimePoint scriptTime)
    {
        // There are some crashes that specifically occurred when unloading a scene while compiling material changes
        {
            // Create a new SimpleLcgRandom every time TickMaterialUpdate is called to keep a consistent seed and consistent color selection.
            SimpleLcgRandom random;
            
            bool updatedSharedMaterialInstance = false;
            size_t entityIndex = 0;

            MaterialPropertyIndex colorPropertyIndex;

            for (auto& meshHandle : m_meshHandles)
            {
                const Render::MaterialAssignmentMap& materials = GetMeshFeatureProcessor()->GetMaterialAssignmentMap(meshHandle);
                const auto defaultMaterialItr = materials.find(Render::DefaultMaterialAssignmentId);
                const auto& defaultMaterial = defaultMaterialItr != materials.end() ? defaultMaterialItr->second : Render::MaterialAssignment();
                Data::Instance<Material> material = defaultMaterial.m_materialInstance;
                if (material == nullptr)
                {
                    continue;
                }

                static const float speed = 4.0f;
                const float t = static_cast<float>(sin(m_totalTime * speed) * 0.5f + 0.5f);

                static const Color colorOptions[] =
                {
                    Color(1.0f, 0.0f, 0.0f, 1.0f),
                    Color(0.0f, 1.0f, 0.0f, 1.0f),
                    Color(0.0f, 0.0f, 1.0f, 1.0f),
                    Color(1.0f, 1.0f, 0.0f, 1.0f),
                    Color(0.0f, 1.0f, 1.0f, 1.0f),
                    Color(1.0f, 0.0f, 1.0f, 1.0f),
                };

                const int colorIndexA = random.GetRandom() % AZ_ARRAY_SIZE(colorOptions);
                int colorIndexB = colorIndexA;
                while (colorIndexA == colorIndexB)
                {
                    colorIndexB = random.GetRandom() % AZ_ARRAY_SIZE(colorOptions);
                }
                const Color color = colorOptions[colorIndexA] * t + colorOptions[colorIndexB] * (1.0f - t);

                if (m_materialIsUnique[entityIndex] || !updatedSharedMaterialInstance)
                {
                    if (colorPropertyIndex.IsNull())
                    {
                        colorPropertyIndex = material->FindPropertyIndex(Name("baseColor.color"));
                    }

                    if (colorPropertyIndex.IsValid())
                    {
                        material->SetPropertyValue(colorPropertyIndex, color);

                        if (!m_materialIsUnique[entityIndex])
                        {
                            updatedSharedMaterialInstance = true;
                        }
                    }
                    else
                    {
                        AZ_Error("", false, "Could not find the color property index");
                    }

                    material->Compile();
                }

                entityIndex++;
            }
        }
        
        m_countdown -= deltaTime;
        m_totalTime += deltaTime;

        // Rebuild the scene every time the countdown expires.
        // We might also need to move to the next TimeSetting with a new countdown time.
        if (m_countdown < 0)
        {
            TimeSetting currentSetting = m_timeSettings[m_currentSettingIndex % m_timeSettings.size()];

            m_countdown = currentSetting.resetDelay;

            // The TimeSetting struct tells us how many times we should use it, before moving to the next TimeSetting struct
            m_currentCount++;
            if (m_currentCount >= currentSetting.count)
            {
                m_currentCount = 0;
                m_currentSettingIndex++;
            }

            m_totalResetCount++;
            AZ_TracePrintf("", "SceneReloadSoakTest RESET # %d @ time %f. Next reset in %f s\n", m_totalResetCount, m_totalTime, m_countdown);
            RebuildLattice();
        }
    }

} // namespace AtomSampleViewer
