#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
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
        TIMEOUT 600
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
        TIMEOUT 1200
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
    ly_add_pytest(
        NAME AtomSampleViewer::PeriodicPerformanceBenchmarks
        PATH ${CMAKE_CURRENT_LIST_DIR}/Automated/benchmark_runner_periodic_suite.py
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
endif()
