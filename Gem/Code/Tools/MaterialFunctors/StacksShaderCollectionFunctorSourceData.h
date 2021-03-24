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
