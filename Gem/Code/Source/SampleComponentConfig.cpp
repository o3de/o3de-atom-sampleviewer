/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <SampleComponentConfig.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void SampleComponentConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SampleComponentConfig, AZ::ComponentConfig>()
                ->Version(1)
                ;
        }
    }

    bool SampleComponentConfig::IsValid() const
    {
        bool valid = true;

        valid &= m_windowContext.get() != nullptr;
        valid &= m_cameraEntityId != AZ::EntityId(AZ::EntityId::InvalidEntityId);
        valid &= !m_entityContextId.IsNull();

        return valid;
    }
} // namespace AtomSampleViewer