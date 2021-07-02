----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/Checkerboard/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/Checkerboard')
ResizeViewport(600, 600)

SelectImageComparisonToleranceLevel("Level F")

IdleFrames(10) 

CaptureScreenshot(g_screenshotOutputFolder .. 'frame1.png')

OpenSample(nil)