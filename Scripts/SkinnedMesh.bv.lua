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

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/' .. g_testEnv .. '/SkinnedMesh/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

OpenSample('Features/SkinnedMesh')
ResizeViewport(1600, 900)
SelectImageComparisonToleranceLevel("Level B")

SetImguiValue('Sub-mesh count', 3)
SetImguiValue('Bones Per-Mesh', 78)
SetImguiValue('Vertices Per-Segment', 693)
SetImguiValue('Segments Per-Mesh', 65.000000)
SetImguiValue('Use Fixed Animation Time', true)
SetImguiValue('Fixed Animation Time', 4.751000)
SetImguiValue('Draw bones', false)

NoClipCameraController_SetPosition(Vector3(-0.125466, -2.129441, 1.728536))
NoClipCameraController_SetHeading(DegToRad(-8.116900))
NoClipCameraController_SetPitch(DegToRad(-31.035244))
CaptureScreenshot(g_screenshotOutputFolder .. '/screenshot_skinnedmesh.png')

OpenSample(nil)