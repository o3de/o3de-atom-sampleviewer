#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(FILES
    ../../../../Resources/IOSLauncher/Info.plist
    AtomSampleViewerOptions_iOS.cpp
    EntityLatticeTestComponent_Traits_Platform.h
    MultiThreadComponent_Traits_Platform.h
    SceneReloadSoakTestComponent_Traits_Platform.h
    SSRExampleComponent_Traits_Platform.h
    TriangleConstantBufferExampleComponent_Traits_Platform.h
    SampleComponentManager_iOS.cpp
    StreamingImageExampleComponent_iOS.cpp
    Utils_iOS.cpp
    ScriptReporter_iOS.cpp
)

set(LY_COMPILE_OPTIONS
    PRIVATE
        -xobjective-c++
        -Wno-c++11-narrowing
)
