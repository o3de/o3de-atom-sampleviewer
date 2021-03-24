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
        return "MainPipeline_Mobile";
    }
} // namespace AtomSampleViewer
