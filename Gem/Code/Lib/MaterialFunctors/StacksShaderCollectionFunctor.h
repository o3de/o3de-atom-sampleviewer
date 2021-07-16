/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Reflect/Material/MaterialFunctor.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertyDescriptor.h>

namespace AtomSampleViewer
{
    //! This is an example of a custom hard-coded MaterialFunctor used to dynamically select shaders/variants/passes.
    //! It is used by comprehensive.materialtype to enable/disable variants of the "stacks" shader.
    class StacksShaderCollectionFunctor final
        : public AZ::RPI::MaterialFunctor
    {
        friend class StacksShaderCollectionFunctorSourceData;
    public:
        AZ_RTTI(StacksShaderCollectionFunctor, "{4E51A7D5-7DF1-4402-8975-F6C9DFDEDC1E}", AZ::RPI::MaterialFunctor);

        static void Reflect(AZ::ReflectContext* context);

        void Process(RuntimeContext& context) override;

    private:

        // Indexes used to look up material property values at runtime
        AZ::RPI::MaterialPropertyIndex m_stackCountProperty;
        AZ::RPI::MaterialPropertyIndex m_highlightLastStackProperty;

        // Indexes used to access ShaderOption values at runtime
        AZ::RPI::ShaderOptionIndex m_highlightLastStackOption;
    };
} // namespace AtomSampleViewer
