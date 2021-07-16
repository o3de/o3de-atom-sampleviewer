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
SAMPLES_TO_RUN = {
    {prefix = 'RPI', name = 'CullingAndLod', width = 1400, height = 800},
    {prefix = 'RPI', name = 'SponzaBenchmark', width = 1400, height = 800}
}

Print('Capturing data for ' .. tostring(#SAMPLES_TO_RUN) .. ' benchmarks')
for index, sample in ipairs(SAMPLES_TO_RUN) do
    sample_path = sample['prefix'] .. '/' .. sample['name']
    Print('Opening sample ' .. sample_path)
    OpenSample(sample_path)
    ResizeViewport(sample['width'], sample['height'])

    output_path = g_baseFolder .. sample['name']
    CaptureBenchmarkMetadata(sample['name'], output_path .. '/benchmark_metadata.json')
    Print('Idling for ' .. tostring(IDLE_COUNT) .. ' frames..')
    IdleFrames(IDLE_COUNT)
    Print('Capturing timestamps for ' .. tostring(FRAME_COUNT) .. ' frames...')
    for i = 1,FRAME_COUNT do
        frame_timestamps = output_path .. '/frame' .. tostring(i) .. '_timestamps.json'
        CapturePassTimestamp(frame_timestamps)
    end
end

Print('Capturing complete.')
OpenSample(nil)
