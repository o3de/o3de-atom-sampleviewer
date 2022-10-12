/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>

namespace AtomSampleViewer
{
    class AtomSampleComponent
        : public AZ::Component
    {
    public:
        AZ_RTTI(AtomSampleComponent, "{2318DFD6-BC6B-4335-9F25-8E270A10CA81}", AZ::Component);

        AtomSampleComponent() = default;
        ~AtomSampleComponent() override = default;

        // Redefine this string in the sample component subclass to provide a sample-specific warning message.
        // Any non-empty string will automatically cause a warning message to be displayed before opening the sample.
        static constexpr const char* ContentWarning = "";

        // If the above ContentWarning is overridden with a non-empty value, this string will be used as the message box's title.
        // Redefine this string in the sample component subclass to provide a custom title.
        static constexpr const char* ContentWarningTitle = "Content Warning";
        
        // This is a common photosensitive/seizure warning that could be used for the above ContentWarning in specific samples as needed.
        static constexpr const char CommonPhotosensitiveWarning[] = "This sample includes flashing images that could cause seizures or other adverse effects in photosensitive individuals.";
        static constexpr const char CommonPhotosensitiveWarningTitle[] = "Photosensitive Seizure Warning";
    };
} // namespace AtomSampleViewer
