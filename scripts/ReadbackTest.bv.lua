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

g_testCaseFolder = 'Readback'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('RPI/Readback')

SelectImageComparisonToleranceLevel("Level H")
SetShowImGui(false)

-- First capture at 512x512
ResizeViewport(512, 512)
SetImguiValue('Width', 512)
SetImguiValue('Height', 512)
IdleFrames(3)
SetImguiValue('Readback', true)
IdleFrames(5)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_1.png')
IdleFrames(1)

-- Then at 1024x1024
ResizeViewport(1024, 1024)
SetImguiValue('Width', 1024)
SetImguiValue('Height', 1024)
IdleFrames(1)
SetImguiValue('Readback', true)
IdleFrames(5)
CaptureScreenshot(g_testCaseFolder .. '/screenshot_2.png')
IdleFrames(1)


SetShowImGui(true)
OpenSample(nil)

