/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Performance/100KDraw_10KDrawable_MultiView_ExampleComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AzCore/Serialization/SerializeContext.h>

AZ_DECLARE_BUDGET(AtomSampleViewer);

namespace AtomSampleViewer
{
    using namespace AZ;

    void _100KDraw10KDrawableExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        Base::Reflect(context);
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<_100KDraw10KDrawableExampleComponent, HighInstanceTestComponent>()
                ->Version(0)
                ;
        }
    }


    _100KDraw10KDrawableExampleComponent::_100KDraw10KDrawableExampleComponent() 
    {
        m_sampleName = "100KDraw10KEntityTest";

        for (int index = 0; index < 3; ++index)
        {
            m_testParameters.m_latticeSize[index] = 22;
            m_testParameters.m_latticeSpacing[index] = 3.0f;
        }
        m_testParameters.m_numShadowCastingSpotLights = 7;
        m_testParameters.m_shadowSpotlightInnerAngleDeg = 30.0f;
        m_testParameters.m_shadowSpotlightOuterAngleDeg = 30.0f;
        m_testParameters.m_shadowSpotlightMaxDistance = 200.0f;
        m_testParameters.m_shadowSpotlightIntensity = 50000.f;

        m_testParameters.m_activateDirectionalLight = true;
        m_testParameters.m_directionalLightIntensity = 5.0f;

        m_testParameters.m_cameraPosition[0] = -81.0f;
        m_testParameters.m_cameraPosition[1] = 31.0f;
        m_testParameters.m_cameraPosition[2] = 32.0f;
        m_testParameters.m_cameraHeadingDeg = -90.0f;
        m_testParameters.m_cameraPitchDeg = 0.0f;
        m_testParameters.m_iblExposure = -10.0f;
    }

    
} // namespace AtomSampleViewer
