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

logger = logging.getLogger(__name__)


class AtomSampleViewerException(Exception):
    """Custom Exception class for AtomSampleViewer tests."""
    pass


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.parametrize('rhi', ['dx12'])
@pytest.mark.usefixtures("clean_atomsampleviewer_logs", "atomsampleviewer_log_monitor")
class TestAutomationWarpSuite:

    def test_AutomatedReviewWARPTestSuite(self, request, workspace, launcher_platform, rhi, project, atomsampleviewer_log_monitor):
        # Script call setup.
        test_script = '_AutomatedReviewWARPTestSuite_.bv.lua'
        test_script_path = os.path.join(workspace.paths.project(), 'Scripts', test_script)
        if not os.path.exists(test_script_path):
            raise AtomSampleViewerException(f'Test script does not exist in path: {test_script_path}')
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
                           '-forceAdapter="Microsoft Basic Render Driver" '
                           f'--project-path={workspace.paths.project()} '
                           f'--rhi {rhi} '
                           f'--runtestsuite scripts/{test_script} '
                           '--exitontestend')

        def teardown():
            process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)
        request.addfinalizer(teardown)

        # Execute test.
        process_utils.safe_check_call(cmd, stderr=subprocess.STDOUT, encoding='UTF-8', shell=True)
        try:
            unexpected_lines = ["Script: Screenshot check failed. Diff score",  # "Diff score" ensures legit failure.
                                "Trace::Error",
                                "Trace::Assert",
                                "Traceback (most recent call last):"]
            atomsampleviewer_log_monitor.monitor_log_for_lines(
                unexpected_lines=unexpected_lines, halt_on_unexpected=True, timeout=50)
        except ly_test_tools.log.log_monitor.LogMonitorException as e:
            expected_screenshots_path = os.path.join(
                workspace.paths.project(), "Scripts", "ExpectedScreenshots")
            test_screenshots_path = os.path.join(
                workspace.paths.project(), "user", "Scripts", "Screenshots")
            raise AtomSampleViewerException(
                f"Got error: {e}\n"
                f"Screenshot comparison check failed using Render Hardware Interface (RHI): '{rhi}'\n"
                "Please review logs and screenshots at:\n"
                f"Log file: {atomsampleviewer_log_monitor.file_to_monitor}\n"
                f"Expected screenshots: {expected_screenshots_path}\n"
                f"Test screenshots: {test_screenshots_path}\n")
