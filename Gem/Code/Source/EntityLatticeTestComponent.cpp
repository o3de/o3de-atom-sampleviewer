/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <EntityLatticeTestComponent.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <AzCore/Component/Entity.h>

#include <AzFramework/Components/TransformComponent.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>

#include <RHI/BasicRHIComponent.h>
#include <EntityLatticeTestComponent_Traits_Platform.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    constexpr int32_t s_latticeSizeMax = ENTITY_LATTEST_TEST_COMPONENT_MAX;
    constexpr float s_spacingMax = ENTITY_LATTEST_TEST_COMPONENT_SPACING_MAX;
    constexpr float s_entityScaleMax = ENTITY_LATTEST_TEST_COMPONENT_ENTITY_SCALE_MAX;

    void EntityLatticeTestComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<EntityLatticeTestComponent, Component>()
                ->Version(0)
                ;
        }
    }

    void EntityLatticeTestComponent::Activate()
    {
        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<Debug::NoClipControllerComponent>());

        m_defaultIbl.Init(m_scene);
        BuildLattice();
    }

    void EntityLatticeTestComponent::Deactivate()
    {
        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Disable);

        DestroyLatticeInstances();
        m_defaultIbl.Reset();
    }

    void EntityLatticeTestComponent::RebuildLattice()
    {
        DestroyLatticeInstances();
        BuildLattice();
    }

    void EntityLatticeTestComponent::BuildLattice()
    {
        PrepareCreateLatticeInstances(GetInstanceCount());

        // We first rotate the model by 180 degrees before translating it. This is to make it face the camera as it did
        // when the world was Y-up.
        Transform transform = Transform::CreateRotationZ(Constants::Pi);
        m_worldAabb = AZ::Aabb::CreateNull();

        for (int32_t x = 0; x < m_latticeWidth; ++x)
        {
            for (int32_t y = 0; y < m_latticeDepth; ++y)
            {
                for (int32_t z = 0; z < m_latticeHeight; ++z)
                {
                    Vector3 position(
                        static_cast<float>(x) * m_spacingX,
                        static_cast<float>(y) * m_spacingY,
                        static_cast<float>(z) * m_spacingZ);

                    transform.SetTranslation(position);
                    transform.SetUniformScale(m_entityScale);
                    CreateLatticeInstance(transform);
                    m_worldAabb.AddPoint(transform.GetTranslation());
                }
            }
        }
        FinalizeLatticeInstances();
    }

    uint32_t EntityLatticeTestComponent::GetInstanceCount() const
    {
        return m_latticeWidth * m_latticeHeight * m_latticeDepth;
    }

    AZ::Aabb EntityLatticeTestComponent::GetLatticeAabb() const
    {
        return m_worldAabb;
    }

    void EntityLatticeTestComponent::SetLatticeDimensions(uint32_t width, uint32_t depth, uint32_t height)
    {
        m_latticeWidth  = AZ::GetClamp<int32_t>(width, 1, s_latticeSizeMax);
        m_latticeHeight = AZ::GetClamp<int32_t>(height, 1, s_latticeSizeMax);
        m_latticeDepth  = AZ::GetClamp<int32_t>(depth, 1, s_latticeSizeMax);
    }

    void EntityLatticeTestComponent::SetLatticeSpacing( float spaceX, float spaceY, float spaceZ)
    {
        m_spacingX = AZ::GetClamp<float>(spaceX, 0.0f, s_spacingMax);
        m_spacingY = AZ::GetClamp<float>(spaceY, 0.0f, s_spacingMax);
        m_spacingZ = AZ::GetClamp<float>(spaceZ, 0.0f, s_spacingMax);
    }

    void EntityLatticeTestComponent::SetLatticeEntityScale(float scale)
    {
        m_entityScale = AZ::GetClamp<float>(scale, 0.0f, s_entityScaleMax);
    }

    void EntityLatticeTestComponent::SetIBLExposure(float exposure)
    {
        m_defaultIbl.SetExposure(exposure);
    }

    void EntityLatticeTestComponent::RenderImGuiLatticeControls()
    {
        bool latticeChanged = false;

        ImGui::Text("Lattice Width");
        latticeChanged |= ScriptableImGui::SliderInt("##LatticeWidth", &m_latticeWidth, 1, s_latticeSizeMax);

        ImGui::Spacing();

        ImGui::Text("Lattice Height");
        latticeChanged |= ScriptableImGui::SliderInt("##LatticeHeight", &m_latticeHeight, 1, s_latticeSizeMax);

        ImGui::Spacing();

        ImGui::Text("Lattice Depth");
        latticeChanged |= ScriptableImGui::SliderInt("##LatticeDepth", &m_latticeDepth, 1, s_latticeSizeMax);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Lattice Spacing X");
        latticeChanged |= ScriptableImGui::SliderFloat("##LatticeSpaceX", &m_spacingX, 0.5, s_spacingMax);

        ImGui::Spacing();

        ImGui::Text("Lattice Spacing Y");
        latticeChanged |= ScriptableImGui::SliderFloat("##LatticeSpaceY", &m_spacingY, 0.5, s_spacingMax);

        ImGui::Spacing();

        ImGui::Text("Lattice Spacing Z");
        latticeChanged |= ScriptableImGui::SliderFloat("##LatticeSpaceZ", &m_spacingZ, 0.5, s_spacingMax);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Entity Scale");
        latticeChanged |= ScriptableImGui::SliderFloat("##EntityScale", &m_entityScale, 0.01, s_entityScaleMax);


        if (latticeChanged)
        {
            RebuildLattice();
        }
    }
} // namespace AtomSampleViewer
