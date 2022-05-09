/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/100KDrawable_SingleView_ExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AzCore/Serialization/SerializeContext.h>

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer
{
    using namespace AZ;

    void _100KDrawableExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<_100KDrawableExampleComponent, HighInstanceTestComponent>()
                ->Version(0)
                ;
        }
    }


    _100KDrawableExampleComponent::_100KDrawableExampleComponent() 
    {
        m_sampleName = "100KEntityTest";
        InitDefaultValues(m_testParameters);
    }

    void _100KDrawableExampleComponent::InitDefaultValues(HighInstanceTestParameters& defaultParameters)
    {
        defaultParameters.m_latticeSize[0] = 46;
        defaultParameters.m_latticeSize[1] = 46;
        defaultParameters.m_latticeSize[2] = 46;

        defaultParameters.m_latticeSpacing[0] = 3.0f;
        defaultParameters.m_latticeSpacing[1] = 3.0f;
        defaultParameters.m_latticeSpacing[2] = 3.0f;

        defaultParameters.m_numShadowCastingSpotLights = 0;
        defaultParameters.m_activateDirectionalLight = false;

        defaultParameters.m_cameraPosition[0] = -173.0f;
        defaultParameters.m_cameraPosition[1] = 66.0f;
        defaultParameters.m_cameraPosition[2] = 68.0f;
        defaultParameters.m_cameraHeadingDeg = -90.0f;
        defaultParameters.m_cameraPitchDeg = 0.0f;
        defaultParameters.m_iblExposure = 0.0f;
    }

    
} // namespace AtomSampleViewer
