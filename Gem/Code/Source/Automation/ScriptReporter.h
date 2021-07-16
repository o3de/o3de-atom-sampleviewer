/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Debug/TraceMessageBus.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <Atom/Utils/ImageComparison.h>
#include <Automation/ImageComparisonConfig.h>
#include <Utils/ImGuiMessageBox.h>
#include <Utils/FileIOErrorHandler.h>

namespace AtomSampleViewer
{
    struct ImageComparisonToleranceLevel;

    namespace ScreenshotPaths
    {
        //! Returns the path to the screenshots capture folder.
        //! @resolvePath indicates whether to call ResolvePath() which will produce a full path, or keep the shorter asset folder path.
        AZStd::string GetScreenshotsFolder(bool resolvePath);

        //! Returns the path to the local baseline folder, which stores copies of screenshots previously taken on this machine.
        //! @resolvePath indicates whether to call ResolvePath() which will produce a full path, or keep the shorter asset folder path.
        AZStd::string GetLocalBaselineFolder(bool resolvePath);

        //! Returns the path to the official baseline folder, which stores copies of expected screenshots saved in source control.
        //! @resolvePath indicates whether to call ResolvePath() which will produce a full path, or keep the shorter asset folder path.
        AZStd::string GetOfficialBaselineFolder(bool resolvePath);

        //! Returns the path to the local baseline image that corresponds to @forScreenshotFile
        AZStd::string GetLocalBaseline(const AZStd::string& forScreenshotFile);

        //! Returns the path to the official baseline image that corresponds to @forScreenshotFile
        AZStd::string GetOfficialBaseline(const AZStd::string& forScreenshotFile);
    }

    //! Collects data about each script run by the ScriptManager.
    //! This includes counting errors, checking screenshots, and providing a final report dialog.
    class ScriptReporter
    {
    public:

        //! Set the list of available tolerance levels, so the report can suggest an alternate level that matches the actual results.
        void SetAvailableToleranceLevels(const AZStd::vector<ImageComparisonToleranceLevel>& toleranceLevels);

        //! Clears all recorded data.
        void Reset();

        //! Invalidates the final results when displaying a report to the user. This can be used to highlight
        //! local changes that were made, and remind the user that these results should not be considered official.
        //! Use an empty string to clear the invalidation.
        void SetInvalidationMessage(const AZStd::string& message);

        //! Indicates that a new script has started processing.
        //! Any subsequent errors will be included as part of this script's report.
        void PushScript(const AZStd::string& scriptAssetPath);

        //! Indicates that the current script has finished executing.
        //! Any subsequent errors will be included as part of the prior script's report.
        void PopScript();

        //! Returns whether there are active processing scripts (i.e. more PushScript() calls than PopScript() calls)
        bool HasActiveScript() const;

        //! Indicates that a new screenshot is about to be captured.
        bool AddScreenshotTest(const AZStd::string& path);

        //! Check the latest screenshot using default thresholds.
        void CheckLatestScreenshot(const ImageComparisonToleranceLevel* comparisonPreset);

        //! Opens the script report dialog.
        //! This displays all the collected script reporting data, provides links to tools for analyzing data like
        //! viewing screenshot diffs. It can be left open during processing and will update in real-time.
        void OpenReportDialog();

        //! Called every frame to update the ImGui dialog
        void TickImGui();

        //! Returns true if there are any errors or asserts in the script report
        bool HasErrorsAssertsInReport() const;

        struct ImageComparisonResult
        {
            enum class ResultCode
            {
                None,
                Pass,
                FileNotFound,
                FileNotLoaded,
                WrongSize,
                WrongFormat,
                NullImageComparisonToleranceLevel,
                ThresholdExceeded
            };

            ResultCode m_resultCode = ResultCode::None;
            float m_standardDiffScore = 0.0f;
            float m_filteredDiffScore = 0.0f; //!< The diff score after filtering out visually imperceptible differences.
            float m_finalDiffScore = 0.0f; //! The diff score that was used for comparison. May be m_diffScore or m_filteredDiffScore.

            AZStd::string GetSummaryString() const;
        };

        //! Records all the information about a screenshot comparison test.
        struct ScreenshotTestInfo
        {
            AZStd::string m_screenshotFilePath;
            AZStd::string m_officialBaselineScreenshotFilePath; //!< The path to the official baseline image that is checked into source control
            AZStd::string m_localBaselineScreenshotFilePath;    //!< The path to a local baseline image that was established by the user
            ImageComparisonToleranceLevel m_toleranceLevel;     //!< Tolerance for checking against the official baseline image
            ImageComparisonResult m_officialComparisonResult;   //!< Result of comparing against the official baseline image, for reporting test failure
            ImageComparisonResult m_localComparisonResult;      //!< Result of comparing against a local baseline, for reporting warnings
        };

        //! Records all the information about a single test script.
        struct ScriptReport : public AZ::Debug::TraceMessageBus::Handler
        {
            ~ScriptReport()
            {
                AZ::Debug::TraceMessageBus::Handler::BusDisconnect();
            }

