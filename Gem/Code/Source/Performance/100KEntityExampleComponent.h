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
    /*
        This test loads a 46x46x46 lattice of entities with randomized meshes and materials. This test is used as a simple cpu performance stress test for Atom.

        The assets that are applied to the entities are chosen from the 
        "allow-list" of models and materials.
    */
    class HundredKEntityExampleComponent
        : public HighInstanceTestComponent
    {
        using Base = HighInstanceTestComponent;

    public:
        AZ_COMPONENT(HundredKEntityExampleComponent, "{A373EDC7-399F-49BB-A3A9-D81FA7E05E60}", HighInstanceTestComponent);

        static void Reflect(AZ::ReflectContext* context);

        HundredKEntityExampleComponent();

    private:
        AZ_DISABLE_COPY_MOVE(HundredKEntityExampleComponent);
    };
} // namespace AtomSampleViewer
