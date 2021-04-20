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

#include "AtomSampleViewerApplication.h"

#include <AzGameFramework/Application/GameApplication.h>

#include <GridMate/GridMate.h>
#include <GridMate/Session/LANSession.h>

#include "MacO3DEApplication.h"
#include <mach-o/dyld.h>
#include <libgen.h>

int main(int argc, char** argv)
{
    /*
     *  AtomSampleViewer executable is not getting it's current working directory set correctly on Mac.
     *  When launched from the Finder it's set to the active users folder.
     *  When launched in the terminal it's set to the terminal's working directory.
     *
     *  Fix is to get the path to the executable and set the current working directory to match it.
     */
    char path[1024];
    uint32_t size = sizeof(path);
    int result = _NSGetExecutablePath(path, &size);
    AZ_Assert(result == 0, "Mac path buffer too small; need size %u\n", size);
    char* dir = dirname(path);
    chdir(dir);

    // Ensure the process is a foreground application. Must be done before creating the application.
    ProcessSerialNumber processSerialNumber = { 0, kCurrentProcess };
    TransformProcessType(&processSerialNumber, kProcessTransformToForegroundApplication);
    
    // Create a custom AppKit application, and a custom AppKit application delegate.
    @autoreleasepool
    {
        [MacO3DEApplication sharedApplication];
        [NSApp setDelegate: [[MacO3DEApplicationDelegate alloc] init]];
        
        // Register some default application behaviours
        [[NSUserDefaults standardUserDefaults] registerDefaults:
         [[NSDictionary alloc] initWithObjectsAndKeys:
          [NSNumber numberWithBool: FALSE], @"AppleMomentumScrollSupported",
          [NSNumber numberWithBool: FALSE], @"ApplePressAndHoldEnabled",
          nil]];
        
        // Launch the AppKit application and release the memory pool.
        [NSApp finishLaunching];
    }
    
    // Launch the Open 3D Engine application.
    return AtomSampleViewer::RunGameCommon(argc, argv);
}
