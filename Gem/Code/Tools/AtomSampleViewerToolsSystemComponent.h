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

#include <AzCore/Component/Component.h>

namespace AtomSampleViewer
{
    namespace Tools
    {
        class AtomSampleViewerToolsSystemComponent final
            : public AZ::Component
        {
        public:
            AZ_COMPONENT(AtomSampleViewerToolsSystemComponent, "{113A0A09-5828-4035-8327-030D2CFCBD56}");
            static void Reflect(AZ::ReflectContext* context);

            AtomSampleViewerToolsSystemComponent() = default;
            AZ_DISABLE_COPY_MOVE(AtomSampleViewerToolsSystemComponent)

            void Activate() override;
            void Deactivate() override;

        private:
        };
    }
}