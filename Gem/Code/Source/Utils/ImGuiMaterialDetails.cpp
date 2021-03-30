/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <Utils/ImGuiMaterialDetails.h>
#include <Utils/ImGuiShaderUtils.h>
#include <imgui/imgui.h>
#include <Atom/RPI.Public/Material/Material.h>


#include <AzCore/Casting/numeric_cast.h>

namespace AtomSampleViewer
{
    void ImGuiMaterialDetails::SetMaterial(AZ::Data::Instance<AZ::RPI::Material> material)
    {
        m_material = material;
    }

    void ImGuiMaterialDetails::OpenDialog()
    {
        m_dialogIsOpen = true;
    }

    void ImGuiMaterialDetails::Tick()
    {
        if (m_dialogIsOpen)
        {
            if (ImGui::Begin("Material Details", &m_dialogIsOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                if (m_material)
                {
                    const ImGuiTreeNodeFlags flagDefaultOpen = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
                    if (ImGui::TreeNodeEx("Shaders", flagDefaultOpen))
                    {
                        for (size_t i = 0; i < m_material->GetShaderCollection().size(); i++)
                        {
                            const AZ::RPI::ShaderCollection::Item& shaderItem = m_material->GetShaderCollection()[i];

                            if (ImGui::TreeNodeEx(AZStd::string::format("Shader[%d] - Enabled=%d - %s", aznumeric_cast<int>(i), shaderItem.IsEnabled(), shaderItem.GetShaderAsset()->GetName().GetCStr()).c_str(), flagDefaultOpen))
                            {
                                if (shaderItem.IsEnabled())
                                {
                                    AZ::RPI::ShaderVariantId requestedVariantId = shaderItem.GetShaderVariantId();
                                    AZ::Data::Asset<AZ::RPI::ShaderVariantAsset> selectedVariantAsset =
                                        shaderItem.GetShaderAsset()->GetVariant(requestedVariantId);

                                    if (selectedVariantAsset)
                                    {
                                        AZ::RPI::ShaderVariantId selectedVariantId = selectedVariantAsset->GetShaderVariantId();

                                        ImGui::Indent();

                                        ImGuiShaderUtils::DrawShaderVariantTable(
                                            shaderItem.GetShaderAsset()->GetShaderOptionGroupLayout(), requestedVariantId,
                                            selectedVariantId);

                                        ImGui::Unindent();
                                    }
                                }

                                ImGui::TreePop();
                            }
                        }

                        ImGui::TreePop();
                    }
                }
                else
                {
                    ImGui::Text("No material selected");
                }
            }
            ImGui::End();
        }
    }
} // namespace AtomSampleViewer
