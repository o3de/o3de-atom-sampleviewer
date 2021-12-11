/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Module/Module.h>
#include <AzCore/Memory/SystemAllocator.h>

#include <AtomSampleViewerSystemComponent.h>
#include <SampleComponentManager.h>

#include <AzFramework/Scene/SceneSystemComponent.h>

namespace AtomSampleViewer
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator, 0);
        AZ_RTTI(Module, "{8FEB7E9B-A5F7-4917-A1DE-974DE1FA7F1E}", AZ::Module);

        Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
                AtomSampleViewerSystemComponent::CreateDescriptor(),
                SampleComponentManager::CreateDescriptor(),
                });

            AZStd::array_view<SampleEntry> samples = SampleComponentManager::GetSamples();
            for (const SampleEntry& sample : samples)
            {
                m_descriptors.emplace_back(sample.m_componentDescriptor);
            }
        }

        ~Module() override = default;

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            AZ::ComponentTypeList requiredComponents;
    
            requiredComponents = 
            {
                azrtti_typeid<AzFramework::SceneSystemComponent>(),
                azrtti_typeid<AtomSampleViewerSystemComponent>(),
                azrtti_typeid<SampleComponentManager>()
            };
    
            return requiredComponents;
        }
    };
} // namespace AtomSampleViewer

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Gem_AtomSampleViewer, AtomSampleViewer::Module)
