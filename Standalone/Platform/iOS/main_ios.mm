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
                      @"IosLumberyardApplication",
                      @"IosLumberyardApplicationDelegate");
    }
}
