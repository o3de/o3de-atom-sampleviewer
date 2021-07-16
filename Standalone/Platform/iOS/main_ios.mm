/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AtomSampleViewerApplication.h"

#include <AzGameFramework/Application/GameApplication.h>

#include <GridMate/GridMate.h>
#include <GridMate/Session/LANSession.h>

#import <Foundation/Foundation.h>
#include <UIKit/UIKit.h>

int ios_main()
{
    return AtomSampleViewer::RunGameCommon(0, nullptr);
}

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        UIApplicationMain(argc,
                      argv,
                      @"IosO3DEApplication",
                      @"IosO3DEApplicationDelegate");
    }
}
