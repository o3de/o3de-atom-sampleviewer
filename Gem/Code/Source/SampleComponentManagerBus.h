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

namespace AZ
{
    class ComponentDescriptor;
}

namespace AtomSampleViewer
{
    enum class SamplePipelineType : uint32_t
    {
        RHI = 0,
        RPI
    };

    class SampleEntry
    {
    public:
        AZStd::string m_parentMenuName;
        AZStd::string m_sampleName;
        // m_parentMenuName/m_sampleName
        AZStd::string m_fullName;
        AZ::Uuid m_sampleUuid;
        AZStd::function<bool()> m_isSupportedFunc;
        SamplePipelineType m_pipelineType = SamplePipelineType::RHI;
        AZ::ComponentDescriptor* m_componentDescriptor;
        AZStd::string m_contentWarning;
        AZStd::string m_contentWarningTitle;

        bool operator==(const SampleEntry& other)
        {
            return other.m_sampleName == m_sampleName && other.m_parentMenuName == m_parentMenuName;
        }
    };

    template <typename T>
    static SampleEntry NewSample(SamplePipelineType type, const char* menuName, const AZStd::string& name)
    {
        SampleEntry entry;
        entry.m_sampleName = name;
        entry.m_sampleUuid = azrtti_typeid<T>();
        entry.m_pipelineType = type;
        entry.m_componentDescriptor = T::CreateDescriptor();
        entry.m_parentMenuName = menuName;
        entry.m_fullName = entry.m_parentMenuName + '/' + entry.m_sampleName;
        entry.m_contentWarning = T::ContentWarning;
        entry.m_contentWarningTitle = T::ContentWarningTitle;

        return entry;
    }

    template <typename T>
    static SampleEntry NewSample(SamplePipelineType type, const char* menuName, const AZStd::string& name, AZStd::function<bool()> isSupportedFunction)
    {
        SampleEntry entry = NewSample<T>(type, menuName, name);
        entry.m_isSupportedFunc = isSupportedFunction;
        return entry;
    }

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

        //! Set the number of MSAA samples
        //! @param numMSAASamples the number of MSAA samples
        virtual void SetNumMSAASamples(int16_t numMsaaSamples) = 0;

        //! Gets the number of MSAA samples
        virtual int16_t GetNumMSAASamples() = 0;

        //! Sets the default number of MSAA samples
        virtual void SetDefaultNumMSAASamples(int16_t defaultNumMsaaSamples) = 0;

        //! Gets the default number of MSAA samples
        virtual int16_t GetDefaultNumMSAASamples() = 0;

        //! Set the number of MSAA samples to the platform default
        virtual void ResetNumMSAASamples() = 0;

        //! Reset the RPI scene while keeping the current sample running
        virtual void ResetRPIScene() = 0;

        //! Clear the RPI scene
        virtual void ClearRPIScene() = 0;

        //! Enables or disables the default render pipeline.
        virtual void EnableRenderPipeline(bool value) = 0;

        //! Enables or disables the XR pipelines.
        virtual void EnableXrPipelines(bool value) = 0;
    };
    using SampleComponentManagerRequestBus = AZ::EBus<SampleComponentManagerRequests>;

    class ScriptManager;
    class ScriptableImGui;

    class SampleComponentSingletonRequests
        : public AZ::EBusTraits
    {
    public:

        virtual ScriptManager* GetScriptManagerInstance() = 0;
        virtual ScriptableImGui* GetScriptableImGuiInstance() = 0;
        virtual void RegisterSampleComponent(const SampleEntry& sample) = 0;
    };
    using SampleComponentSingletonRequestBus = AZ::EBus<SampleComponentSingletonRequests>;


    class SampleComponentManagerNotifications
        : public AZ::EBusTraits
    {
    public:
        //! Called when SampleComponentManager has been activated
        virtual void OnSampleManagerActivated() {}
    };
    using SampleComponentManagerNotificationBus = AZ::EBus<SampleComponentManagerNotifications>;

} // namespace AtomSampleViewer
