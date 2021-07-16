/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Automation/ScriptReporter.h>
#include <Utils/Utils.h>
#include <imgui/imgui.h>
#include <Atom/RHI/Factory.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Utils/Utils.h>

namespace AtomSampleViewer
{
    // Must match ScriptReporter::DisplayOption Enum
    static const char* DiplayOptions[] =
    {
        "All Results", "Warnings & Errors", "Errors Only",
    };

    namespace ScreenshotPaths
    {
        AZStd::string GetScreenshotsFolder(bool resolvePath)
        {
            AZStd::string path = "@user@/scripts/screenshots/";

            if (resolvePath)
            {
                path = Utils::ResolvePath(path);
            }

            return path;
        }

        AZStd::string GetLocalBaselineFolder(bool resolvePath)
        {
            AZStd::string path = AZStd::string::format("@user@/scripts/screenshotslocalbaseline/%s", AZ::RHI::Factory::Get().GetName().GetCStr());

            if (resolvePath)
            {
                path = Utils::ResolvePath(path);
            }

            return path;
        }

        AZStd::string GetOfficialBaselineFolder(bool resolvePath)
        {
            AZStd::string path = "scripts/expectedscreenshots/";

            if (resolvePath)
            {
                path = Utils::ResolvePath(path);
            }

            return path;
        }

        AZStd::string GetLocalBaseline(const AZStd::string& forScreenshotFile)
        {
            AZStd::string localBaselineFolder = GetLocalBaselineFolder(false);
            AzFramework::StringFunc::Replace(localBaselineFolder, "@user@/", "");
            AZStd::string newPath = forScreenshotFile;
            if (!AzFramework::StringFunc::Replace(newPath, "scripts/screenshots", localBaselineFolder.c_str()))
            {
                newPath = "";
            }
            return newPath;
        }

        AZStd::string GetOfficialBaseline(const AZStd::string& forScreenshotFile)
        {
            AZStd::string path = forScreenshotFile;
            const AZStd::string userPath = Utils::ResolvePath("@user@");

            // make the path relative to the user folder
            if (!AzFramework::StringFunc::Replace(path, userPath.c_str(), ""))
            {
                return "";
            }

            // After replacing "screenshots" with "expectedscreenshots", the path should be a valid asset path, relative to asset root.
            if (!AzFramework::StringFunc::Replace(path, "scripts/screenshots", "scripts/expectedscreenshots"))
            {
                return "";
            }

            // Turn it back into a full path
            path = Utils::ResolvePath("@devassets@" + path);

            return path;
        }
    }

    AZStd::string ScriptReporter::ImageComparisonResult::GetSummaryString() const
    {
        AZStd::string resultString;

        if (m_resultCode == ResultCode::ThresholdExceeded || m_resultCode == ResultCode::Pass)
        {
            resultString = AZStd::string::format("Diff Score: %f", m_finalDiffScore);
        }
        else if (m_resultCode == ResultCode::WrongSize)
        {
            resultString = "Wrong size";
        }
        else if (m_resultCode == ResultCode::FileNotFound)
        {
            resultString = "File not found";
        }
        else if (m_resultCode == ResultCode::FileNotLoaded)
        {
            resultString = "File load failed";
        }
        else if (m_resultCode == ResultCode::WrongFormat)
        {
            resultString = "Format is not supported";
        }
        else if (m_resultCode == ResultCode::NullImageComparisonToleranceLevel)
        {
            resultString = "ImageComparisonToleranceLevel not provided";
        }
        else if (m_resultCode == ResultCode::None)
        {
            // "None" could be the case if the results dialog is open while the script is running
            resultString = "No results";
        }
        else 
        {
            resultString = "Unhandled Image Comparison ResultCode";
            AZ_Assert(false, "Unhandled Image Comparison ResultCode");
        }

        return resultString;
    }

    void ScriptReporter::SetAvailableToleranceLevels(const AZStd::vector<ImageComparisonToleranceLevel>& toleranceLevels)
    {
        m_availableToleranceLevels = toleranceLevels;
    }

    void ScriptReporter::Reset()
    {
        m_scriptReports.clear();
        m_currentScriptIndexStack.clear();
        m_invalidationMessage.clear();
    }

    void ScriptReporter::SetInvalidationMessage(const AZStd::string& message)
    {
        m_invalidationMessage = message;

        // Reporting this message here instead of when running the script so it won't show up as an error in the ImGui report.
        AZ_Error("Automation", m_invalidationMessage.empty(), "Subsequent test results will be invalid because '%s'", m_invalidationMessage.c_str());
    }

    void ScriptReporter::PushScript(const AZStd::string& scriptAssetPath)
    {
        if (GetCurrentScriptReport())
        {
            // Only the current script should listen for Trace Errors
            GetCurrentScriptReport()->BusDisconnect();
        }

        m_currentScriptIndexStack.push_back(m_scriptReports.size());
        m_scriptReports.push_back();
        m_scriptReports.back().m_scriptAssetPath = scriptAssetPath;
        m_scriptReports.back().BusConnect();
    }

