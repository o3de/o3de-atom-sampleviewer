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


ly_get_list_relative_pal_filename(pal_dir ${CMAKE_CURRENT_LIST_DIR}/Platform/${PAL_PLATFORM_NAME})

include(${pal_dir}/atomsampleviewer_traits_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

if (NOT LY_MONOLITHIC_GAME)
    if (ATOMSAMPLEVIEWER_TRAIT_BUILD_SUPPORTS_STANDALONE)
        ly_add_target(
            NAME AtomSampleViewerStandalone EXECUTABLE
            NAMESPACE AZ
            FILES_CMAKE
                atomsample_viewer_files.cmake
                ${pal_dir}/platform_${PAL_PLATFORM_NAME_LOWERCASE}_files.cmake
            INCLUDE_DIRECTORIES
                PUBLIC
                    .
                    Platform/Common
                PRIVATE
                    ${pal_dir}
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AtomCore
                    AZ::AzCore
                    AZ::AzGameFramework
                    Gem::Atom_RPI.Public
                    Gem::AtomSampleViewer.Private.Static
        )

        add_subdirectory(PythonTests)

        ly_add_target_dependencies(
            TARGETS 
                AtomSampleViewerStandalone
            DEPENDENCIES_FILES 
                ../Gem/Code/runtime_dependencies.cmake
                runtime_dependencies.cmake
        )

        # Adds the AtomSampleViewerStandalone target as a C preprocessor define so that it can be used as a Settings Registry
        # specialization in order to look up the generated .setreg which contains the dependencies
        # specified for the target.
        if(TARGET AtomSampleViewerStandalone)
            set_source_files_properties(
                Platform/Common/AtomSampleViewerApplication.cpp
                PROPERTIES
                    COMPILE_DEFINITIONS
                        LY_CMAKE_TARGET="AtomSampleViewerStandalone"
            )
        else()
            message(FATAL_ERROR "Cannot set LY_CMAKE_TARGET define to AtomSampleViewerStandalone as the target doesn't exist anymore."
                " Perhaps it has been renamed")
        endif()
    endif()
endif()
