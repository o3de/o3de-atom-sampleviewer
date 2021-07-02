/*
 * Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
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