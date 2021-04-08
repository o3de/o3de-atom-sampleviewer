"""
All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
its licensors.

For complete copyright and license terms please see the LICENSE at the root of this
distribution (the "License"). All use of this software is governed by the License,
or, if provided, by the license below or the license accompanying this file. Do not
remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
"""
import logging
import os
import subprocess
import types

import pytest

import ly_test_tools.environment.process_utils as process_utils
import ly_test_tools.launchers.platforms.base
import ly_test_tools.log.log_monitor

RENDER_HARDWARE_INTERFACE = 'dx12'
logger = logging.getLogger(__name__)


class AtomSampleViewerException(Exception):
    """Custom Exception class for AtomSampleViewer tests."""
    pass


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.usefixtures("setup_atomsampleviewer_assets", "clean_atomsampleviewer_logs")
class TestDX12AutomationMainSuite:

    @pytest.mark.test_case_id('C35638245')
    def test_C35638245_dx12_CullingAndLod(self, request, workspace, launcher_platform):
        # Script call setup.
        test_script = 'CullingAndLod.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
                           f'--rhi {RENDER_HARDWARE_INTERFACE} '
                           f'--runtestsuite scripts/{test_script} '
                           '--exitontestend')

        def teardown():
            process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)

        request.addfinalizer(teardown)

        # Log monitor setup.
        def is_alive(launcher_name):
            return True
        launcher = ly_test_tools.launchers.platforms.base.Launcher(workspace, [])  # Needed for log monitor to work.
        launcher.is_alive = types.MethodType(is_alive, launcher)
        file_to_monitor = os.path.join(workspace.paths.project_log(), 'atomsampleviewer.log')
        log_monitor = ly_test_tools.log.log_monitor.LogMonitor(launcher=launcher, log_file_path=file_to_monitor)

        # Execute test.
        process_utils.safe_check_call(cmd, stderr=subprocess.STDOUT, encoding='UTF-8', shell=True)
        try:
            unexpected_lines = ["Script: Screenshot check failed. Diff score"]  # "Diff score" ensures legit failure.
            log_monitor.monitor_log_for_lines(unexpected_lines=unexpected_lines, halt_on_unexpected=True, timeout=5)
        except ly_test_tools.log.log_monitor.LogMonitorException as e:
            expected_screenshots_path = os.path.join(
                workspace.paths.dev(), "AtomSampleViewer", "Scripts", "ExpectedScreenshots")
            test_screenshots_path = os.path.join(
                workspace.paths.project_cache(), "pc", "user", "Scripts", "Screenshots")
            raise AtomSampleViewerException(
                f"Got error: {e}\n"
                "Screenshot comparison check failed. Please review logs and screenshots at:\n"
                f"Log file: {file_to_monitor}\n"
                f"Expected screenshots: {expected_screenshots_path}\n"
                f"Test screenshots: {test_screenshots_path}\n")
