/*
 * Copyright (c) Contributors to the Open 3D Engine Project
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/EBus/EBus.h>

namespace AtomSampleViewer
{
    class AtomSampleViewerRequests
        : public AZ::EBusTraits
    {
    public:
        //! Return the specified exit code when exiting AtomSampleViewer
        virtual void SetExitCode(int exitCode) = 0;
    };
    using AtomSampleViewerRequestsBus = AZ::EBus<AtomSampleViewerRequests>;

} // namespace AtomSampleViewer
