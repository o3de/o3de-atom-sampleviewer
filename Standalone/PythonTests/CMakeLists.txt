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
# Tests are not in their final/desired suites yet - see ATOM-14520
################################################################################

if(PAL_TRAIT_BUILD_HOST_TOOLS AND PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    ly_add_pytest(
        NAME AtomSampleViewer::PythonTestsMain
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/test_suites/main/
        TEST_REQUIRES gpu
        TEST_SUITE main
        TEST_SERIAL
        TIMEOUT 1200
        RUNTIME_DEPENDENCIES
            AssetProcessor
            AssetProcessorBatch
            AtomSampleViewerStandalone
            AtomSampleViewer.GameLauncher
            AtomSampleViewer.Assets
    )
    ly_add_pytest(
        NAME AtomSampleViewer::PythonTestsMainPeriodic
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/test_suites/periodic/
        TEST_REQUIRES gpu
        TEST_SUITE periodic
        TEST_SERIAL
        TIMEOUT 1200
        RUNTIME_DEPENDENCIES
            AssetProcessor
            AssetProcessorBatch
            AtomSampleViewerStandalone
            AtomSampleViewer.GameLauncher
            AtomSampleViewer.Assets
    )
endif()
