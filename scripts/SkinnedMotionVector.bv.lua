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

g_testCaseFolder = 'SkinnedMotionVector'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

function TestDelayedCapture(atTime, screenshotFileName)
    SetImguiValue('Reset Clock', true)
    IdleSeconds(atTime)
    CaptureScreenshotWithPreview(g_testCaseFolder .. '/' .. screenshotFileName)
    IdleFrames(1)
end

OpenSample('Features/SkinnedMesh')
ResizeViewport(1920, 1080)
NoClipCameraController_SetPitch(DegToRad(-30))
SetImguiValue('Draw bones', false)
SelectImageComparisonToleranceLevel("Level E")

-- Show the pass tree tool and enable preview image attachment so we can capture the motion vector image
ShowTool('PassTree', true)
SetImguiValue('Show Pass Attachments', true)
SetImguiValue('Preview Attachment', true)
SetImguiValue('Color Range', Vector2(0.0, 0.01)) -- Use a constrained color range so small values will show up better
SetImguiValue('MeshMotionVectorPass/[InputOutput] [MotionInputOutput] [Image] CameraMotion [R16G16_FLOAT] [1920x1080]', true)
IdleFrames(1) -- Wait one frame to apply imgui change

-- We need to lock the frame time to get deterministic timing of the screenshots for consistency between runs
LockFrameTime(1/100)

SetShowImGui(false)

IdleFrames(1)

-- capture image attachment previews at different points in the animation
TestDelayedCapture(0.2, 'MotionVector1.png')
TestDelayedCapture(1.5, 'MotionVector2.png')
TestDelayedCapture(2.3, 'MotionVector3.png')

SetShowImGui(true)

-- Hide the pass tree tool
SetImguiValue('Color Range', Vector2(0.0, 1.0))
IdleFrames(1) -- Make sure Color Range is applied before hiding Preview Attachment
SetImguiValue('Preview Attachment', false)
SetImguiValue('Show Pass Attachments', false)
IdleFrames(1) -- Wait one frame to apply imgui change
ShowTool('PassTree', false)

UnlockFrameTime()

OpenSample(nil)