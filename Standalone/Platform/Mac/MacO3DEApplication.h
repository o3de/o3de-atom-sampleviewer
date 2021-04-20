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
