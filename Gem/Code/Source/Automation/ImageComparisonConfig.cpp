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

#include <Automation/ImageComparisonConfig.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{

    void ImageComparisonConfig::Reflect(AZ::ReflectContext* context)
    {
        ImageComparisonToleranceLevel::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ImageComparisonConfig>()
                ->Version(0)
                ->Field("toleranceLevels", &ImageComparisonConfig::m_toleranceLevels)
                ;
        }
    }

    void ImageComparisonToleranceLevel::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ImageComparisonToleranceLevel>()
                ->Version(0)
                ->Field("name", &ImageComparisonToleranceLevel::m_name)
                ->Field("threshold", &ImageComparisonToleranceLevel::m_threshold)
                ->Field("filterImperceptibleDiffs", &ImageComparisonToleranceLevel::m_filterImperceptibleDiffs)
                ;
        }
    }

    AZStd::string ImageComparisonToleranceLevel::ToString() const
    {
        return AZStd::string::format("'%s' (threshold %f%s)", m_name.c_str(), m_threshold, m_filterImperceptibleDiffs ? ", filtered" : "");
    }

} // namespace AtomSampleViewer
