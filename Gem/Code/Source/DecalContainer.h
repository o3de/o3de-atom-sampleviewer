/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once
#include <AzCore/std/containers/vector.h>
#include <AzCore/Math/Quaternion.h>
#include <Atom/Feature/Decals/DecalFeatureProcessorInterface.h>


namespace AtomSampleViewer
{
    //! Helper class used by the AtomSampleViewer examples.
    //! Stores a list of decals and will automatically release them upon destruction of the container.
    class DecalContainer
    {
    public:

        DecalContainer(AZ::Render::DecalFeatureProcessorInterface* fp, const AZ::Vector3 position);
        DecalContainer(const DecalContainer&) = delete;
        DecalContainer& operator=(const DecalContainer&) = delete;
        ~DecalContainer();

        void SetNumDecalsActive(int numDecals);
        int GetMaxDecals() const { return aznumeric_cast<int>(m_decals.size()); }
        int GetNumDecalsActive() const { return m_numDecalsActive; }
        void CloneFrom(const DecalContainer& containerToClone);

    private:

        void SetupDecals();
        void SetupNewDecal(const AZ::Vector3 position, const AZ::Vector3 halfSize, const char* const decalMaterialName);
        void AcquireDecal(int i);
        void ReleaseDecal(int i);

        struct Decal
        {
            AZ::Vector3 m_position;
            AZ::Vector3 m_halfSize;
            AZ::Quaternion m_quaternion;
            const char* m_materialName = nullptr;
            AZ::Render::DecalFeatureProcessorInterface::DecalHandle m_decalHandle;
        };

        AZStd::vector<Decal> m_decals;
        AZ::Render::DecalFeatureProcessorInterface* m_decalFeatureProcessor = nullptr;
        int m_numDecalsActive = 0;
        AZ::Vector3 m_position;
    };
} // namespace AtomSampleViewer
