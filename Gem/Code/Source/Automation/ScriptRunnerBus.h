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
