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

g_testCaseFolder = 'CullingAndLod'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('RPI/CullingAndLod')
ResizeViewport(500, 500)
SelectImageComparisonToleranceLevel("Level G")


SetImguiValue("Begin Verification", true)
IdleFrames(5)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_1.png')
IdleFrames(1)
SetImguiValue("End Verification", true)
IdleFrames(1) -- make sure all outstanding imgui comamnds are processed before closing sample.

OpenSample(nil)