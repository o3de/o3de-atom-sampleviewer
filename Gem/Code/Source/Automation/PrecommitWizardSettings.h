/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Automation/ScriptReporter.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/map.h>

namespace AtomSampleViewer
{
    struct PrecommitWizardSettings
    {
        
        enum Stage
        {
            INTRO,
            REPORT_FULLSUITE_SUMMARY,
            MANUAL_INSPECTION,
            REPORT_FINAL_SUMMARY
        };

        enum ImageDifferenceLevel {
            NO_DIFFERENCE = 0,
            LOW_DIFFERENCE = 1,
            MODERATE_DIFFERENCE = 2,
            HIGH_DIFFERENCE = 3,

            NUM_DIFFERENCE_LEVELS = 4
        };
        static constexpr const char* ManualInspectionDifferenceLevels[] = {
            "No Difference",
            "Low Difference",
            "Moderate Difference",
            "High Difference"
        };
        static constexpr const char* ManualInspectionOptions[] = {
            "I don't see any difference",
            "I see a benign difference",
            "I see a difference that's *probably* benign",
            "This looks like a problem"
        };

        // currently set to track the ScriptReport index and the ScreenshotTestInfo index.
        using ReportIndex = AZStd::pair<size_t, size_t>;

        void GenerateReportsToInpsect(const AZStd::vector<ScriptReporter::ScriptReport>& scriptReports)
        {
            m_reportsOrderedByThresholdToInspect.clear();

            for (size_t i = 0; i < scriptReports.size(); ++i)
            {
                const AZStd::vector<ScriptReporter::ScreenshotTestInfo>& screenshotTestInfos = scriptReports[i].m_screenshotTests;
                for (size_t j = 0; j < screenshotTestInfos.size(); ++j)
                {
                    if (screenshotTestInfos[j].m_officialComparisonResult.m_resultCode == ScriptReporter::ImageComparisonResult::ResultCode::ThresholdExceeded)
                    {
                        m_reportsOrderedByThresholdToInspect.insert(AZStd::pair<float, ReportIndex>(
                            screenshotTestInfos[j].m_officialComparisonResult.m_finalDiffScore,
                            ReportIndex{ i, j }));
                    }
                }
            }

            m_reportIterator = m_reportsOrderedByThresholdToInspect.begin();
        }

        int m_inspectionSelection = 1;
        Stage m_stage = Stage::INTRO;
        AZStd::string m_exportedPngPath = "";
        AZStd::multimap<float, ReportIndex> m_reportsOrderedByThresholdToInspect;
        AZStd::multimap<float, ReportIndex>::iterator m_reportIterator;
        AZStd::unordered_map<ReportIndex, ImageDifferenceLevel> m_reportIndexDifferenceLevelMap;
    };
} // namespace AtomSampleViewer
