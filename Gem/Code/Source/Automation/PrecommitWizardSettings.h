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
#include <AzCore/Preprocessor/Enum.h>

namespace AtomSampleViewer
{
    struct PrecommitWizardSettings
    {
        static const int DefaultInspectionSelection = -1;
        enum class Stage
        {
            Intro,
            RunFullsuiteTest,
            ReportFullsuiteSummary,
            ManualInspection,
            ReportFinalSummary
        };

        struct ImageDifferenceLevel
        {
            enum Levels
            {
                NoDifference = 0,
                LowDifference = 1,
                ModerateDifference = 2,
                HighDifference = 3,

                NumDifferenceLevels = 4
            };
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

        // This function does the following:
        // 1. Collect passing screenshot tests and sorts them by decreasing diff score.
        // 2. Collect failed screenshot tests and sorts them by decreasing diff score. 
        void ProcessScriptReports(const AZStd::vector<ScriptReporter::ScriptReport>& scriptReports)
        {
            m_reportsOrderedByThresholdToInspect.clear();
            m_failedReports.clear();

            for (size_t i = 0; i < scriptReports.size(); ++i)
            {
                const AZStd::vector<ScriptReporter::ScreenshotTestInfo>& screenshotTestInfos = scriptReports[i].m_screenshotTests;
                for (size_t j = 0; j < screenshotTestInfos.size(); ++j)
                {
                    // Collect and sort reports that passed by threshold. This will be used to detect false negatives
                    // e.g. a test is reported to pass by being below the threshold when in fact it's simply because the threshold is too
                    // high
                    if (screenshotTestInfos[j].m_officialComparisonResult.m_resultCode == ScriptReporter::ImageComparisonResult::ResultCode::Pass)
                    {
                        m_reportsOrderedByThresholdToInspect.insert(AZStd::pair<float, ScriptReporter::ReportIndex>(
                            screenshotTestInfos[j].m_officialComparisonResult.m_diffScore,
                            ScriptReporter::ReportIndex{ i, j }));
                    }
                    else
                    {
                        m_failedReports.insert(AZStd::pair<float, ScriptReporter::ReportIndex>(
                            screenshotTestInfos[j].m_officialComparisonResult.m_diffScore,
                            ScriptReporter::ReportIndex{ i, j }));
                    }
                }
            }

            m_reportIterator = m_reportsOrderedByThresholdToInspect.begin();
        }

        int m_inspectionSelection = DefaultInspectionSelection;
        Stage m_stage = Stage::Intro;
        AZStd::string m_exportedPngPath;
        AZStd::multimap<float, ScriptReporter::ReportIndex, AZStd::greater<float>> m_reportsOrderedByThresholdToInspect;
        AZStd::multimap<float, ScriptReporter::ReportIndex, AZStd::greater<float>> m_failedReports;
        AZStd::multimap<float, ScriptReporter::ReportIndex, AZStd::greater<float>>::iterator m_reportIterator;
        AZStd::unordered_map<ScriptReporter::ReportIndex, ImageDifferenceLevel::Levels> m_reportIndexDifferenceLevelMap;
    };
} // namespace AtomSampleViewer
