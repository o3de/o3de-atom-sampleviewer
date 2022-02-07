/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/100KEntityExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AzCore/Serialization/SerializeContext.h>

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer
{
    using namespace AZ;

    void HundredKEntityExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HundredKEntityExampleComponent, HighInstanceTestComponent>()
                ->Version(0)
                ;
        }
    }


    HundredKEntityExampleComponent::HundredKEntityExampleComponent() 
    {
        m_sampleName = "100KEntityTest";

        for (int index = 0; index < 3; ++index)
        {
            m_testParameters.m_latticeSize[index] = 46;
            m_testParameters.m_latticeSpacing[index] = 3.0f;
        }
        m_testParameters.m_numShadowCastingSpotLights = 0;
        m_testParameters.m_activateDirectionalLight = false;

        m_testParameters.m_cameraPosition[0] = -173.0f;
        m_testParameters.m_cameraPosition[1] = 66.0f;
        m_testParameters.m_cameraPosition[2] = 68.0f;
        m_testParameters.m_cameraHeadingDeg = -90.0f;
        m_testParameters.m_cameraPitchDeg = 0.0f;
        m_testParameters.m_iblExposure = 0.0f;
    }
    
} // namespace AtomSampleViewer
