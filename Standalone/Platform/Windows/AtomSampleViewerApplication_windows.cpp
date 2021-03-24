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

#include <AtomSampleViewerApplication.h>

#include <AzCore/PlatformIncl.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Debug/Trace.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>

namespace AtomSampleViewer
{
    const bool AtomSampleViewerApplication::s_connectToAssetProcessor = true;

    BOOL CTRL_BREAK_HandlerRoutine(DWORD /*dwCtrlType*/)
    {
        AZ::ComponentApplication* app = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(app, &AZ::ComponentApplicationBus::Events::GetApplication);
        if (app)
        {
            app->Destroy();
        }
        return TRUE;
    }

    void AtomSampleViewerApplication::SetupConsoleHandlerRoutine()
    {
        // if we're in console mode, listen for events that close it.
        ::SetConsoleCtrlHandler(CTRL_BREAK_HandlerRoutine, true);
    }
} // namespace AtomSampleViewer
