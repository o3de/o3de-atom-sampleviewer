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

#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/string/string.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    class ReflectContext;
}

namespace AtomSampleViewer
{
    struct ImageComparisonToleranceLevel
    {
        AZ_TYPE_INFO(AtomSampleViewer::ImageComparisonToleranceLevel, "{C9B16AE8-71B4-48D8-A1DF-1128600EDE7A}")

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;                     //!< A unique name for this tolerance level
        float m_threshold = 0.0f;                 //!< Range should be 0-1, with 0 meaning no error and 1 meaning as different as possible.
        bool m_filterImperceptibleDiffs = false;  //!< If true, visually imperceptible differences will be filtered out before scoring.

        AZStd::string ToString() const;
    };

    struct ImageComparisonConfig final
    {
        AZ_TYPE_INFO(AtomSampleViewer::ImageComparisonConfig, "{7D5C0F1E-BEB7-4C80-B1A7-EDBD55EF6119}")

        static void Reflect(AZ::ReflectContext* context);

        //! Lists available tolerance levels sorted from most- to least-strict.
        AZStd::vector<ImageComparisonToleranceLevel> m_toleranceLevels;
    };

} // namespace AtomSampleViewer
