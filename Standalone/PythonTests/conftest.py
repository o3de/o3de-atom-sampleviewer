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

import ly_test_tools.builtin.helpers as helpers
import ly_test_tools.environment.file_system as file_system

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

