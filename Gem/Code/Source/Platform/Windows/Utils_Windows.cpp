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
            return true;
        }

        bool RunDiffTool(const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            bool result = false;

            AZStd::string exe = "C:\\Program Files\\Beyond Compare 4\\BCompare.exe";
            AZStd::string commandLine = "\"" + exe + "\" \"" + filePathA + "\" \"" + filePathB + "\"";

            STARTUPINFO si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            // start the program up
            result = CreateProcess(exe.data(),   // the path
                commandLine.data(),        // Command line
                NULL,           // Process handle not inheritable
                NULL,           // Thread handle not inheritable
                FALSE,          // Set handle inheritance to FALSE
                0,              // No creation flags
                NULL,           // Use parent's environment block
                NULL,           // Use parent's starting directory 
                &si,            // Pointer to STARTUPINFO structure
                &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
            );
            // Close process and thread handles. 
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return result;
        }
    } // namespace Utils
} // namespace AtomSampleViewer
