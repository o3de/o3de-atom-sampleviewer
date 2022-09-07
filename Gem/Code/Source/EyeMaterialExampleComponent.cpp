/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <EyeMaterialExampleComponent.h>

#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <RHI/BasicRHIComponent.h>

#include <Automation/ScriptableImGui.h>


namespace AtomSampleViewer
{
    static const char* MeshPath = "objects/eye.azmodel";
    static const char* MaterialPath = "materials/eye/001_EyeBasic.azmaterial";
    static const float DefaultCameraHeading = 40.0f;
    static const float DefaultCameraDistance = 2.0f;
    
    static const char* IrisColorName = "iris.baseColor.color";
    static const char* IrisColorFactorName = "iris.baseColor.factor";
    static const char* IrisRoughnessName = "iris.roughness.factor";
    
    static const char* ScleraColorName = "sclera.baseColor.color";
    static const char* ScleraColorFactorName = "sclera.baseColor.factor";
    static const char* ScleraRoughnessName = "sclera.roughness.factor";
    static const char* ScleraNormalFactorName = "sclera.normal.factor";
    
    static const char* IrisDepthName = "eye.irisDepth";
    static const char* IrisRadiusName = "eye.irisRadius";
    static const char* InnerEyeIORName = "eye.innerEyeIOR";
    static const char* LimbusSizeName = "eye.limbusSize";

    static const char* SpecularFactorName = "specularF0.factor";

    static const char* SSSEnableName = "subsurfaceScattering.enableSubsurfaceScattering";
    static const char* SSSColorName = "subsurfaceScattering.scatterColor";
    static const char* SSSFactorName = "subsurfaceScattering.subsurfaceScatterFactor";

