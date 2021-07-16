/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AppKit/NSApplication.h>

//[GFX TODO][ATOM-449] - Remove this file once we switch to unified launcher

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface MacO3DEApplication : NSApplication
{
}
@end    // MacO3DEApplication Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface MacO3DEApplicationDelegate : NSObject<NSApplicationDelegate>
{
}
@end    // MacO3DEApplicationDelegate Interface
