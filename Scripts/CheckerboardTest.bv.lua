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

g_testCaseFolder = 'Checkerboard'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

OpenSample('Features/Checkerboard')
ResizeViewport(600, 600)

SelectImageComparisonToleranceLevel("Level F")

IdleFrames(10) 

CaptureScreenshot(g_testCaseFolder .. 'frame1.png')

OpenSample(nil)