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
#include <Utils/Utils.h>

#include <AzCore/PlatformIncl.h>
#include <Atom/RHI.Edit/Utils.h>
#include <iostream>
#include <errno.h>

namespace AtomSampleViewer
{
    namespace Utils
    {
        bool SupportsResizeClientArea()
        {
            return true;
        }

        bool RunDiffTool(const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            bool result = true;
            AZStd::string executablePath = "/usr/local/bin/bcompare";
           
            //Fork a process to run Beyond Compare app
            pid_t childPid = fork();
            
            if (childPid == 0)
            {
                //In child process
                char* args[] = { const_cast<char*>(executablePath.c_str()), const_cast<char*>(filePathA.c_str()), const_cast<char*>(filePathB.c_str()) };
                execv(executablePath.c_str(), args);
                
                AZ_TracePrintf("RunDiffTool", "RunDiffTool: Unable to launch Beyond Compare %s : errno = %s . Make sure you have installed Beyond Compare command line tools.", executablePath.c_str(), strerror(errno));
                std::cerr << strerror(errno) << std::endl;

                _exit(errno);
            }
            else if (childPid > 0)
            {
                //In parent process
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
