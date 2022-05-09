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
        InitDefaultValues(m_testParameters);
    }

    void _100KDraw10KDrawableExampleComponent::InitDefaultValues(HighInstanceTestParameters& defaultParameters)
    {
        defaultParameters.m_latticeSize[0] = 22;
        defaultParameters.m_latticeSize[1] = 22;
        defaultParameters.m_latticeSize[2] = 22;

        defaultParameters.m_latticeSpacing[0] = 3.0f;
        defaultParameters.m_latticeSpacing[1] = 3.0f;
        defaultParameters.m_latticeSpacing[2] = 3.0f;

        defaultParameters.m_numShadowCastingSpotLights = 7;
        defaultParameters.m_shadowSpotlightInnerAngleDeg = 30.0f;
        defaultParameters.m_shadowSpotlightOuterAngleDeg = 30.0f;
        defaultParameters.m_shadowSpotlightMaxDistance = 200.0f;
        defaultParameters.m_shadowSpotlightIntensity = 50000.f;

        defaultParameters.m_activateDirectionalLight = true;
        defaultParameters.m_directionalLightIntensity = 5.0f;

        defaultParameters.m_cameraPosition[0] = -81.0f;
        defaultParameters.m_cameraPosition[1] = 31.0f;
        defaultParameters.m_cameraPosition[2] = 32.0f;
        defaultParameters.m_cameraHeadingDeg = -90.0f;
        defaultParameters.m_cameraPitchDeg = 0.0f;
        defaultParameters.m_iblExposure = -10.0f;
    }

} // namespace AtomSampleViewer
