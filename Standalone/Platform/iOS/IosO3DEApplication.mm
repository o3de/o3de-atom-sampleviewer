/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_ios.h>

#import <UIKit/UIKit.h>

//[GFX TODO][ATOM-449] - Remove this file once we switch to unified launcher

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface IosO3DEApplication : UIApplication
{
}
@end    // IosO3DEApplication Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation IosO3DEApplication

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesBegan: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (const UITouch* touch in touches)
    {
        EBUS_EVENT(AzFramework::RawInputNotificationBusIos, OnRawTouchEventBegan, touch);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesMoved: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (const UITouch* touch in touches)
    {
        EBUS_EVENT(AzFramework::RawInputNotificationBusIos, OnRawTouchEventMoved, touch);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesEnded: (NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    for (const UITouch* touch in touches)
    {
        EBUS_EVENT(AzFramework::RawInputNotificationBusIos, OnRawTouchEventEnded, touch);

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent: (UIEvent*)event
{
    // Active touches can be cancelled (as opposed to ended) for a variety of reasons, including:
    // - The active view being rotated to match the device orientation.
    // - The application resigning it's active status (eg. when receiving a message or phone call).
    // - Exceeding the max number of active touches tracked by the system (which as explained above
    //   is device dependent). For some reason this causes all active touches to be cancelled.
    // In any case, for the purposes of a game (or really any application that I can think of),
    // there really isn't any reason to distinguish between a touch ending or being cancelled.
    // They are mutually exclusive events, and both result in the touch being discarded by the
    // system.
    [self touchesEnded: touches withEvent: event];
}

@end 
