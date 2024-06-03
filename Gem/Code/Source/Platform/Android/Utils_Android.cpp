/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Utils/Utils.h>

#include <AzCore/PlatformIncl.h>

namespace AtomSampleViewer
{
    namespace Utils
    {
        bool SupportsResizeClientArea()
        {
            return false;
        }

        AZStd::string GetDefaultDiffToolPath_Impl()
        {
            return AZStd::string("");
        }

        bool RunDiffTool_Impl(const AZStd::string& diffToolPath, const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            return false;
        }
    } // namespace Utils
} // namespace AtomSampleViewer
