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

RunScript("scripts/TestEnvironment.luac")

OpenSample('RPI/ShaderReloadTest')
ResizeViewport(500, 500)


function TestButton(buttonName)
    AssetTracking_Start()
    AssetTracking_ExpectAsset("shaders/shaderreloadtest/temp/fullscreen.shader")
    SetImguiValue(buttonName, true)
    AssetTracking_IdleUntilExpectedAssetsFinish(10)
    -- Even though the shader has been recompiled, it takes a few frames
    -- for the notification to bubble up and get the screen refreshed.
    IdleSeconds(0.25)

    SetShowImGui(false)
    SetImguiValue('Check color', true)

    -- Need a few frames to capture the screen and compare expected color.
    IdleSeconds(0.25)
    
    SetShowImGui(true)
end


-- Fixme. As a Soak Test, this should run for a long time and exit
-- on the first failure, or upon user request.
for i=1,5 do
    -- Always start with "Green shader" or "Blue shader" because when RPI/ShaderReloadTest
    -- activates it starts with the "Red shader", so updating the source asset
    --- to the exact same content won't trigger asset recompilation.
    TestButton("Green shader")
    TestButton("Blue shader")
    TestButton("Red shader")
end