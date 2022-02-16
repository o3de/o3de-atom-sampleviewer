/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Performance/HighInstanceExampleComponent.h>

namespace AtomSampleViewer
{
    //! This test loads a 22x22x22 (~10K) lattice of cubes with a white material, shadowed lights are added to the scene until 100K draws are required. 
    //! This test benchmarks how well atom scales cpu time with the number of draws. 
    class _100KDraw10KDrawableExampleComponent
        : public HighInstanceTestComponent
    {
        using Base = HighInstanceTestComponent;

    public:
        AZ_COMPONENT(_100KDraw10KDrawableExampleComponent, "{85C4BEE0-9FBC-4E9A-8425-12555703CC61}", HighInstanceTestComponent);

        static void Reflect(AZ::ReflectContext* context);

        _100KDraw10KDrawableExampleComponent();

    private:
        AZ_DISABLE_COPY_MOVE(_100KDraw10KDrawableExampleComponent);

        static void InitDefaultValues(HighInstanceTestParameters& defaultParameters);
    };
} // namespace AtomSampleViewer
