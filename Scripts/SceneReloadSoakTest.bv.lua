----------------------------------------------------------------------------------------------------
--
-- All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
-- its licensors.
--
-- For complete copyright and license terms please see the LICENSE at the root of this
-- distribution (the "License"). All use of this software is governed by the License,
-- or, if provided, by the license below or the license accompanying this file. Do not
-- remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--
--
----------------------------------------------------------------------------------------------------

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/SceneReloadSoakTest/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

-- First we capture a screenshot to make sure everything is rendering correctly...
LockFrameTime(1/30) -- frame lock on to get a consistent result
OpenSample('RPI/SceneReloadSoakTest')
ResizeViewport(500, 500)
NoClipCameraController_SetFov(DegToRad(90))
IdleSeconds(1.0)
SelectImageComparisonToleranceLevel("Level G")
CaptureScreenshot(g_screenshotOutputFolder .. 'screenshot.ppm')

-- Unlock the frame time now that we have our screen capture, so the actual "soaking" can happen at a natural rate
UnlockFrameTime()

-- We let the soak test run for just a few seconds just to make sure things don't fail right away.
-- Note that a true test of the system would be to let the soak run for more like 5 minutes 
-- without failure, but that would be too long for our normal per-check-in test suite. We could do
-- that as part of a manual LKG test suite.
IdleSeconds(5.0)

OpenSample(nil)