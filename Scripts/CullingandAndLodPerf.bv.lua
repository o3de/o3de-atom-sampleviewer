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

g_performanceStatsOutputFolder = ResolvePath('@user@/Scripts/PerformanceStats/CullingAndLod/')
Print('Saving screenshots to ' .. NormalizePath(g_performanceStatsOutputFolder))

OpenSample('RPI/CullingAndLod')
ResizeViewport(800, 800)
IDLE_COUNT = 100
FRAME_COUNT = 100

CaptureBenchmarkMetadata('CullingAndLod', g_performanceStatsOutputFolder .. '/benchmark_metadata.json')
Print('Idling for ' .. tostring(IDLE_COUNT) .. ' frames..')
IdleFrames(IDLE_COUNT)
Print('Capturing timestamps for ' .. tostring(FRAME_COUNT) .. ' frames...')
for i = 1,FRAME_COUNT do
    frame_timestamps = g_performanceStatsOutputFolder .. '/frame' .. tostring(i) .. '_timestamps.json'
    CapturePassTimestamp(frame_timestamps)
end
Print('Capturing complete.')

OpenSample(nil)