    void EyeMaterialExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EyeMaterialExampleComponent, AZ::Component>()->Version(0);
        }
    }

    EyeMaterialExampleComponent::EyeMaterialExampleComponent()
    {
    }

    void EyeMaterialExampleComponent::Activate()
    {
        Prepare();

        m_eyeTransform = AZ::Transform::CreateIdentity();
        LoadMesh(m_eyeTransform);

        InitializeMaterialProperties();

        AZ::TickBus::Handler::BusConnect();
    }

    void EyeMaterialExampleComponent::Deactivate()
    {
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        m_defaultIbl.Reset();

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        AZ::TickBus::Handler::BusDisconnect();
    }

    void EyeMaterialExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(scriptTime);

        DrawSidebar();
    }

    void EyeMaterialExampleComponent::LoadMesh(AZ::Transform transform)
    {
        m_materialInstance = AZ::RPI::Material::Create(m_materialAsset);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_modelAsset }, m_materialInstance);
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, transform);
    }

    void EyeMaterialExampleComponent::Prepare()
    {
        // Camera
        AZ::Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::ArcBallControllerComponent>());
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetDistance, DefaultCameraDistance);
        AZ::Debug::ArcBallControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::ArcBallControllerRequestBus::Events::SetHeading, DefaultCameraHeading);

        // Lighting
        m_defaultIbl.Init(m_scene);
        // Model
        m_modelAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::ModelAsset>(MeshPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
        // Material
        m_materialAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::MaterialAsset>(MaterialPath, AZ::RPI::AssetUtils::TraceLevel::Assert);
    }


    void EyeMaterialExampleComponent::InitializeMaterialProperties()
    {
        // Get material indices of properties
        m_irisColorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(IrisColorName));
        m_irisColorFactorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(IrisColorFactorName));
        m_irisRoughnessIndex = m_materialInstance->FindPropertyIndex(AZ::Name(IrisRoughnessName));

        m_scleraColorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(ScleraColorName));
        m_scleraColorFactorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(ScleraColorFactorName));
        m_scleraRoughnessIndex = m_materialInstance->FindPropertyIndex(AZ::Name(ScleraRoughnessName));
        m_scleraNormalFactorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(ScleraNormalFactorName));

        m_irisDepthIndex = m_materialInstance->FindPropertyIndex(AZ::Name(IrisDepthName));
        m_irisRadiusIndex = m_materialInstance->FindPropertyIndex(AZ::Name(IrisRadiusName));
        m_innerEyeIORIndex = m_materialInstance->FindPropertyIndex(AZ::Name(InnerEyeIORName));
        m_limbusSizeIndex = m_materialInstance->FindPropertyIndex(AZ::Name(LimbusSizeName));

        m_specularFactorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(SpecularFactorName));

        m_SSSEnableIndex = m_materialInstance->FindPropertyIndex(AZ::Name(SSSEnableName));
        m_SSSColorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(SSSColorName));
        m_SSSFactorIndex = m_materialInstance->FindPropertyIndex(AZ::Name(SSSFactorName));
        
        // Assign material property values to the GUI variables so that ImGui displays them properly
        m_materialInstance->GetPropertyValue(m_irisColorIndex).GetValue<AZ::Color>().GetAsVector3().StoreToFloat3(m_irisColor);
        m_irisColorFactor = m_materialInstance->GetPropertyValue(m_irisColorFactorIndex).GetValue<float>();
        m_irisRoughness = m_materialInstance->GetPropertyValue(m_irisRoughnessIndex).GetValue<float>();

        m_materialInstance->GetPropertyValue(m_scleraColorIndex).GetValue<AZ::Color>().GetAsVector3().StoreToFloat3(m_scleraColor);
        m_scleraColorFactor = m_materialInstance->GetPropertyValue(m_scleraColorFactorIndex).GetValue<float>();
        m_scleraRoughness = m_materialInstance->GetPropertyValue(m_scleraRoughnessIndex).GetValue<float>();
        m_scleraNormalFactor = m_materialInstance->GetPropertyValue(m_scleraNormalFactorIndex).GetValue<float>();

        m_irisDepth = m_materialInstance->GetPropertyValue(m_irisDepthIndex).GetValue<float>();
        m_irisRadius = m_materialInstance->GetPropertyValue(m_irisRadiusIndex).GetValue<float>();
        m_innerEyeIOR = m_materialInstance->GetPropertyValue(m_innerEyeIORIndex).GetValue<float>();
        m_limbusSize = m_materialInstance->GetPropertyValue(m_limbusSizeIndex).GetValue<float>();

        m_specularFactor = m_materialInstance->GetPropertyValue(m_specularFactorIndex).GetValue<float>();

        m_SSSEnable = m_materialInstance->GetPropertyValue(m_SSSEnableIndex).GetValue<bool>();
        m_materialInstance->GetPropertyValue(m_SSSColorIndex).GetValue<AZ::Color>().GetAsVector3().StoreToFloat3(m_SSSColor);
        m_SSSFactor = m_materialInstance->GetPropertyValue(m_SSSFactorIndex).GetValue<float>();
    }

    void EyeMaterialExampleComponent::DrawSidebar()
    {
        using namespace AZ::Render;

        if (m_imguiSidebar.Begin())
        {
            ImGui::Spacing();

            DrawSidebarMaterialProperties();

            ImGui::Separator();

            if (ScriptableImGui::Button("Material Details..."))
            {
                m_imguiMaterialDetails.OpenDialog();
            }

            m_imguiSidebar.End();
        }
        m_imguiMaterialDetails.Tick(&GetMeshFeatureProcessor()->GetDrawPackets(m_meshHandle), "Eye Mesh");
    }
    
    void EyeMaterialExampleComponent::DrawSidebarMaterialProperties()
    {
        bool eyeSettingsChanged = false;
        if (ImGui::CollapsingHeader("Iris", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::ColorEdit3("Color##irisColor", m_irisColor, ImGuiColorEditFlags_None))
            {
                m_materialInstance->SetPropertyValue(m_irisColorIndex, AZ::Color(m_irisColor[0], m_irisColor[1], m_irisColor[2], 1.0));
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Color Factor##irisColorFactor", &m_irisColorFactor, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_irisColorFactorIndex, m_irisColorFactor);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Roughness##irisRoughness", &m_irisRoughness, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_irisRoughnessIndex, m_irisRoughness);
                eyeSettingsChanged = true;
            }
            ImGui::Unindent();
        }
        if (ImGui::CollapsingHeader("Sclera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::ColorEdit3("Color##scleraColor", m_scleraColor, ImGuiColorEditFlags_None))
            {
                m_materialInstance->SetPropertyValue(m_scleraColorIndex, AZ::Color(m_scleraColor[0], m_scleraColor[1], m_scleraColor[2], 1.0));
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Color Factor##scleraColorFactor", &m_scleraColorFactor, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_scleraColorFactorIndex, m_scleraColorFactor);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Roughness##scleraRoughness", &m_scleraRoughness, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_scleraRoughnessIndex, m_scleraRoughness);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Normal Factor", &m_scleraNormalFactor, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_scleraNormalFactorIndex, m_scleraNormalFactor);
                eyeSettingsChanged = true;
            }
            ImGui::Unindent();
        }
        if (ImGui::CollapsingHeader("General Eye Properties", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::SliderFloat("Iris Depth", &m_irisDepth, 0.f, 0.5f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_irisDepthIndex, m_irisDepth);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Inner Radius", &m_irisRadius, 0.f, 0.5f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_irisRadiusIndex, m_irisRadius);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Inner IOR", &m_innerEyeIOR, 1.f, 2.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_innerEyeIORIndex, m_innerEyeIOR);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Limbus Size", &m_limbusSize, 0.f, 0.5f, "%.3f", ImGuiSliderFlags_Logarithmic))
            {
                m_materialInstance->SetPropertyValue(m_limbusSizeIndex, m_limbusSize);
                eyeSettingsChanged = true;
            }
            ImGui::Unindent();
        }
        if (ImGui::CollapsingHeader("Specular F0", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::SliderFloat("Factor##specularF0Factor", &m_specularFactor, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_specularFactorIndex, m_specularFactor);
                eyeSettingsChanged = true;
            }
            ImGui::Unindent();
        }
        if (ImGui::CollapsingHeader("Subsurface Scattering", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::Checkbox("Enable##enableSSS", &m_SSSEnable))
            {
                m_materialInstance->SetPropertyValue(m_SSSEnableIndex, m_SSSEnable);
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::ColorEdit3("Color##SSSColor", m_SSSColor, ImGuiColorEditFlags_None))
            {
                m_materialInstance->SetPropertyValue(m_SSSColorIndex, AZ::Color(m_SSSColor[0], m_SSSColor[1], m_SSSColor[2], 1.0));
                eyeSettingsChanged = true;
            }
            if (ScriptableImGui::SliderFloat("Factor##SSSColorFactor", &m_SSSFactor, 0.f, 1.f, "%.3f"))
            {
                m_materialInstance->SetPropertyValue(m_SSSFactorIndex, m_SSSFactor);
                eyeSettingsChanged = true;
            }
            ImGui::Unindent();
        }

        bool transformChanged = false;
        
        if (ImGui::CollapsingHeader("Eye Orientation", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            if (ScriptableImGui::SliderFloat3("Rotation", m_rotationEuler, -180.0, 180.0)) {
                transformChanged = true;
                m_eyeTransform.SetRotation(AZ::Quaternion::CreateFromEulerAnglesDegrees(AZ::Vector3(m_rotationEuler[0], m_rotationEuler[1], m_rotationEuler[2])));
            }

            ImGui::Unindent();
        }

        if (eyeSettingsChanged)
        {
            m_materialInstance->Compile();
        }

        if (transformChanged)
        {
            GetMeshFeatureProcessor()->SetTransform(m_meshHandle, m_eyeTransform);
        }
    }
}
