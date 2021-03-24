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