            bool OnPreAssert(const char* /*fileName*/, int /*line*/, const char* /*func*/, [[maybe_unused]] const char* message) override
            {
                ++m_assertCount;
                return false;
            }

            bool OnPreError(const char* /*window*/, const char* /*fileName*/, int /*line*/, const char* /*func*/, const char* message) override
            {
                if (AZStd::string::npos == AzFramework::StringFunc::Find(message, "Screenshot check failed"))
                {
                    ++m_generalErrorCount;
                }
                else
                {
                    ++m_screenshotErrorCount;
                }

                return false;
            }

            bool OnPreWarning(const char* /*window*/, const char* /*fileName*/, int /*line*/, const char* /*func*/, const char* message) override
            {
                if (AZStd::string::npos == AzFramework::StringFunc::Find(message, "Screenshot does not match the local baseline"))
                {
                    ++m_generalWarningCount;
                }
                else
                {
                    ++m_screenshotWarningCount;
                }

                return false;
            }

            AZStd::string m_scriptAssetPath;

            uint32_t m_assertCount = 0;

            uint32_t m_generalErrorCount = 0;
            uint32_t m_screenshotErrorCount = 0;

            uint32_t m_generalWarningCount = 0;
            uint32_t m_screenshotWarningCount = 0;

            AZStd::vector<ScreenshotTestInfo> m_screenshotTests;
        };
        
        const AZStd::vector<ScriptReport>& GetScriptReport() const { return m_scriptReports; }

    private:

        // Reports a script error using standard formatting that matches ScriptManager
        enum class TraceLevel
        {
            Error,
            Warning
        };

        // Controls which results are shown to the user
        // Must match static const char* DiplayOptions in .cpp file
        enum DisplayOption : int
        {
            AllResults,
            WarningsAndErrors,
            ErrorsOnly
        };

        static void ReportScriptError(const AZStd::string& message);
        static void ReportScriptWarning(const AZStd::string& message);
        static void ReportScriptIssue(const AZStd::string& message, TraceLevel traceLevel);
        static void ReportScreenshotComparisonIssue(const AZStd::string& message, const AZStd::string& expectedImageFilePath, const AZStd::string& actualImageFilePath, TraceLevel traceLevel);

        // Loads image data from a .png file.
        // @param imageComparisonResult will be set to an error code if the function fails
        // @param path the path the .png file
        // @param buffer will be filled with the raw image data from the .png file
        // @param size will be set to the image size of the .png file
        // @param format will be set to the pixel format of the .png file
        // @return true if the file was loaded successfully
        static bool LoadPngData(ImageComparisonResult& imageComparisonResult, const AZStd::string& path, AZStd::vector<uint8_t>& buffer, AZ::RHI::Size& size, AZ::RHI::Format& format, TraceLevel traceLevel);

        // Compares two image files and updates the ImageComparisonResult accordingly.
        // Returns false if an error prevented the comparison.
        static bool DiffImages(ImageComparisonResult& imageComparisonResult, const AZStd::string& expectedImageFilePath, const AZStd::string& actualImageFilePath, TraceLevel traceLevel);

        // Copies all captured screenshots to the local baseline folder. These can be used as an alternative to the central baseline for comparison.
        void UpdateAllLocalBaselineImages();

        // Copies a single captured screenshot to the local baseline folder. This can be used as an alternative to the central baseline for comparison.
        bool UpdateLocalBaselineImage(ScreenshotTestInfo& screenshotTest, bool showResultDialog);

        // Copies a single captured screenshot to the official baseline source folder.
        bool UpdateSourceBaselineImage(ScreenshotTestInfo& screenshotTest, bool showResultDialog);

        // Clears comparison result to passing with no errors or warnings
        void ClearImageComparisonResult(ImageComparisonResult& comparisonResult);

        // Show a message box to let the user know the results of updating local baseline images
        void ShowUpdateLocalBaselineResult(int successCount, int failureCount);

        const ImageComparisonToleranceLevel* FindBestToleranceLevel(float diffScore, bool filterImperceptibleDiffs) const;

        void ShowReportDialog();

        void ShowDiffButton(const char* buttonLabel, const AZStd::string& imagePathA, const AZStd::string& imagePathB);

        ScriptReport* GetCurrentScriptReport();

        ImGuiMessageBox m_messageBox;
        FileIOErrorHandler m_fileIoErrorHandler;

        AZStd::vector<ImageComparisonToleranceLevel> m_availableToleranceLevels;
        AZStd::string m_invalidationMessage;

        AZStd::vector<ScriptReport> m_scriptReports; //< Tracks errors for the current active script
        AZStd::vector<size_t> m_currentScriptIndexStack; //< Tracks which of the scripts in m_scriptReports is currently active
        bool m_showReportDialog = false;
        DisplayOption m_displayOption = DisplayOption::AllResults;
        bool m_forceShowUpdateButtons = false; //< By default, the "Update" buttons are visible only for failed screenshots. This forces them to be visible.
        AZStd::string m_officialBaselineSourceFolder; //< Used for updating official baseline screenshots
    };

} // namespace AtomSampleViewer
