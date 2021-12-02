"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""
import logging
import os
import subprocess

import pytest

import ly_test_tools.environment.process_utils as process_utils
import ly_test_tools.launchers.platforms.base
from ly_test_tools.benchmark.data_aggregator import BenchmarkDataAggregator

logger = logging.getLogger(__name__)


class AtomSampleViewerException(Exception):
    """Custom Exception class for AtomSampleViewer tests."""
    pass


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.parametrize('rhi', ['dx12', 'vulkan'])
@pytest.mark.usefixtures("clean_atomsampleviewer_logs", "atomsampleviewer_log_monitor")
class TestPerformanceBenchmarksPeriodicSuite:
    def test_PerformanceBenchmarkPeriodicSuite(self, request, workspace, launcher_platform, rhi, atomsampleviewer_log_monitor):
        # Script call setup.
        benchmark_script = '_AutomatedPeriodicBenchmarkSuite_.bv.lua'
        benchmark_script_path = os.path.join(workspace.paths.project(), 'Scripts', benchmark_script)
        if not os.path.exists(benchmark_script_path):
            raise AtomSampleViewerException(f'Test script does not exist in path: {benchmark_script_path}')
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
                           f'--project-path={workspace.paths.project()} '
                           f'--rhi {rhi} '
                           f'--runtestsuite scripts/{benchmark_script} '
                           '--exitontestend')

        def teardown():
            process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)
        request.addfinalizer(teardown)

        # Execute test.
        process_utils.safe_check_call(cmd, stderr=subprocess.STDOUT, encoding='UTF-8', shell=True)
        try:
            expected_lines = ["Script: Capturing complete."]
            atomsampleviewer_log_monitor.monitor_log_for_lines(expected_lines, timeout=180)

            aggregator = BenchmarkDataAggregator(workspace, logger, 'periodic')
            aggregator.upload_metrics(rhi)
        except ly_test_tools.log.log_monitor.LogMonitorException as e:
            raise AtomSampleViewerException(f'Data capturing did not complete in time for RHI {rhi}, got error: {e}')
