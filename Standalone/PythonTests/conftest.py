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

import pytest

import ly_test_tools.environment.file_system as file_system

logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption("--clean-logs", action="store_true", default=False,
                     help="Deletes any existing AssetProcessor, AtomSampleViewer, Editor, or Game log files.")


@pytest.fixture(scope="function", autouse=True)
def clean_logs(request, workspace):
    enable_clean_logs = request.config.getoption('--clean-logs')
    logs = ['AP_GUI.log', 'atomsampleviewer.log', 'Editor.log', 'Game.log']

    if not enable_clean_logs:
        logger.info(f'--clean-logs is "{enable_clean_logs}" - skipping the deletion of existing log files: {logs}')
        return

    logger.info(f'--clean-logs is "{enable_clean_logs}" - deleting log files: {logs}')
    for log in logs:
        log_file = os.path.join(workspace.paths.project_log(), log)
        if os.path.exists(log_file):
            file_system.delete(file_list=[log_file],
                               del_files=True,
                               del_dirs=False)
