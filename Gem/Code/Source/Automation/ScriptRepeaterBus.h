/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/string/string_view.h>
#include <AzCore/EBus/EBus.h>

namespace AtomSampleViewer
{
    class ScriptRepeaterRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        //! Reports a snippet of script that can be run to repeat some action that was just done by the user.
        virtual void ReportScriptableAction(AZStd::string_view scriptCommand) = 0;
    };

    using ScriptRepeaterRequestBus = AZ::EBus<ScriptRepeaterRequests>;

} // namespace AtomSampleViewer
