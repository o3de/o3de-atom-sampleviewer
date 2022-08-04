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
g_baseFolder = ResolvePath('@user@/Scripts/PerformanceBenchmarks/')
IDLE_COUNT = 100
FRAME_COUNT = 100

sample_path = 'Performance/100KDraw_10KDrawable_MultiView'
Print('Opening sample ' .. sample_path)
--OpenSample(sample_path)
ResizeViewport(800, 600)

output_path = g_baseFolder .. '100KDraw_10KDrawable_MultiView'
CaptureBenchmarkMetadata('100KDraw_10KDrawable_MultiView', output_path .. '/benchmark_metadata.json')
Print('Idling for ' .. tostring(IDLE_COUNT) .. ' frames..')
IdleFrames(IDLE_COUNT)
Print('Capturing timestamps for ' .. tostring(FRAME_COUNT) .. ' frames...')
for i = 1,FRAME_COUNT do
    frame_timestamps = output_path .. '/frame' .. tostring(i) .. '_timestamps.json'
    cpu_timings = output_path .. '/cpu_frame' .. tostring(i) .. '_time.json'
    CapturePassTimestamp(frame_timestamps)
    CaptureCpuFrameTime(cpu_timings)
end

Print('Capturing complete.')
--OpenSample(nil)
