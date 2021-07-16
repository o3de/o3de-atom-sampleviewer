#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(FILES
    ../../../../Resources/MacLauncher/Info.plist
    AtomSampleViewerOptions_Mac.cpp
    EntityLatticeTestComponent_Traits_Platform.h
    MultiThreadComponent_Traits_Platform.h
    SceneReloadSoakTestComponent_Traits_Platform.h
    SSRExampleComponent_Traits_Platform.h
    TriangleConstantBufferExampleComponent_Traits_Platform.h
    SampleComponentManager_Mac.cpp
    StreamingImageExampleComponent_Mac.cpp
    Utils_Mac.cpp
    ScriptReporter_Mac.cpp
)

set(LY_COMPILE_OPTIONS
    PRIVATE
        -xobjective-c++
        -Wno-c++11-narrowing
)