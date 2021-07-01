/*
 * Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <MaterialFunctors/StacksShaderCollectionFunctor.h>
#include <Atom/RPI.Edit/Material/MaterialFunctorSourceData.h>

namespace AtomSampleViewer
{
    class StacksShaderCollectionFunctor;

    //! This is an example of a MaterialFunctorSourceData subclass which creates a 
    //! MaterialFunctor during asset processing. 
    //! This is used with comprehensive.material to enable/disable variants of the "stacks" shader.
    class StacksShaderCollectionFunctorSourceData final
        : public AZ::RPI::MaterialFunctorSourceData
    {
    public:
        AZ_RTTI(StacksShaderCollectionFunctorSourceData, "{2B4678D5-5C1B-4BEE-99E5-9EC9FB871D37}", AZ::RPI::MaterialFunctorSourceData);

        static void Reflect(AZ::ReflectContext* context);

        FunctorResult CreateFunctor(const RuntimeContext& context) const override;
    };
} // namespace AtomSampleViewer
