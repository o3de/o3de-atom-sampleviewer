/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Component/ComponentBus.h>

namespace AtomSampleViewer
{
    /**
     * ExampleComponentRequestBus provides an interface to request operations on an example component
     */
    class ExampleComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual void ResetCamera() = 0;
    };
    using ExampleComponentRequestBus = AZ::EBus<ExampleComponentRequests>;

} // namespace AtomSampleViewer
