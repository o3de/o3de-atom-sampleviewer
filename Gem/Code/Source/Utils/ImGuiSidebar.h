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

#include <AzFramework/Windowing/WindowBus.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    class ImGuiSidebar
    {
    public:
        static void Reflect(AZ::ReflectContext* context);

        ImGuiSidebar() = default;

        /// @param configFilePath - Path to a local json file for maintaining state between runs. Should start with "@user@/"
        explicit ImGuiSidebar(AZStd::string_view configFilePath);

        void Activate();
        void Deactivate();

        void SetHideSidebar(bool isHidden);

        bool Begin();
        void End();

    private:

        struct ConfigFile final
        {
            AZ_TYPE_INFO(ImGuiSidebar::ConfigFile, "{8B5F54D4-1DE8-4476-AE2F-80B2EE02E0A1}");
            AZ_CLASS_ALLOCATOR(ConfigFile, AZ::SystemAllocator, 0);

            static void Reflect(AZ::ReflectContext* context);

            float m_width = 300.0f;
        };

        AzFramework::WindowSize BeginFrame();
        void EndFrame();

        bool LoadConfigFile();
        void SaveConfigFile();

        static const float s_widthMin;
        static const float s_widthMax;
        static const float s_widthStepSmall;
        static const float s_widthStepBig;

        bool m_hideSidebar = false;
        bool m_isSidebarReady = false;

        AZStd::string m_configFilePath;
        ConfigFile m_configFile;
    };
} // namespace AtomSampleViewer
