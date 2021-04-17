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

RENDER_HARDWARE_INTERFACE = 'vulkan'
logger = logging.getLogger(__name__)


def teardown():
    process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.usefixtures("clean_atomsampleviewer_logs")
class TestVulkanAutomationMainSuite:

    @pytest.mark.test_case_id('C35638265')
    def test_C35638265_vulkan_CullingAndLod(self, request, workspace, editor, launcher_platform):
        test_script = 'CullingAndLod.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
                           f'--project-path={workspace.paths.project()} '
                           f'--rhi {RENDER_HARDWARE_INTERFACE} '
                           f'--runtestsuite scripts/{test_script} '
                           '--exitontestend')
        request.addfinalizer(teardown)

        try:
            return_code = process_utils.check_call(cmd, stderr=subprocess.STDOUT, encoding='UTF-8', shell=True)
            logger.debug(f"AtomSampleViewer {test_script} test command got response return code : {return_code}")
            assert return_code == 0
        except subprocess.CalledProcessError as e:
            logger.error(f'AtomSampleViewer lua test "{test_script}" had a failure.\n')
            raise e
