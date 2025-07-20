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

        AZStd::string GetDefaultDiffToolPath_Impl()
        {
            return AZStd::string("C:\\Program Files\\Beyond Compare 4\\BCompare.exe");
        }

        bool RunDiffTool_Impl(const AZStd::string& diffToolPath, const AZStd::string& filePathA, const AZStd::string& filePathB)
        {
            bool result = false;

            AZStd::wstring exeW;
            AZStd::to_wstring(exeW, diffToolPath.c_str());
            AZStd::wstring filePathAW;
            AZStd::to_wstring(filePathAW, filePathA.c_str());
            AZStd::wstring filePathBW;
            AZStd::to_wstring(filePathBW, filePathB.c_str());
            AZStd::wstring commandLineW = L"\"" + exeW + L"\" \"" + filePathAW + L"\" \"" + filePathBW + L"\"";

            STARTUPINFOW si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            // start the program up
            result = CreateProcessW(exeW.data(),   // the path
                commandLineW.data(),        // Command line
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
