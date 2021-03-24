/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
