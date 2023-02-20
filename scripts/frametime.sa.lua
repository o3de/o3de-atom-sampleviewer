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
-- values provided by the runtime
local ProjectUserPath <const> = "/O3DE/Runtime/FilePaths/SourceProjectUserPath"

-- optional settings
local AssetLoadFrameIdleCountRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/AssetFrameIdleCount"
local FrameIdleCountRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/IdleCount"
local FrameCaptureCountRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/CaptureCount"
local ViewportWidthRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/ViewportWidth"
local ViewportHeightRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/ViewportHeight"
local ProfileOutputPathKey <const> = "/O3DE/ScriptAutomation/FrameTime/OutputPath"

-- required settings
local ProfileNameRegistryKey <const> = "/O3DE/ScriptAutomation/FrameTime/ProfileName"

IdleFrames(100) -- wait for assets to load into the level

function GetRequiredStringValue(valueKey, prettyName)
    value = g_SettingsRegistry:GetString(valueKey)
    if (not value:has_value()) then
        Print('FrameTime script missing ' .. tostring(prettyName) .. ' settings registry entry, ending script early')
        return false, nil
    end
    return true, value:value()
end

function GetOptionalUIntValue(valueKey, defaultValue)
    return g_SettingsRegistry:GetUInt(valueKey):value_or(defaultValue)
end
function GetOptionalStringValue(valueKey, defaultValue)
    return g_SettingsRegistry:GetString(valueKey):value_or(defaultValue)
end

-- default values
DEFAULT_ASSET_LOAD_FRAME_WAIT_COUNT = 100
DEFAULT_IDLE_COUNT = 100
DEFAULT_FRAME_COUNT = 100
DEFAULT_VIEWPORT_WIDTH = 800
DEFAULT_VIEWPORT_HEIGHT = 600
DEFAULT_OUTPUT_PATH = "ScriptAutomation/Profiling"

-- check for SettingsRegistry values that must exist
succeeded, userFolderPath = GetRequiredStringValue(ProjectUserPath, "Project User Path")
if (not succeeded) then return end
succeeded, profileName = GetRequiredStringValue(ProfileNameRegistryKey, "Profile Name")
if (not succeeded) then return end

-- read optional SettingsRegistry values
local assetLoadIdleFrameCount   = GetOptionalUIntValue(AssetLoadFrameIdleCountRegistryKey, DEFAULT_ASSET_LOAD_FRAME_WAIT_COUNT)
local frameIdleCount            = GetOptionalUIntValue(FrameIdleCountRegistryKey, DEFAULT_IDLE_COUNT)
local frameCaptureCount         = GetOptionalUIntValue(FrameCaptureCountRegistryKey, DEFAULT_FRAME_COUNT)
local viewportWidth             = GetOptionalUIntValue(ViewportWidthRegistryKey, DEFAULT_VIEWPORT_WIDTH)
local viewportHeight            = GetOptionalUIntValue(ViewportHeightRegistryKey, DEFAULT_VIEWPORT_HEIGHT)
local outputPath                = GetOptionalStringValue(ProfileOutputPathKey, DEFAULT_OUTPUT_PATH)

-- get the output folder path
g_profileOutputFolder = userFolderPath .. "/" .. tostring(outputPath) .. "/" .. tostring(profileName)
Print('Saving profile data to ' .. NormalizePath(g_profileOutputFolder))

-- Begin script execution
ResizeViewport(viewportWidth, viewportHeight)
ExecuteConsoleCommand("r_displayInfo=0")

CaptureBenchmarkMetadata(tostring(profileName), g_profileOutputFolder .. '/benchmark_metadata.json')
Print('Idling for ' .. tostring(frameIdleCount) .. ' frames..')
IdleFrames(frameIdleCount)
Print('Capturing timestamps for ' .. tostring(frameCaptureCount) .. ' frames...')
for i = 1,frameCaptureCount do
    cpu_timings = g_profileOutputFolder .. '/cpu_frame' .. tostring(i) .. '_time.json'
    CaptureCpuFrameTime(cpu_timings)
end
Print('Capturing complete.')
