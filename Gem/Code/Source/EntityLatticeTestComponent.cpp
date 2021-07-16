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

        m_defaultIbl.Init(RPI::RPISystemInterface::Get()->GetDefaultScene().get());
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

        static Vector3 distance(5.0f, 5.0f, 5.0f);
        for (int32_t x = 0; x < m_latticeWidth; ++x)
        {
            for (int32_t y = 0; y < m_latticeDepth; ++y)
            {
                for (int32_t z = 0; z < m_latticeHeight; ++z)
                {
                    Vector3 position(
                        static_cast<float>(x) * distance.GetX(),
                        static_cast<float>(y) * distance.GetY(),
                        static_cast<float>(z) * distance.GetZ());

                    transform.SetTranslation(position);
                    CreateLatticeInstance(transform);
                }
            }
        }
        FinalizeLatticeInstances();
    }

    uint32_t EntityLatticeTestComponent::GetInstanceCount() const
    {
        return m_latticeWidth * m_latticeHeight * m_latticeDepth;
    }

    void EntityLatticeTestComponent::SetLatticeDimensions(uint32_t width, uint32_t depth, uint32_t height)
    {
        m_latticeWidth = GetClamp<int32_t>(width, 1, s_latticeSizeMax);
        m_latticeHeight = GetClamp<int32_t>(height, 1, s_latticeSizeMax);
        m_latticeDepth = GetClamp<int32_t>(depth, 1, s_latticeSizeMax);
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

        if (latticeChanged)
        {
            RebuildLattice();
        }
    }
} // namespace AtomSampleViewer