    void ScriptReporter::PopScript()
    {
        AZ_Assert(GetCurrentScriptReport(), "There is no active script");

        if (GetCurrentScriptReport())
        {
            GetCurrentScriptReport()->BusDisconnect();
            m_currentScriptIndexStack.pop_back();
        }

        if (GetCurrentScriptReport())
        {
            // Make sure the newly restored current script is listening for Trace Errors
            GetCurrentScriptReport()->BusConnect();
        }
    }

    bool ScriptReporter::HasActiveScript() const
    {
        return !m_currentScriptIndexStack.empty();
    }

    bool ScriptReporter::AddScreenshotTest(const AZStd::string& path)
    {
        AZ_Assert(GetCurrentScriptReport(), "There is no active script");

        ScreenshotTestInfo screenshotTestInfo;
        screenshotTestInfo.m_screenshotFilePath = path;
        GetCurrentScriptReport()->m_screenshotTests.push_back(AZStd::move(screenshotTestInfo));

        return true;
    }

    void ScriptReporter::TickImGui()
    {
        if (m_showReportDialog)
        {
            ShowReportDialog();
        }
    }

    bool ScriptReporter::HasErrorsAssertsInReport() const
    {
        for (const ScriptReport& scriptReport : m_scriptReports)
        {
            if (scriptReport.m_assertCount > 0 || scriptReport.m_generalErrorCount > 0 || scriptReport.m_screenshotErrorCount > 0)
            {
                return true;
            }
        }

        return false;
    }

    void ScriptReporter::ShowDiffButton(const char* buttonLabel, const AZStd::string& imagePathA, const AZStd::string& imagePathB)
    {
        if (ImGui::Button(buttonLabel))
        {
            if (!Utils::RunDiffTool(imagePathA, imagePathB))
            {
                m_messageBox.OpenPopupMessage("Can't Diff", "Image diff is not supported on this platform, or the required diff tool is not installed.");
            }
        }
    }

    const ImageComparisonToleranceLevel* ScriptReporter::FindBestToleranceLevel(float diffScore, bool filterImperceptibleDiffs) const
    {
        float thresholdChecked = 0.0f;
        bool ignoringMinorDiffs = false;
        for (const ImageComparisonToleranceLevel& level : m_availableToleranceLevels)
        {
            AZ_Assert(level.m_threshold > thresholdChecked || thresholdChecked == 0.0f, "Threshold values are not sequential");
            AZ_Assert(level.m_filterImperceptibleDiffs >= ignoringMinorDiffs, "filterImperceptibleDiffs values are not sequential");
            thresholdChecked = level.m_threshold;
            ignoringMinorDiffs = level.m_filterImperceptibleDiffs;

            if (filterImperceptibleDiffs <= level.m_filterImperceptibleDiffs && diffScore <= level.m_threshold)
            {
                return &level;
            }
        }

        return nullptr;
    }

