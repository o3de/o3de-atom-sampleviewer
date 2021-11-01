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

-- WARNING: This is a soak test, do not add this test to the fully automated
-- test suites.

OpenSample('RPI/ShaderReloadSoakTest')
ResizeViewport(500, 500)
IdleSeconds(2)

function TestButton(buttonName, shaderReloadWaitSeconds, pixelCaptureWaitSeconds)
    SetImguiValue(buttonName, true)
    IdleSeconds(shaderReloadWaitSeconds) -- Idle for a bit to give time for the shader to reload
    SetShowImGui(false)
    SetImguiValue('Check color', true)
    IdleSeconds(pixelCaptureWaitSeconds)
    SetShowImGui(true)
end

local shaderReloadWaitSeconds = 3
local pixelCaptureWaitSeconds = 1


-- Fixme. As a Soak Test, this should run for a long time and exit
-- on the first failure, or upon user request.
for i=1,5 do
    TestButton("Red shader", shaderReloadWaitSeconds, pixelCaptureWaitSeconds)
    TestButton("Green shader", shaderReloadWaitSeconds, pixelCaptureWaitSeconds)
    TestButton("Blue shader", shaderReloadWaitSeconds, pixelCaptureWaitSeconds)
end