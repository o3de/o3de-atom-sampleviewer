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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/StreamingImage/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('RPI/StreamingImage')
ResizeViewport(900, 900)

SelectImageComparisonToleranceLevel("Level H")

-- capture screenshot with all 2d images
CaptureScreenshot(g_screenshotOutputFolder .. 'Streaming2dImages.png')

-- capture screenshot for hot loading
SetImguiValue('Switch texture', true)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. 'HotReloading.png')


-- capture screenshot for 3d images
SetImguiValue('View 3D Images', true)
IdleFrames(1)
CaptureScreenshot(g_screenshotOutputFolder .. 'Streaming3dImage.png')


OpenSample(nil)