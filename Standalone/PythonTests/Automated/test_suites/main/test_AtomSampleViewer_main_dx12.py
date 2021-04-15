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

import pytest

import ly_test_tools.environment.process_utils as process_utils
import ly_test_tools.launchers.platforms.base

RENDER_HARDWARE_INTERFACE = 'dx12'
logger = logging.getLogger(__name__)


class AtomSampleViewerException(Exception):
    """Custom Exception class for AtomSampleViewer tests."""
    pass


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.usefixtures("clean_atomsampleviewer_logs", "atomsampleviewer_log_monitor")
class TestDX12AutomationMainSuite:

    def test_dx12_MainTestSuite(self, request, workspace, launcher_platform, atomsampleviewer_log_monitor):
        # Script call setup.
        test_script = '_MainTestSuite_.bv.lua'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
                           f'--project-path={workspace.paths.project()} '
                           f'--rhi {RENDER_HARDWARE_INTERFACE} '
                           f'--runtestsuite scripts/{test_script} '
                           '--exitontestend')

        def teardown():
            process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)
        request.addfinalizer(teardown)

        # Execute test.
        process_utils.safe_check_call(cmd, stderr=subprocess.STDOUT, encoding='UTF-8', shell=True)
        try:
            unexpected_lines = ["Script: Screenshot check failed. Diff score"]  # "Diff score" ensures legit failure.
            atomsampleviewer_log_monitor.monitor_log_for_lines(
                unexpected_lines=unexpected_lines, halt_on_unexpected=True, timeout=5)
        except ly_test_tools.log.log_monitor.LogMonitorException as e:
            expected_screenshots_path = os.path.join(
                workspace.paths.engine_root(), "AtomSampleViewer", "Scripts", "ExpectedScreenshots")
            test_screenshots_path = os.path.join(
                workspace.paths.project(), "user", "Scripts", "Screenshots")
            raise AtomSampleViewerException(
                f"Got error: {e}\n"
                "Screenshot comparison check failed. Please review logs and screenshots at:\n"
                f"Log file: {atomsampleviewer_log_monitor.file_to_monitor}\n"
                f"Expected screenshots: {expected_screenshots_path}\n"
                f"Test screenshots: {test_screenshots_path}\n")
