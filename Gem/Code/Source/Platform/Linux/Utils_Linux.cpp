/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Utils/Utils.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <AzCore/PlatformIncl.h>

namespace AtomSampleViewer
{
    namespace Utils
    {
        bool SupportsResizeClientArea()
        {
            return true;
        }

        AZStd::string GetDefaultDiffToolPath_Impl()
        {
            return AZStd::string("/usr/bin/bcompare");
        }

        bool RunDiffTool_Impl(const AZStd::string& diffToolPath, const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            bool result = true;

            // Fork a process to run Beyond Compare app
            pid_t childPid = fork();

            if (childPid == 0)
            {
                // In child process
                char* args[] = { const_cast<char*>(diffToolPath.c_str()), const_cast<char*>(filePathA.c_str()),
                                 const_cast<char*>(filePathB.c_str()), static_cast<char*>(0) };
                execv(diffToolPath.c_str(), args);

                AZ_Error(
                    "RunDiffTool", false,
                    "RunDiffTool: Unable to launch Diff Tool %s : errno = %s .",
                    diffToolPath.c_str(), strerror(errno));

                _exit(errno);
            }
            else if (childPid > 0)
            {
                // In parent process
                int status;
                pid_t result = waitpid(childPid, &status, WNOHANG);
                if (result == -1)
                {
                    result = false;
                }
            }
            else
            {
                result = false;
            }

            return result;
        }
    } // namespace Utils
} // namespace AtomSampleViewer
