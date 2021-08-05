/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <SampleComponentManager.h>

namespace AtomSampleViewer
{
    bool SampleComponentManager::IsMultiViewportSwapchainSampleSupported()
    {
        return true;
    }

    void SampleComponentManager::AdjustImGuiFontScale()
    {
    }
  
    const char* SampleComponentManager::GetRootPassTemplateName()
    {
        return "MainPipeline";
    }

    int SampleComponentManager::GutNumMSAASamples()
    {
        return 4;
    }
} // namespace AtomSampleViewer
