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
        return false;
    }

    void SampleComponentManager::AdjustImGuiFontScale()
    {
        // Scale the text and the general size for mobile platforms because the screens are smaller.
        const float fontScale = 1.5f;
        const float sizeScale = 2.0f;
        AZ::Render::ImGuiSystemRequestBus::Broadcast(&AZ::Render::ImGuiSystemRequestBus::Events::SetGlobalSizeScale, sizeScale);
        AZ::Render::ImGuiSystemRequestBus::Broadcast(&AZ::Render::ImGuiSystemRequestBus::Events::SetGlobalFontScale, fontScale);
    }

    const char* SampleComponentManager::GetRootPassTemplateName()
    {
        return "LowEndPipelineTemplate";
    }

    int SampleComponentManager::GutNumMSAASamples()
    {
        return 1;
    }
} // namespace AtomSampleViewer
