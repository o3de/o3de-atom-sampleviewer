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

ly_get_list_relative_pal_filename(pal_dir ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/${PAL_PLATFORM_NAME})

ly_add_target(
    NAME AtomSampleViewer.Lib.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        atomsampleviewergem_lib_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Lib
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzGameFramework
            Gem::Atom_AtomBridge.Static
            Gem::Atom_RPI.Public
            Gem::Atom_Utils.Static
)

ly_add_target(
    NAME AtomSampleViewer.Private.Static STATIC
    NAMESPACE Gem
    FILES_CMAKE
        atomsampleviewergem_private_files.cmake
        ${pal_dir}/atomsampleviewer_${PAL_PLATFORM_NAME_LOWERCASE}_files.cmake
    PLATFORM_INCLUDE_FILES
        ${pal_dir}/platform_${PAL_PLATFORM_NAME_LOWERCASE}.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Lib
            Source
            ${pal_dir}
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzGameFramework
            Gem::Atom_AtomBridge.Static
            Gem::Atom_Feature_Common.Static
            Gem::Atom_Component_DebugCamera.Static
            Gem::AtomSampleViewer.Lib.Static
)

ly_add_target(
    NAME AtomSampleViewer ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE
        atomsampleviewergem_private_shared_files.cmake
        ../../atomsampleviewer_asset_files.cmake
    PLATFORM_INCLUDE_FILES
        ${pal_dir}/additional_${PAL_PLATFORM_NAME_LOWERCASE}_runtime_library.cmake
    BUILD_DEPENDENCIES
        PRIVATE
            AZ::AzGameFramework
            Gem::Atom_AtomBridge.Static
            Gem::AtomSampleViewer.Private.Static
            Gem::ImGui.imguilib
)

if(PAL_TRAIT_BUILD_HOST_TOOLS)

    ly_add_target(
        NAME AtomSampleViewer.Tools.Static STATIC
        NAMESPACE Gem
        FILES_CMAKE
            atomsampleviewergem_tools_files.cmake
        INCLUDE_DIRECTORIES
            PUBLIC
                .
                Tools
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AzGameFramework
                Gem::Atom_AtomBridge.Static
                Gem::AtomSampleViewer.Lib.Static
            PUBLIC
                Gem::Atom_RPI.Edit
    )

    ly_add_target(
        NAME AtomSampleViewer.Tools GEM_MODULE

        NAMESPACE Gem
        FILES_CMAKE
            atomsampleviewergem_tools_shared_files.cmake
        COMPILE_DEFINITIONS
            PUBLIC
        INCLUDE_DIRECTORIES
            PRIVATE
                .
                Source
                Source/Platform/${PAL_PLATFORM_NAME}
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzCore
                Gem::AtomSampleViewer.Tools.Static
    )

endif()

################################################################################
# Gem dependencies
################################################################################
ly_add_project_dependencies(
    PROJECT_NAME
        AtomSampleViewer
    TARGETS 
        AtomSampleViewer.GameLauncher
    DEPENDENCIES_FILES runtime_dependencies.cmake
)

if(PAL_TRAIT_BUILD_HOST_TOOLS)
    ly_add_project_dependencies(
        PROJECT_NAME
            AtomSampleViewer
        TARGETS
            AssetBuilder
            AssetProcessor
            AssetProcessorBatch
            Editor
        DEPENDENCIES_FILES tool_dependencies.cmake
    )
endif()

if(PAL_TRAIT_BUILD_SUPPORTS_SERVER)
    ly_add_project_dependencies(
        PROJECT_NAME
            AtomSampleViewer
        TARGETS
            AtomSampleViewerServer
        DEPENDENCIES_FILES runtime_dependencies.cmake
    )
endif()

################################################################################
# Tests
################################################################################
if(PAL_TRAIT_BUILD_SUPPORTS_TESTS)
    ly_add_target(
        NAME AtomSampleViewer.Tests MODULE

        NAMESPACE Gem
        FILES_CMAKE
            atomsampleviewer_tests_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Tests
        BUILD_DEPENDENCIES
            PRIVATE
                AZ::AzTest
                Gem::AtomSampleViewer
    )
    ly_add_googletest(
        NAME Gem::AtomSampleViewer.Tests
        TARGET Gem::AtomSampleViewer.Tests
    )
endif()
