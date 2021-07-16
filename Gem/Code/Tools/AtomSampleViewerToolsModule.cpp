/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Module/Module.h>
#include <Tools/AtomSampleViewerToolsSystemComponent.h>

namespace AtomSampleViewer
{
    namespace Tools
    {
        /**
         * Module for AtomSampleViewer code used in AssetProcessor and other tools.
         */
        class AtomSampleViewerToolsModule final
            : public AZ::Module
        {
        public:
            AZ_RTTI(AtomSampleViewerToolsModule, "{1945C729-77D2-4727-83FA-10E2271D62FE}", AZ::Module);

            AtomSampleViewerToolsModule()
            {
                m_descriptors.push_back(AtomSampleViewerToolsSystemComponent::CreateDescriptor());
            }

            AZ::ComponentTypeList GetRequiredSystemComponents() const override
            {
                return AZ::ComponentTypeList();
            }
        };
    } // namespace RPI
} // namespace AZ

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(AtomSampleViewerGem_2114099d7a8940c2813e93b8b417277e, AtomSampleViewer::Tools::AtomSampleViewerToolsModule);