    void ScriptReporter::ShowReportDialog()
    {
        if (ImGui::Begin("Script Results", &m_showReportDialog) && !m_scriptReports.empty())
        {
            const ImVec4& bgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            const bool isDarkStyle = bgColor.x < 0.2 && bgColor.y < 0.2 && bgColor.z < 0.2;
            const ImVec4 HighlightPassed = isDarkStyle ? ImVec4{0.5, 1, 0.5, 1} : ImVec4{0, 0.75, 0, 1};
            const ImVec4 HighlightFailed = isDarkStyle ? ImVec4{1, 0.5, 0.5, 1} : ImVec4{0.75, 0, 0, 1};
            const ImVec4 HighlightWarning = isDarkStyle ? ImVec4{1, 1, 0.5, 1} : ImVec4{0.5, 0.5, 0, 1};

            // Local utilities for setting text color
            bool colorHasBeenSet = false;
            auto highlightTextIf = [&colorHasBeenSet](bool shouldSet, ImVec4 color)
            {
                if (colorHasBeenSet)
                {
                    ImGui::PopStyleColor();
                    colorHasBeenSet = false;
                }

                if (shouldSet)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    colorHasBeenSet = true;
                }
            };
            auto highlightTextFailedOrWarning = [&](bool isFailed, bool isWarning)
            {
                if (colorHasBeenSet)
                {
                    ImGui::PopStyleColor();
                    colorHasBeenSet = false;
                }

                if (isFailed)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, HighlightFailed);
                    colorHasBeenSet = true;
                }
                else if (isWarning)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, HighlightWarning);
                    colorHasBeenSet = true;
                }
            };
            auto resetTextHighlight = [&colorHasBeenSet]()
            {
                if (colorHasBeenSet)
                {
                    ImGui::PopStyleColor();
                    colorHasBeenSet = false;
                }
            };

            auto seeConsole = [](uint32_t issueCount, const char* searchString)
            {
                if (issueCount == 0)
                {
                    return AZStd::string{};
                }
                else
                {
                    return AZStd::string::format("(See \"%s\" messages in console output)", searchString);
                }
            };

            auto seeBelow = [](uint32_t issueCount)
            {
                if (issueCount == 0)
                {
                    return AZStd::string{};
                }
                else
                {
                    return AZStd::string::format("(See below)");
                }
            };

            uint32_t totalAsserts = 0;
            uint32_t totalErrors = 0;
            uint32_t totalWarnings = 0;
            uint32_t totalScreenshotsCount = 0;
            uint32_t totalScreenshotsFailed = 0;
            uint32_t totalScreenshotWarnings = 0;
            for (ScriptReport& scriptReport : m_scriptReports)
            {
                totalAsserts += scriptReport.m_assertCount;

                // We don't include screenshot errors and warnings in these totals because those have their own line-items.
                totalErrors += scriptReport.m_generalErrorCount;
                totalWarnings += scriptReport.m_generalWarningCount;

                totalScreenshotWarnings += scriptReport.m_screenshotWarningCount;
                totalScreenshotsFailed += scriptReport.m_screenshotErrorCount;

                // This will catch any false-negatives that could occur if the screenshot failure error messages change without also updating ScriptReport::OnPreError()
                for (ScreenshotTestInfo& screenshotTest : scriptReport.m_screenshotTests)
                {
                    if (screenshotTest.m_officialComparisonResult.m_resultCode != ImageComparisonResult::ResultCode::Pass &&
                        screenshotTest.m_officialComparisonResult.m_resultCode != ImageComparisonResult::ResultCode::None)
                    {
                        AZ_Assert(scriptReport.m_screenshotErrorCount > 0, "If screenshot comparison failed in any way, m_screenshotErrorCount should be non-zero.");
                    }
                }
            }

            ImGui::Separator();

            if (HasActiveScript())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, HighlightWarning);
                ImGui::Text("Script is running... (_ _)zzz");
                ImGui::PopStyleColor();
            }
            else if (totalErrors > 0 || totalAsserts > 0 || totalScreenshotsFailed > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, HighlightFailed);
                ImGui::Text("(>_<)  FAILED  (>_<)");
                ImGui::PopStyleColor();
            }
            else
            {
                if (m_invalidationMessage.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, HighlightPassed);
                    ImGui::Text("\\(^_^)/  PASSED  \\(^_^)/");
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::Text("(-_-) INVALID ... but passed (-_-)");
                }
            }

            if (!m_invalidationMessage.empty())
            {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, HighlightFailed);
                ImGui::Text("(%s)", m_invalidationMessage.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            ImGui::Text("Test Script Count: %zu", m_scriptReports.size());

            highlightTextIf(totalAsserts > 0, HighlightFailed);
            ImGui::Text("Total Asserts:  %u %s", totalAsserts, seeConsole(totalAsserts, "Trace::Assert").c_str());

            highlightTextIf(totalErrors > 0, HighlightFailed);
            ImGui::Text("Total Errors:   %u %s", totalErrors, seeConsole(totalErrors, "Trace::Error").c_str());

            highlightTextIf(totalWarnings > 0, HighlightWarning);
            ImGui::Text("Total Warnings: %u %s", totalWarnings, seeConsole(totalWarnings, "Trace::Warning").c_str());

            resetTextHighlight();
            ImGui::Text("Total Screenshot Count: %u", totalScreenshotsCount);

            highlightTextIf(totalScreenshotsFailed > 0, HighlightFailed);
            ImGui::Text("Total Screenshot Failures: %u %s", totalScreenshotsFailed, seeBelow(totalScreenshotsFailed).c_str());

            highlightTextIf(totalScreenshotWarnings > 0, HighlightWarning);
            ImGui::Text("Total Screenshot Warnings: %u %s", totalScreenshotWarnings, seeBelow(totalScreenshotWarnings).c_str());

            resetTextHighlight();

            if (ImGui::Button("Update All Local Baseline Images"))
            {
                m_messageBox.OpenPopupConfirmation(
                    "Update All Local Baseline Images",
                    "This will replace all local baseline images \n"
                    "with the images captured during this test run. \n"
                    "Are you sure?",
                    [this]() {
                        UpdateAllLocalBaselineImages();
                    });
            }

            int displayOption = m_displayOption;
            ImGui::Combo("Display", &displayOption, DiplayOptions, AZ_ARRAY_SIZE(DiplayOptions));
            m_displayOption = (DisplayOption)displayOption;

            ImGui::Checkbox("Force Show 'Update' Buttons", &m_forceShowUpdateButtons);

            bool showWarnings = (m_displayOption == DisplayOption::AllResults) || (m_displayOption == DisplayOption::WarningsAndErrors);
            bool showAll = (m_displayOption == DisplayOption::AllResults);

            ImGui::Separator();

            const ImGuiTreeNodeFlags FlagDefaultOpen = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
            const ImGuiTreeNodeFlags FlagDefaultClosed = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

            for (ScriptReport& scriptReport : m_scriptReports)
            {
                const bool scriptPassed = scriptReport.m_assertCount == 0 && scriptReport.m_generalErrorCount == 0 && scriptReport.m_screenshotErrorCount == 0;
                const bool scriptHasWarnings = scriptReport.m_generalWarningCount > 0 || scriptReport.m_screenshotWarningCount > 0;

                // Skip if tests passed without warnings and we don't want to show successes
                bool skipReport = (scriptPassed && !scriptHasWarnings && !showAll);

                // Skip if we only have warnings only and we don't want to show warnings
                skipReport = skipReport || (scriptPassed && scriptHasWarnings && !showWarnings);

                if (skipReport)
                {
                    continue;
                }

                ImGuiTreeNodeFlags scriptNodeFlag = scriptPassed ? FlagDefaultClosed : FlagDefaultOpen;

                AZStd::string header = AZStd::string::format("%s %s",
                    scriptPassed ? "PASSED" : "FAILED",
                    scriptReport.m_scriptAssetPath.c_str()
                );

                highlightTextFailedOrWarning(!scriptPassed, scriptHasWarnings);

                if (ImGui::TreeNodeEx(&scriptReport, scriptNodeFlag, "%s", header.c_str()))
                {
                    resetTextHighlight();

                    // Number of Asserts
                    highlightTextIf(scriptReport.m_assertCount > 0, HighlightFailed);
                    if (showAll || scriptReport.m_assertCount > 0)
                    {
                        ImGui::Text("Asserts:  %u %s", scriptReport.m_assertCount, seeConsole(scriptReport.m_assertCount, "Trace::Assert").c_str());
                    }

                    // Number of Errors
                    highlightTextIf(scriptReport.m_generalErrorCount > 0, HighlightFailed);
                    if (showAll || scriptReport.m_generalErrorCount > 0)
                    {
                        ImGui::Text("Errors:   %u %s", scriptReport.m_generalErrorCount, seeConsole(scriptReport.m_generalErrorCount, "Trace::Error").c_str());
                    }

                    // Number of Warnings
                    highlightTextIf(scriptReport.m_generalWarningCount > 0, HighlightWarning);
                    if (showAll || (showWarnings && scriptReport.m_generalWarningCount > 0))
                    {
                        ImGui::Text("Warnings: %u %s", scriptReport.m_generalWarningCount, seeConsole(scriptReport.m_generalWarningCount, "Trace::Warning").c_str());
                    }

                    resetTextHighlight();

                    // Number of screenshots
                    if (showAll || scriptReport.m_screenshotErrorCount > 0 || (showWarnings && scriptReport.m_screenshotWarningCount > 0))
                    {
                        ImGui::Text("Screenshot Test Count: %zu", scriptReport.m_screenshotTests.size());
                    }

                    // Number of screenshot failures
                    highlightTextIf(scriptReport.m_screenshotErrorCount > 0, HighlightFailed);
                    if (showAll || scriptReport.m_screenshotErrorCount > 0)
                    {
                        ImGui::Text("Screenshot Tests Failed: %u %s", scriptReport.m_screenshotErrorCount, seeBelow(scriptReport.m_screenshotErrorCount).c_str());
                    }

                    // Number of screenshot warnings
                    highlightTextIf(scriptReport.m_screenshotWarningCount > 0, HighlightWarning);
                    if (showAll || (showWarnings && scriptReport.m_screenshotWarningCount > 0))
                    {
                        ImGui::Text("Screenshot Warnings:     %u %s", scriptReport.m_screenshotWarningCount, seeBelow(scriptReport.m_screenshotWarningCount).c_str());
                    }

                    resetTextHighlight();

                    for (ScreenshotTestInfo& screenshotResult : scriptReport.m_screenshotTests)
                    {
                        const bool screenshotPassed = screenshotResult.m_officialComparisonResult.m_resultCode == ImageComparisonResult::ResultCode::Pass;
                        const bool localBaselineWarning = screenshotResult.m_localComparisonResult.m_resultCode != ImageComparisonResult::ResultCode::Pass;

                        // Skip if tests passed without warnings and we don't want to show successes
                        bool skipScreenshot = (screenshotPassed && !localBaselineWarning && !showAll);

                        // Skip if we only have warnings only and we don't want to show warnings
                        skipScreenshot = skipScreenshot || (screenshotPassed && localBaselineWarning && !showWarnings);

                        if (skipScreenshot)
                        {
                            continue;
                        }

                        AZStd::string fileName;
                        AzFramework::StringFunc::Path::GetFullFileName(screenshotResult.m_screenshotFilePath.c_str(), fileName);

                        AZStd::string headerSummary;
                        if (!screenshotPassed)
                        {
                            headerSummary = "(" + screenshotResult.m_officialComparisonResult.GetSummaryString() + ") ";
                        }
                        if (localBaselineWarning)
                        {
                            headerSummary += "(Local Baseline Warning)";
                        }

                        ImGuiTreeNodeFlags screenshotNodeFlag = FlagDefaultClosed;
                        AZStd::string screenshotHeader = AZStd::string::format("%s %s %s", screenshotPassed ? "PASSED" : "FAILED", fileName.c_str(), headerSummary.c_str());

                        highlightTextFailedOrWarning(!screenshotPassed, localBaselineWarning);
                        if (ImGui::TreeNodeEx(&screenshotResult, screenshotNodeFlag, "%s", screenshotHeader.c_str()))
                        {
                            resetTextHighlight();

                            ImGui::Text(("Screenshot:        " + screenshotResult.m_screenshotFilePath).c_str());

                            ImGui::Spacing();

                            highlightTextIf(!screenshotPassed, HighlightFailed);

                            ImGui::Text(("Official Baseline: " + screenshotResult.m_officialBaselineScreenshotFilePath).c_str());

                            // Official Baseline Result
                            ImGui::Indent();
                            {
                                ImGui::Text(screenshotResult.m_officialComparisonResult.GetSummaryString().c_str());

                                if (screenshotResult.m_officialComparisonResult.m_resultCode == ImageComparisonResult::ResultCode::ThresholdExceeded ||
                                    screenshotResult.m_officialComparisonResult.m_resultCode == ImageComparisonResult::ResultCode::Pass)
                                {
                                    ImGui::Text("Used Tolerance: %s", screenshotResult.m_toleranceLevel.ToString().c_str());

                                    const ImageComparisonToleranceLevel* suggestedTolerance = ScriptReporter::FindBestToleranceLevel(
                                        screenshotResult.m_officialComparisonResult.m_finalDiffScore,
                                        screenshotResult.m_toleranceLevel.m_filterImperceptibleDiffs);

                                    if(suggestedTolerance)
                                    {
                                        ImGui::Text("Suggested Tolerance: %s", suggestedTolerance->ToString().c_str());
                                    }

                                    if (screenshotResult.m_toleranceLevel.m_filterImperceptibleDiffs)
                                    {
                                        // This gives an idea of what the tolerance level would be if the imperceptible diffs were not filtered out.
                                        const ImageComparisonToleranceLevel* unfilteredTolerance = ScriptReporter::FindBestToleranceLevel(
                                            screenshotResult.m_officialComparisonResult.m_standardDiffScore, false);

                                        ImGui::Text("(Unfiltered Diff Score: %f%s)",
                                            screenshotResult.m_officialComparisonResult.m_standardDiffScore,
                                            unfilteredTolerance ? AZStd::string::format(" ~ '%s'", unfilteredTolerance->m_name.c_str()).c_str() : "");
                                    }
                                }

                                resetTextHighlight();

                                ImGui::PushID("Official");
                                ShowDiffButton("View Diff", screenshotResult.m_officialBaselineScreenshotFilePath, screenshotResult.m_screenshotFilePath);
                                ImGui::PopID();

                                if ((!screenshotPassed || m_forceShowUpdateButtons) && ImGui::Button("Update##Official"))
                                {
                                    if (screenshotResult.m_localComparisonResult.m_resultCode == ImageComparisonResult::ResultCode::FileNotFound)
                                    {
                                        UpdateSourceBaselineImage(screenshotResult, true);
                                    }
                                    else
                                    {
                                        m_messageBox.OpenPopupConfirmation(
                                            "Update Official Baseline Image",
                                            "This will replace the official baseline image \n"
                                            "with the image captured during this test run. \n"
                                            "Are you sure?",
                                            // It's important to bind screenshotResult by reference because UpdateOfficialBaselineImage will update it
                                            [this, &screenshotResult]() {
                                                UpdateSourceBaselineImage(screenshotResult, true);
                                            });
                                    }
                                }
                            }
                            ImGui::Unindent();

                            ImGui::Spacing();

                            highlightTextIf(localBaselineWarning, HighlightWarning);

                            ImGui::Text(("Local Baseline:    " + screenshotResult.m_localBaselineScreenshotFilePath).c_str());

                            // Local Baseline Result
                            ImGui::Indent();
                            {
                                ImGui::Text(screenshotResult.m_localComparisonResult.GetSummaryString().c_str());

                                resetTextHighlight();

                                ImGui::PushID("Local");
                                ShowDiffButton("View Diff", screenshotResult.m_localBaselineScreenshotFilePath, screenshotResult.m_screenshotFilePath);
                                ImGui::PopID();

                                if ((localBaselineWarning || m_forceShowUpdateButtons) && ImGui::Button("Update##Local"))
                                {
                                    if (screenshotResult.m_localComparisonResult.m_resultCode == ImageComparisonResult::ResultCode::FileNotFound)
                                    {
                                        UpdateLocalBaselineImage(screenshotResult, true);
                                    }
                                    else
                                    {
                                        m_messageBox.OpenPopupConfirmation(
                                            "Update Local Baseline Image",
                                            "This will replace the local baseline image \n"
                                            "with the image captured during this test run. \n"
                                            "Are you sure?",
                                            // It's important to bind screenshotResult by reference because UpdateLocalBaselineImage will update it
                                            [this, &screenshotResult]() {
                                                UpdateLocalBaselineImage(screenshotResult, true);
                                            });
                                    }
                                }
                            }
                            ImGui::Unindent();

                            ImGui::Spacing();

                            resetTextHighlight();

                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }

                resetTextHighlight();
            }

            resetTextHighlight();

            // Repeat the m_invalidationMessage at the bottom as well, to make sure the user doesn't miss it.
            if (!m_invalidationMessage.empty())
            {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, HighlightFailed);
                ImGui::Text("(%s)", m_invalidationMessage.c_str());
                ImGui::PopStyleColor();
            }
        }

        m_messageBox.TickPopup();

        ImGui::End();
    }

    void ScriptReporter::OpenReportDialog()
    {
        m_showReportDialog = true;
    }

    ScriptReporter::ScriptReport* ScriptReporter::GetCurrentScriptReport()
    {
        if (!m_currentScriptIndexStack.empty())
        {
            return &m_scriptReports[m_currentScriptIndexStack.back()];
        }
        else
        {
            return nullptr;
        }
    }

    void ScriptReporter::ReportScriptError([[maybe_unused]] const AZStd::string& message)
    {
        AZ_Error("Automation", false, "Script: %s", message.c_str());
    }

    void ScriptReporter::ReportScriptWarning([[maybe_unused]] const AZStd::string& message)
    {
        AZ_Warning("Automation", false, "Script: %s", message.c_str());
    }

    void ScriptReporter::ReportScriptIssue(const AZStd::string& message, TraceLevel traceLevel)
    {
        switch (traceLevel)
        {
        case TraceLevel::Error:
            ReportScriptError(message);
            break;
        case TraceLevel::Warning:
            ReportScriptWarning(message);
            break;
        default:
            AZ_Assert(false, "Unhandled TraceLevel");
        }
    }

    void ScriptReporter::ReportScreenshotComparisonIssue(const AZStd::string& message, const AZStd::string& expectedImageFilePath, const AZStd::string& actualImageFilePath, TraceLevel traceLevel)
    {
        AZStd::string fullMessage = AZStd::string::format("%s\n    Expected: '%s'\n    Actual:   '%s'",
            message.c_str(),
            expectedImageFilePath.c_str(),
            actualImageFilePath.c_str());

        ReportScriptIssue(fullMessage, traceLevel);
    }

    bool ScriptReporter::DiffImages(ImageComparisonResult& imageComparisonResult, const AZStd::string& expectedImageFilePath, const AZStd::string& actualImageFilePath, TraceLevel traceLevel)
    {
        using namespace AZ::Utils;

        AZStd::vector<uint8_t> actualImageBuffer;
        AZ::RHI::Size actualImageSize;
        AZ::RHI::Format actualImageFormat;
        if (!LoadPngData(imageComparisonResult, actualImageFilePath, actualImageBuffer, actualImageSize, actualImageFormat, traceLevel))
        {
            return false;
        }

        AZStd::vector<uint8_t> expectedImageBuffer;
        AZ::RHI::Size expectedImageSize;
        AZ::RHI::Format expectedImageFormat;
        if (!LoadPngData(imageComparisonResult, expectedImageFilePath, expectedImageBuffer, expectedImageSize, expectedImageFormat, traceLevel))
        {
            return false;
        }

        float diffScore = 0.0f;
        float filteredDiffScore = 0.0f;

        static constexpr float ImperceptibleDiffFilter = 0.01;

        ImageDiffResultCode rmsResult = AZ::Utils::CalcImageDiffRms(
            actualImageBuffer, actualImageSize, actualImageFormat,
            expectedImageBuffer, expectedImageSize, expectedImageFormat,
            &diffScore,
            &filteredDiffScore,
            ImperceptibleDiffFilter);

        if (rmsResult != ImageDiffResultCode::Success)
        {
            if(rmsResult == ImageDiffResultCode::SizeMismatch)
            {
                ReportScreenshotComparisonIssue(AZStd::string::format("Screenshot check failed. Sizes don't match. Expected %u x %u but was %u x %u.",
                    expectedImageSize.m_width, expectedImageSize.m_height,
                    actualImageSize.m_width, actualImageSize.m_height),
                    expectedImageFilePath,
                    actualImageFilePath,
                    traceLevel);
                imageComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::WrongSize;
                return false;
            }
            else if (rmsResult == ImageDiffResultCode::FormatMismatch || rmsResult == ImageDiffResultCode::UnsupportedFormat)
            {
                ReportScreenshotComparisonIssue(AZStd::string::format("Screenshot check failed. Could not compare screenshots due to a format issue."),
                    expectedImageFilePath,
                    actualImageFilePath,
                    traceLevel);
                imageComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::WrongFormat;
                return false;
            }
        }

        imageComparisonResult.m_standardDiffScore = diffScore;
        imageComparisonResult.m_filteredDiffScore = filteredDiffScore;
        imageComparisonResult.m_finalDiffScore = diffScore; // Set the final score to the standard score just in case the filtered one is ignored
        return true;
    }

    void ScriptReporter::UpdateAllLocalBaselineImages()
    {
        int failureCount = 0;
        int successCount = 0;

        for (ScriptReport& report : m_scriptReports)
        {
            for (ScreenshotTestInfo& screenshotTest : report.m_screenshotTests)
            {
                if (UpdateLocalBaselineImage(screenshotTest, false))
                {
                    successCount++;
                }
                else
                {
                    failureCount++;
                }
            }
        }

        ShowUpdateLocalBaselineResult(successCount, failureCount);
    }
    
    bool ScriptReporter::UpdateLocalBaselineImage(ScreenshotTestInfo& screenshotTest, bool showResultDialog)
    {
        const AZStd::string destinationFile = ScreenshotPaths::GetLocalBaseline(screenshotTest.m_screenshotFilePath);

        AZStd::string destinationFolder = destinationFile;
        AzFramework::StringFunc::Path::StripFullName(destinationFolder);

        m_fileIoErrorHandler.BusConnect();

        bool failed = false;

        if (!AZ::IO::LocalFileIO::GetInstance()->CreatePath(destinationFolder.c_str()))
        {
            failed = true;
            m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to create folder '%s'.", destinationFolder.c_str()));
        }

        if (!AZ::IO::LocalFileIO::GetInstance()->Copy(screenshotTest.m_screenshotFilePath.c_str(), destinationFile.c_str()))
        {
            failed = true;
            m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to copy '%s' to '%s'.", screenshotTest.m_screenshotFilePath.c_str(), destinationFile.c_str()));
        }

        m_fileIoErrorHandler.BusDisconnect();

        if (!failed)
        {
            // Since we just replaced the baseline image, we can update this screenshot test result as an exact match.
            // This will update the ImGui report dialog by the next frame.
            ClearImageComparisonResult(screenshotTest.m_localComparisonResult);
        }

        if (showResultDialog)
        {
            int successCount = !failed;
            int failureCount = failed;
            ShowUpdateLocalBaselineResult(successCount, failureCount);
        }

        return !failed;
    }

    bool ScriptReporter::UpdateSourceBaselineImage(ScreenshotTestInfo& screenshotTest, bool showResultDialog)
    {
        bool success = true;
        auto io = AZ::IO::LocalFileIO::GetInstance();

        // Get source folder
        if (m_officialBaselineSourceFolder.empty())
        {
            m_officialBaselineSourceFolder = (AZ::IO::FixedMaxPath(AZ::Utils::GetProjectPath()) / "Scripts" / "ExpectedScreenshots").String();

            if (!io->Exists(m_officialBaselineSourceFolder.c_str()))
            {
                AZ_Error("Automation", false, "Could not find source folder '%s'. Copying to source baseline can only be used on dev platforms.", m_officialBaselineSourceFolder.c_str());
                m_officialBaselineSourceFolder.clear();
                success = false;
            }
        }

        // Get official cache baseline file
        const AZStd::string cacheFilePath = ScreenshotPaths::GetOfficialBaseline(screenshotTest.m_screenshotFilePath);

        // Divide cache file path into components to we can access the file name and the parent folder
        AZStd::fixed_vector<AZ::IO::FixedMaxPathString, 16> reversePathComponents;
        auto GatherPathSegments = [&reversePathComponents](AZStd::string_view token)
        {
            reversePathComponents.emplace_back(token);
        };
        AzFramework::StringFunc::TokenizeVisitorReverse(cacheFilePath, GatherPathSegments, "/\\");

        // Source folder path
        // ".../AtomSampleViewer/Scripts/ExpectedScreenshots/" + "MyTestFolder/"
        AZStd::string sourceFolderPath = AZStd::string::format("%s\\%s", m_officialBaselineSourceFolder.c_str(), reversePathComponents[1].c_str());

        // Source file path
        // ".../AtomSampleViewer/Scripts/ExpectedScreenshots/MyTestFolder/" + "MyTest.png"
        AZStd::string sourceFilePath = AZStd::string::format("%s\\%s", sourceFolderPath.c_str(), reversePathComponents[0].c_str());

        m_fileIoErrorHandler.BusConnect();

        // Create parent folder if it doesn't exist
        if (success && !io->CreatePath(sourceFolderPath.c_str()))
        {
            success = false;
            m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to create folder '%s'.", sourceFolderPath.c_str()));
        }

        // Replace source screenshot with new result
        if (success && !io->Copy(screenshotTest.m_screenshotFilePath.c_str(), sourceFilePath.c_str()))
        {
            success = false;
            m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to copy '%s' to '%s'.", screenshotTest.m_screenshotFilePath.c_str(), sourceFilePath.c_str()));
        }

        m_fileIoErrorHandler.BusDisconnect();

        if (success)
        {
            // Since we just replaced the baseline image, we can update this screenshot test result as an exact match.
            // This will update the ImGui report dialog by the next frame.
            ClearImageComparisonResult(screenshotTest.m_officialComparisonResult);
        }

        if (showResultDialog)
        {
            AZStd::string message = "Destination: " + sourceFilePath + "\n";
            message += success
                ? AZStd::string::format("Copy successful!.\n")
                : AZStd::string::format("Copy failed!\n");

            m_messageBox.OpenPopupMessage("Update Baseline Image(s) Result", message);
        }

        return success;
    }

    void ScriptReporter::ClearImageComparisonResult(ImageComparisonResult& comparisonResult)
    {
        comparisonResult.m_resultCode = ImageComparisonResult::ResultCode::Pass;
        comparisonResult.m_standardDiffScore = 0.0f;
        comparisonResult.m_filteredDiffScore = 0.0f;
        comparisonResult.m_finalDiffScore = 0.0f;
    }

    void ScriptReporter::ShowUpdateLocalBaselineResult(int successCount, int failureCount)
    {
        AZStd::string message;
        if (failureCount == 0 && successCount == 0)
        {
            message = "No screenshots found.";
        }
        else
        {
            message = "Destination: " + ScreenshotPaths::GetLocalBaselineFolder(true) + "\n";

            if (successCount > 0)
            {
                message += AZStd::string::format("Successfully copied %d files.\n", successCount);
            }
            if (failureCount > 0)
            {
                message += AZStd::string::format("Failed to copy %d files.\n", failureCount);
            }
        }

        m_messageBox.OpenPopupMessage("Update Baseline Image(s) Result", message);
    }

    void ScriptReporter::CheckLatestScreenshot(const ImageComparisonToleranceLevel* toleranceLevel)
    {
        AZ_Assert(GetCurrentScriptReport(), "There is no active script");

        if (GetCurrentScriptReport() == nullptr || GetCurrentScriptReport()->m_screenshotTests.empty())
        {
            ReportScriptError("CheckLatestScreenshot() did not find any screenshots to check.");
            return;
        }

        ScreenshotTestInfo& screenshotTestInfo = GetCurrentScriptReport()->m_screenshotTests.back();

        if (toleranceLevel == nullptr)
        {
            screenshotTestInfo.m_officialComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::NullImageComparisonToleranceLevel;
            ReportScriptError("Screenshot check failed. No ImageComparisonToleranceLevel provided.");
            return;
        }

        screenshotTestInfo.m_toleranceLevel = *toleranceLevel;

        screenshotTestInfo.m_officialBaselineScreenshotFilePath = ScreenshotPaths::GetOfficialBaseline(screenshotTestInfo.m_screenshotFilePath);
        if (screenshotTestInfo.m_officialBaselineScreenshotFilePath.empty())
        {
            ReportScriptError(AZStd::string::format("Screenshot check failed. Could not determine expected screenshot path for '%s'", screenshotTestInfo.m_screenshotFilePath.c_str()));
            screenshotTestInfo.m_officialComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::FileNotFound;
        }
        else
        {
            bool imagesWereCompared = DiffImages(
                screenshotTestInfo.m_officialComparisonResult,
                screenshotTestInfo.m_officialBaselineScreenshotFilePath,
                screenshotTestInfo.m_screenshotFilePath,
                TraceLevel::Error);

            if (imagesWereCompared)
            {
                screenshotTestInfo.m_officialComparisonResult.m_finalDiffScore = toleranceLevel->m_filterImperceptibleDiffs ?
                    screenshotTestInfo.m_officialComparisonResult.m_filteredDiffScore :
                    screenshotTestInfo.m_officialComparisonResult.m_standardDiffScore;

                if (screenshotTestInfo.m_officialComparisonResult.m_finalDiffScore <= toleranceLevel->m_threshold)
                {
                    screenshotTestInfo.m_officialComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::Pass;
                }
                else
                {
                    // Be aware there is an automation test script that looks for the "Screenshot check failed. Diff score" string text to report failures.
                    // If you change this message, be sure to update the associated tests as well located here: "C:/path/to/Lumberyard/AtomSampleViewer/Standalone/PythonTests"
                    ReportScreenshotComparisonIssue(
                        AZStd::string::format("Screenshot check failed. Diff score %f exceeds threshold of %f ('%s').",
                            screenshotTestInfo.m_officialComparisonResult.m_finalDiffScore, toleranceLevel->m_threshold, toleranceLevel->m_name.c_str()),
                        screenshotTestInfo.m_officialBaselineScreenshotFilePath,
                        screenshotTestInfo.m_screenshotFilePath,
                        TraceLevel::Error);
                    screenshotTestInfo.m_officialComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::ThresholdExceeded;
                }
            }
        }

        screenshotTestInfo.m_localBaselineScreenshotFilePath = ScreenshotPaths::GetLocalBaseline(screenshotTestInfo.m_screenshotFilePath);
        if (screenshotTestInfo.m_localBaselineScreenshotFilePath.empty())
        {
            ReportScriptWarning(AZStd::string::format("Screenshot check failed. Could not determine local baseline screenshot path for '%s'", screenshotTestInfo.m_screenshotFilePath.c_str()));
            screenshotTestInfo.m_localComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::FileNotFound;
        }
        else
        {
            // Local screenshots should be expected match 100% every time, otherwise warnings are reported. This will help developers track and investigate changes,
            // for example if they make local changes that impact some unrelated AtomSampleViewer sample in an unexpected way, they will see a warning about this.

            bool imagesWereCompared = DiffImages(
                screenshotTestInfo.m_localComparisonResult,
                screenshotTestInfo.m_localBaselineScreenshotFilePath,
                screenshotTestInfo.m_screenshotFilePath,
                TraceLevel::Warning);

            if (imagesWereCompared)
            {
                if(screenshotTestInfo.m_localComparisonResult.m_standardDiffScore == 0.0f)
                {
                    screenshotTestInfo.m_localComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::Pass;
                }
                else
                {
                    ReportScreenshotComparisonIssue(
                        AZStd::string::format("Screenshot check failed. Screenshot does not match the local baseline; something has changed. Diff score is %f.", screenshotTestInfo.m_localComparisonResult.m_standardDiffScore),
                        screenshotTestInfo.m_localBaselineScreenshotFilePath,
                        screenshotTestInfo.m_screenshotFilePath,
                        TraceLevel::Warning);
                    screenshotTestInfo.m_localComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::ThresholdExceeded;
                }
            }
        }

    }
} // namespace AtomSampleViewer
