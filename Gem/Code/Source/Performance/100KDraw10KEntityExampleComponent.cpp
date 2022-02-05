/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/100KDraw10KEntityExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AzCore/Serialization/SerializeContext.h>

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer
{
    using namespace AZ;

    void HundredKDraw10KEntityExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HundredKDraw10KEntityExampleComponent, HighInstanceTestComponent>()
                ->Version(0)
                ;
        }
    }


    HundredKDraw10KEntityExampleComponent::HundredKDraw10KEntityExampleComponent() 
    {
        m_sampleName = "100KDraw10KEntityTest";

        for (int index = 0; index < 3; ++index)
        {
            m_testParameters.m_latticeSize[index] = 22;
            m_testParameters.m_latticeSpacing[index] = 5.0f;
        }
        m_testParameters.m_numShadowCastingSpotLights = 7;
        m_testParameters.m_shadowSpotlightOuterAngleDeg = 90.0f;
        m_testParameters.m_shadowSpotlightMaxDistance = 120.0f;
        m_testParameters.m_shadowSpotlightIntensity = 500.f;

        m_testParameters.m_activateDirectionalLight = true;
    }
    
} // namespace AtomSampleViewer
