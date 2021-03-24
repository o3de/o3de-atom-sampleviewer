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

pytest.importorskip("ly_test_tools")  # Bail if LyTT isn't installed.
RENDER_HARDWARE_INTERFACE = 'vulkan'
logger = logging.getLogger(__name__)


def teardown():
    process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.usefixtures("automatic_process_killer")
class TestVulkanAutomation:

    @pytest.mark.test_case_id('C35638262')
    def test_C35638262_vulkan_FullTestSuite(self, request, workspace, editor, launcher_platform):
        test_script = '_FullTestSuite.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638263')
    def test_C35638263_vulkan_AreaLightTest(self, request, workspace, editor, launcher_platform):
        test_script = 'AreaLightTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638264')
    def test_C35638264_vulkan_CheckerboardTest(self, request, workspace, editor, launcher_platform):
        test_script = 'CheckerboardTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638265')
    def test_C35638265_vulkan_CullingAndLod(self, request, workspace, editor, launcher_platform):
        test_script = 'CullingAndLod.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638266')
    def test_C35638266_vulkan_Decals(self, request, workspace, editor, launcher_platform):
        test_script = 'Decals.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638267')
    def test_C35638267_vulkan_DiffuseGITest(self, request, workspace, editor, launcher_platform):
        test_script = 'DiffuseGITest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638268')
    def test_C35638268_vulkan_DynamicDraw(self, request, workspace, editor, launcher_platform):
        test_script = 'DynamicDraw.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638269')
    def test_C35638269_vulkan_DynamicMaterialTest(self, request, workspace, editor, launcher_platform):
        test_script = 'DynamicMaterialTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638270')
    def test_C35638270_vulkan_LightCulling(self, request, workspace, editor, launcher_platform):
        test_script = 'LightCulling.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638271')
    def test_C35638271_vulkan_MaterialHotReloadTest(self, request, workspace, editor, launcher_platform):
        test_script = 'MaterialHotReloadTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638272')
    def test_C35638272_vulkan_MaterialScreenshotTests(self, request, workspace, editor, launcher_platform):
        test_script = 'MaterialScreenshotTests.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638273')
    def test_C35638273_vulkan_MSAA_RPI_Test(self, request, workspace, editor, launcher_platform):
        test_script = 'MSAA_RPI_Test.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638274')
    def test_C35638274_vulkan_MultiRenderPipeline(self, request, workspace, editor, launcher_platform):
        test_script = 'MultiRenderPipeline.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638275')
    def test_C35638275_vulkan_MultiScene(self, request, workspace, editor, launcher_platform):
        test_script = 'MultiScene.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638276')
    def test_C35638276_vulkan_ParallaxTest(self, request, workspace, editor, launcher_platform):
        test_script = 'ParallaxTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638277')
    def test_C35638277_vulkan_SceneReloadSoakTest(self, request, workspace, editor, launcher_platform):
        test_script = 'SceneReloadSoakTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638278')
    def test_C35638278_vulkan_ShadowedBistroTest(self, request, workspace, editor, launcher_platform):
        test_script = 'ShadowedBistroTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638279')
    def test_C35638279_vulkan_ShadowTest(self, request, workspace, editor, launcher_platform):
        test_script = 'ShadowTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638280')
    def test_C35638280_vulkan_StreamingImageTest(self, request, workspace, editor, launcher_platform):
        test_script = 'StreamingImageTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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

    @pytest.mark.test_case_id('C35638281')
    def test_C35638281_vulkan_TransparentTest(self, request, workspace, editor, launcher_platform):
        test_script = 'TransparentTest.bv.luac'
        cmd = os.path.join(workspace.paths.build_directory(),
                           'AtomSampleViewerStandalone.exe '
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
