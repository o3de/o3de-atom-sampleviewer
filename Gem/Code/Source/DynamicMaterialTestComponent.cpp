/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <DynamicMaterialTestComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertiesLayout.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Debug/EventTrace.h>
#include <AzCore/Math/Random.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    void DynamicMaterialTestComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<DynamicMaterialTestComponent, EntityLatticeTestComponent>()
                ->Version(0)
                ;
        }
    }

    DynamicMaterialTestComponent::DynamicMaterialTestComponent()
        : m_imguiSidebar("@user@/DynamicMaterialTestComponent/sidebar.xml")
        , m_compileTimer(CompileTimerQueueSize, CompileTimerQueueSize)
    {

    }

    void DynamicMaterialTestComponent::Activate()
    {
        TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();
        InitMaterialConfigs();
        Base::Activate();

        m_currentTime = 0.0f;
    }

    void DynamicMaterialTestComponent::Deactivate()
    {
        TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        Base::Deactivate();
    }
    
    void DynamicMaterialTestComponent::PrepareCreateLatticeInstances(uint32_t instanceCount)
    {
        const char* modelPath = "objects/shaderball_simple.azmodel";

        Data::AssetId modelAssetId;
        Data::AssetCatalogRequestBus::BroadcastResult(
            modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            modelPath, azrtti_typeid<ModelAsset>(), false);

        AZ_Assert(modelAssetId.IsValid(), "Failed to get model asset id: %s", modelPath);

        m_modelAsset.Create(modelAssetId);

        m_meshHandles.reserve(instanceCount);
        m_materials.reserve(instanceCount);

        // Give the models time to load before continuing scripts
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScript);
        m_waitingForMeshes = true;
    }

    void DynamicMaterialTestComponent::CreateLatticeInstance(const Transform& transform)
    {
        AZ::Data::Asset<AZ::RPI::MaterialAsset>& materialAsset = m_materialConfigs[m_currentMaterialConfig].m_materialAsset;
        AZ::Data::Instance<AZ::RPI::Material> material = Material::Create(materialAsset);
         
        Render::MaterialAssignmentMap materialMap;
        Render::MaterialAssignment& defaultMaterial = materialMap[Render::DefaultMaterialAssignmentId];
        defaultMaterial.m_materialAsset = materialAsset;
        defaultMaterial.m_materialInstance = material;

        Render::MeshHandleDescriptor meshDescriptor;
        meshDescriptor.m_modelAsset = m_modelAsset;
        meshDescriptor.m_isRayTracingEnabled = false;
        auto meshHandle = GetMeshFeatureProcessor()->AcquireMesh(meshDescriptor, materialMap);
        GetMeshFeatureProcessor()->SetTransform(meshHandle, transform);

        Data::Instance<RPI::Model> model = GetMeshFeatureProcessor()->GetModel(meshHandle);
        if (model)
        {
            m_loadedMeshCounter++;
        }
        else
        {
            m_meshLoadEventHandlers.push_back(AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler
                {
                    [this](AZ::Data::Instance<AZ::RPI::Model> model) { m_loadedMeshCounter++; }
                });
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(meshHandle, m_meshLoadEventHandlers.back());
        }
        
        m_meshHandles.push_back(AZStd::move(meshHandle));
        m_materials.push_back(material);
    }

    void DynamicMaterialTestComponent::DestroyLatticeInstances()
    {
        for (auto& meshHandle : m_meshHandles)
        {
            GetMeshFeatureProcessor()->ReleaseMesh(meshHandle);
        }
        m_meshHandles.clear();
        m_materials.clear();

        m_loadedMeshCounter = 0;
        m_waitingForMeshes = false;
        m_meshLoadEventHandlers.clear();
    }

    void DynamicMaterialTestComponent::InitMaterialConfigs()
    {
        using namespace AZ::RPI;

        MaterialConfig config;

        config.m_name = "Default StandardPBR Material";
        config.m_materialAsset = AssetUtils::GetAssetByProductPath<MaterialAsset>(DefaultPbrMaterialPath, AssetUtils::TraceLevel::Assert);
        config.m_updateLatticeMaterials = [this]() { UpdateStandardPbrColors(); };
        m_materialConfigs.push_back(config);

        config.m_name = "C++ Functor Test Material";
        config.m_materialAsset = AssetUtils::GetAssetByProductPath<MaterialAsset>("materials/dynamicmaterialtest/emissivewithcppfunctors.azmaterial", AssetUtils::TraceLevel::Assert);
        config.m_updateLatticeMaterials = [this]() { UpdateEmissiveMaterialIntensity(); };
        m_materialConfigs.push_back(config);

        config.m_name = "Lua Functor Test Material";
        config.m_materialAsset = AssetUtils::GetAssetByProductPath<MaterialAsset>("materials/dynamicmaterialtest/emissivewithluafunctors.azmaterial", AssetUtils::TraceLevel::Assert);
        config.m_updateLatticeMaterials = [this]() { UpdateEmissiveMaterialIntensity(); };
        m_materialConfigs.push_back(config);

        m_currentMaterialConfig = 0;
    }

    void DynamicMaterialTestComponent::UpdateStandardPbrColors()
    {
        static const Color colorOptions[] =
        {
            Color(1.0f, 0.0f, 0.0f, 1.0f),
            Color(0.0f, 1.0f, 0.0f, 1.0f),
            Color(0.0f, 0.0f, 1.0f, 1.0f),
            Color(1.0f, 1.0f, 0.0f, 1.0f),
            Color(0.0f, 1.0f, 1.0f, 1.0f),
            Color(1.0f, 0.0f, 1.0f, 1.0f),
        };

        // Create a new SimpleLcgRandom every time to keep a consistent seed and consistent color selection.
        SimpleLcgRandom random;

        for (int i = 0; i < m_meshHandles.size(); ++i)
        {
            auto& material = m_materials[i];

            static const float CylesPerSecond = 0.5f;
            const float t = aznumeric_cast<float>(sin(m_currentTime * CylesPerSecond * AZ::Constants::TwoPi) * 0.5f + 0.5f);

            const int colorIndexA = random.GetRandom() % AZ_ARRAY_SIZE(colorOptions);
            int colorIndexB = colorIndexA;
            while (colorIndexA == colorIndexB)
            {
                colorIndexB = random.GetRandom() % AZ_ARRAY_SIZE(colorOptions);
            }
            const Color color = colorOptions[colorIndexA] * t + colorOptions[colorIndexB] * (1.0f - t);

            MaterialPropertyIndex colorProperty = material->FindPropertyIndex(AZ::Name{"baseColor.color"});
            material->SetPropertyValue(colorProperty, color);
        }
    }

    void DynamicMaterialTestComponent::UpdateEmissiveMaterialIntensity()
    {
        for (int i = 0; i < m_meshHandles.size(); ++i)
        {
            auto& meshHandle = m_meshHandles[i];
            auto& material = m_materials[i];

            const float distance = GetMeshFeatureProcessor()->GetTransform(meshHandle).GetTranslation().GetLengthEstimate();

            static const float DistanceScale = 0.02f;
            static const float CylesPerSecond = 0.5f;

            const float t = aznumeric_cast<float>(sin((DistanceScale * distance + m_currentTime * CylesPerSecond) * AZ::Constants::TwoPi) * 0.5f + 0.5f);

            static const float MinIntensity = 1.0f;
            static const float MaxIntensity = 4.0f;
            const float intensity = AZ::Lerp(MinIntensity, MaxIntensity, t);

            MaterialPropertyIndex intensityProperty = material->FindPropertyIndex(AZ::Name{"emissive.intensity"});
            material->SetPropertyValue(intensityProperty, intensity);
        }
    }

    void DynamicMaterialTestComponent::CompileMaterials()
    {
        AZ::Debug::Timer timer;
        timer.Stamp();

        for (auto& material : m_materials)
        {
            material->Compile();
        }

        m_compileTimer.PushValue(timer.GetDeltaTimeInSeconds() * 1'000'000);
    }

    void DynamicMaterialTestComponent::OnTick(float deltaTime, ScriptTimePoint /*scriptTime*/)
    {
        AZ_TRACE_METHOD();

        if (m_waitingForMeshes)
        {
            if(m_loadedMeshCounter == m_meshHandles.size())
            {
                m_waitingForMeshes = false;
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);

                // Reset the clock so we get consistent animation for scripts
                m_currentTime = 0;
            }
        }

        AZ_Assert(m_loadedMeshCounter <= m_meshHandles.size(), "Mesh load handlers were called multiple times?");

        bool updateMaterials = false;

        if (!m_pause)
        {
            m_currentTime += deltaTime;
            updateMaterials = true;
        }

        if (m_imguiSidebar.Begin())
        {
            RenderImGuiLatticeControls();

            ImGui::Separator();

            bool configChanged = false;

            for (int i = 0; i < m_materialConfigs.size(); ++i)
            {
                configChanged = configChanged || ScriptableImGui::RadioButton(m_materialConfigs[i].m_name.c_str(), &m_currentMaterialConfig, i);
            }

            if (configChanged)
            {
                RebuildLattice();
            }

            ImGui::Separator();

            ScriptableImGui::Checkbox("Pause", &m_pause);
            
            if (ScriptableImGui::Button("Reset Clock"))
            {
                m_currentTime = 0;
                updateMaterials = true;
            }

            ImGui::Separator();

            ImGui::Text("%d unique objects", aznumeric_cast<int32_t>(m_meshHandles.size()));

            ImGui::Text("Total Material Compile Time:");

            ImGuiHistogramQueue::WidgetSettings settings;
            settings.m_units = "microseconds";
            m_compileTimer.Tick(deltaTime, settings);

            ImGui::Text("Average per Material: %4.2f", m_compileTimer.GetDisplayedAverage() / m_materials.size());

            ImGui::Separator();

            m_imguiSidebar.End();
        }

        if (updateMaterials)
        {
            m_materialConfigs[m_currentMaterialConfig].m_updateLatticeMaterials();
            CompileMaterials();
        }
    }
} // namespace AtomSampleViewer
