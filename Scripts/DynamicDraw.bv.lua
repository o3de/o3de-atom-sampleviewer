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

g_testCaseFolder = 'DynamicDraw'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('RPI/DynamicDraw')
ResizeViewport(800, 500)

-- Vulkan's line width is different than dx12's 
SelectImageComparisonToleranceLevel("Level F") 

CaptureScreenshot(g_testCaseFolder .. '/screenshot_1.png')

OpenSample(nil)