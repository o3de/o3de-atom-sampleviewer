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
RENDER_HARDWARE_INTERFACE = 'dx12'
logger = logging.getLogger(__name__)


def teardown():
    process_utils.kill_processes_named(['AssetProcessor', 'AtomSampleViewerStandalone'], ignore_extensions=True)


@pytest.mark.parametrize('launcher_platform', ['windows'])
@pytest.mark.parametrize("project", ["AtomSampleViewer"])
@pytest.mark.usefixtures("automatic_process_killer")
class TestDX12Automation:

    @pytest.mark.test_case_id('C35638262')
    def test_C35638262_dx12_FullTestSuite(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638243')
    def test_C35638243_dx12_AreaLightTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638244')
    def test_C35638244_dx12_CheckerboardTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638245')
    def test_C35638245_dx12_CullingAndLod(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638246')
    def test_C35638246_dx12_Decals(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638247')
    def test_C35638247_dx12_DiffuseGITest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638248')
    def test_C35638248_dx12_DynamicDraw(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638249')
    def test_C35638249_dx12_DynamicMaterialTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638250')
    def test_C35638250_dx12_LightCulling(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638251')
    def test_C35638251_dx12_MaterialHotReloadTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638252')
    def test_C35638252_dx12_MaterialScreenshotTests(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638253')
    def test_C35638253_dx12_MSAA_RPI_Test(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638254')
    def test_C35638254_dx12_MultiRenderPipeline(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638255')
    def test_C35638255_dx12_MultiScene(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638256')
    def test_C35638256_dx12_ParallaxTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638257')
    def test_C35638257_dx12_SceneReloadSoakTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638258')
    def test_C35638258_dx12_ShadowedBistroTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638259')
    def test_C35638259_dx12_ShadowTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638260')
    def test_C35638260_dx12_StreamingImageTest(self, request, workspace, editor, launcher_platform):
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

    @pytest.mark.test_case_id('C35638261')
    def test_C35638261_dx12_TransparentTest(self, request, workspace, editor, launcher_platform):
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
