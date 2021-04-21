"""
All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
its licensors.

For complete copyright and license terms please see the LICENSE at the root of this
distribution (the "License"). All use of this software is governed by the License,
or, if provided, by the license below or the license accompanying this file. Do not
remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

pytest test configuration file for launching AtomSampleViewerStandalone tests.
"""

import logging
import os
import types

import pytest

import ly_test_tools.builtin.helpers as helpers
import ly_test_tools.environment.file_system as file_system
import ly_test_tools.log.log_monitor

logger = logging.getLogger(__name__)


@pytest.fixture(scope="function", autouse=True)
def clean_atomsampleviewer_logs(request, workspace):
    """Deletes any AtomSampleViewer log files so that the test run can start with empty logs."""
    logs = ['atomsampleviewer.log']
    logger.info(f'Deleting log files for AtomSampleViewer tests: {logs}')

    for log in logs:
        log_file = os.path.join(workspace.paths.project_log(), log)
        if os.path.exists(log_file):
            file_system.delete(file_list=[log_file],
                               del_files=True,
                               del_dirs=False)


@pytest.fixture(scope="function", autouse=True)
def atomsampleviewer_log_monitor(request, workspace):
    """Creates a LyTestTools log monitor object for monitoring AtomSampleViewer logs."""
    def is_alive(launcher_name):
        return True

    launcher = ly_test_tools.launchers.platforms.base.Launcher(workspace, [])  # Needed for log monitor to work.
    launcher.is_alive = types.MethodType(is_alive, launcher)
    file_to_monitor = os.path.join(workspace.paths.project_log(), 'atomsampleviewer.log')
    log_monitor = ly_test_tools.log.log_monitor.LogMonitor(launcher=launcher, log_file_path=file_to_monitor)
    log_monitor.file_to_monitor = file_to_monitor

    return log_monitor
