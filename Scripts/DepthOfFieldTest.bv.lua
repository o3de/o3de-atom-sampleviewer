----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

RunScript("scripts/TestEnvironment.luac")

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/DepthOfFieldTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/DepthOfField')
ResizeViewport(800, 600)
SelectImageComparisonToleranceLevel("Level D")

IdleSeconds(5)

CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_depth_of_field.png')

OpenSample(nil)
