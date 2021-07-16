/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Automation/ScriptReporter.h>
#include <AzCore/Utils/Utils.h>

namespace AtomSampleViewer
{
    bool ScriptReporter::LoadPngData([[maybe_unused]] ImageComparisonResult& imageComparisonResult, [[maybe_unused]] const AZStd::string& path, [[maybe_unused]] AZStd::vector<uint8_t>& buffer, [[maybe_unused]] AZ::RHI::Size& size, [[maybe_unused]] AZ::RHI::Format& format, [[maybe_unused]] TraceLevel traceLevel)
    {
        return false;
    }
}