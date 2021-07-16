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
    class ScriptRunnerRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        //! Can be used by sample components to temporarily pause script processing, for example
        //! to delay until some required resources are loaded and initialized.
        virtual void PauseScript() = 0;
        virtual void PauseScriptWithTimeout(float timeout) = 0;
        virtual void ResumeScript() = 0;
    };

    using ScriptRunnerRequestBus = AZ::EBus<ScriptRunnerRequests>;

} // namespace AtomSampleViewer
