/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
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
