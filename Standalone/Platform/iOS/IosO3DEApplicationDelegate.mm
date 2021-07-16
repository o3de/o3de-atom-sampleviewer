/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/API/ApplicationAPI_ios.h>
#import <UIKit/UIKit.h>

//[GFX TODO][ATOM-449] - Remove this file once we switch to unified launcher

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface IosO3DEApplicationDelegate : NSObject<UIApplicationDelegate>
{
}
@end    // IosO3DEApplicationDelegate Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation IosO3DEApplicationDelegate

extern int ios_main();
////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)launchO3DEApplication
{
    const int exitCode = ios_main();
    exit(exitCode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    [self performSelector:@selector(launchO3DEApplication) withObject:nil afterDelay:0.0];
    return YES;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillResignActive:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillResignActive);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidEnterBackground:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidEnterBackground);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillEnterForeground:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillEnterForeground);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidBecomeActive:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidBecomeActive);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillTerminate:(UIApplication *)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillTerminate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidReceiveMemoryWarning);
}

@end 
