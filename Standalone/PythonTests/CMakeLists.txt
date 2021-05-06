#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#

################################################################################
# Automated Tests
################################################################################

if(PAL_TRAIT_BUILD_HOST_TOOLS AND PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    ly_add_pytest(
        NAME AtomSampleViewer::PythonMainTests
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/test_AtomSampleViewer_main_suite.py
        TEST_REQUIRES gpu
        TEST_SUITE main
        TEST_SERIAL
        TIMEOUT 300
        RUNTIME_DEPENDENCIES
            AssetProcessor
            AssetProcessorBatch
            AtomSampleViewerStandalone
            AtomSampleViewer.GameLauncher
            AtomSampleViewer.Assets
    )
    ly_add_pytest(
        NAME AtomSampleViewer::PythonPeriodicTests
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/test_AtomSampleViewer_periodic_suite.py
        TEST_REQUIRES gpu
        TEST_SUITE periodic
        TEST_SERIAL
        TIMEOUT 300
        RUNTIME_DEPENDENCIES
            AssetProcessor
            AssetProcessorBatch
            AtomSampleViewerStandalone
            AtomSampleViewer.GameLauncher
            AtomSampleViewer.Assets
    )
    ly_add_pytest(
        NAME AtomSampleViewer::PythonWARPTests
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/test_AtomSampleViewer_warp_suite.py
        TEST_SUITE main
        TEST_SERIAL
        TIMEOUT 300
        RUNTIME_DEPENDENCIES
            AssetProcessor
            AssetProcessorBatch
            AtomSampleViewerStandalone
            AtomSampleViewer.GameLauncher
            AtomSampleViewer.Assets
    )
endif()
