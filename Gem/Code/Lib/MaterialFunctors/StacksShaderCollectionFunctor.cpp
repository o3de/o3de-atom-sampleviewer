/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MaterialFunctors/StacksShaderCollectionFunctor.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>

namespace AtomSampleViewer
{
    void StacksShaderCollectionFunctor::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StacksShaderCollectionFunctor, AZ::RPI::MaterialFunctor>()
                ->Version(1)
                ->Field("m_stackCountProperty", &StacksShaderCollectionFunctor::m_stackCountProperty)
                ->Field("m_highlightLastStackProperty", &StacksShaderCollectionFunctor::m_highlightLastStackProperty)
                ->Field("m_highlightLastStackOption", &StacksShaderCollectionFunctor::m_highlightLastStackOption)
                ;
        }
    }

    void StacksShaderCollectionFunctor::Process(RuntimeContext& context)
    {
        using namespace AZ::RPI;

        const uint32_t stackCount = context.GetMaterialPropertyValue<uint32_t>(m_stackCountProperty);
        const bool highlightLastStack = context.GetMaterialPropertyValue<bool>(m_highlightLastStackProperty);

        static const int AvailableStackCount = 4;
        for (uint32_t i = 0; i < AvailableStackCount; ++i)
        {
            const bool isLastStack = (i == stackCount - 1);
            const bool shouldHighlight = highlightLastStack && isLastStack;
            context.SetShaderOptionValue(i, m_highlightLastStackOption, ShaderOptionValue{ shouldHighlight });
            context.SetShaderEnabled(i, i < stackCount);
        }
    }
} // namespace AtomSampleViewer
