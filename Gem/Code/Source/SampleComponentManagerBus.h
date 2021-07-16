/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

namespace AtomSampleViewer
{
    class SampleComponentManagerRequests
        : public AZ::EBusTraits
    {
    public:
        //! Close any open sample and return to the home screen
        virtual void Reset() = 0;

        //! Open a sample with the given name. Return false if the sample wasn't found.
        virtual bool OpenSample(const AZStd::string& sampleName) = 0;

        //! Show or hide a debug tool with the given name. Return false if the tool wasn't found.
        virtual bool ShowTool(const AZStd::string& toolName, bool enable) = 0;

        //! Take a screenshot and save to the indicated file path. 
        //! @param filePath where to save the screenshot file. The screenshot type is indicated by the extension.
        //! @param hideImGui hide ImGui while taking the screenshot.
        virtual void RequestFrameCapture(const AZStd::string& filePath, bool hideImGui) = 0;

        //! Returns true if a frame capture has been requested and not yet fulfilled
        virtual bool IsFrameCapturePending() = 0;

        //! Runs the main test suite, in an automated mode and closes AtomSampleViewer when the suite finished running
        //! @param suiteFilePath path to the compiled luac test script
        //! @param exitOnTestEnd if true, exits AtomSampleViewerStandalone when the script finishes, used in jenkins
        //! @param randomSeed the seed for the random generator, frequently used inside lua tests to shuffle the order of the test execution
        virtual void RunMainTestSuite(const AZStd::string& suiteFilePath, bool exitOnTestEnd, int randomSeed) = 0;

        //! Reset the RPI scene while keeping the current sample running
        virtual void ResetRPIScene() = 0;

        //! Clear the RPI scene
        virtual void ClearRPIScene() = 0;
    };
    using SampleComponentManagerRequestBus = AZ::EBus<SampleComponentManagerRequests>;

    class SampleComponentManagerNotifications
        : public AZ::EBusTraits
    {
    public:
        //! Called when SampleComponentManager has been activated
        virtual void OnSampleManagerActivated() {}
    };
    using SampleComponentManagerNotificationBus = AZ::EBus<SampleComponentManagerNotifications>;

} // namespace AtomSampleViewer
