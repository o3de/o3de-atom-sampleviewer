/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>
#include <Utils/Utils.h>
#include <EntityLatticeTestComponent_Traits_Platform.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    //! Common base class for test components that display a lattice of entities.
    class EntityLatticeTestComponent
        : public CommonSampleComponentBase
    {
    public:
        AZ_RTTI(EntityLatticeTestComponent, "{73C13F66-6F5B-43D3-B1F0-CB4F7BEA1334}", CommonSampleComponentBase)

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    protected:

        //! Returns total number of instances (width * height * depth)
        uint32_t GetInstanceCount() const;
        
        //! Call this to render ImGui controls for controlling the size of the lattice.
        void RenderImGuiLatticeControls();
        
        //! Destroys and rebuilds the lattice.
        virtual void RebuildLattice();

        void SetLatticeDimensions(uint32_t width, uint32_t depth, uint32_t height);

    private:

        //! Called once before CreateLatticeInstance() is called for each instance so the subclass can prepare for the total number of instances.
        virtual void PrepareCreateLatticeInstances(uint32_t instanceCount) = 0;

        //! This is called for each entity in the lattice when it is being built. The subclass should attach
        //! whatever components are necessary to achieve the desired result.
        virtual void CreateLatticeInstance(const AZ::Transform& transform) = 0;

        //! This is called after all the instances are created to any final work. Not required.
        virtual void FinalizeLatticeInstances() {};

        //! Called when the subclass should destroy all of its instances, either because of shutdown or recreation.
        virtual void DestroyLatticeInstances() = 0;

        void BuildLattice();

        // These are signed to avoid casting with imgui controls.
        int32_t m_latticeWidth = ENTITY_LATTICE_TEST_COMPONENT_WIDTH;
        int32_t m_latticeHeight = ENTITY_LATTICE_TEST_COMPONENT_HEIGHT;
        int32_t m_latticeDepth = ENTITY_LATTICE_TEST_COMPONENT_DEPTH;
        
        Utils::DefaultIBL m_defaultIbl;
    };
} // namespace AtomSampleViewer
