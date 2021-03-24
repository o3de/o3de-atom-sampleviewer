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

#include <MaterialFunctors/StacksShaderInputFunctor.h>
#include <Atom/RPI.Edit/Material/MaterialFunctorSourceData.h>

namespace AtomSampleViewer
{
    //! This is an example of a ShaderInputFunctorSourceData subclass which creates a MaterialFunctor during asset processing. 
    //! This is used with comprehensive.materialtype to transform angle values into a light direction vector.
    class StacksShaderInputFunctorSourceData final
        : public AZ::RPI::MaterialFunctorSourceData
    {
    public:
        AZ_RTTI(StacksShaderInputFunctorSourceData, "{6952C7F4-88F7-4840-8A29-4A3AE57F099C}", AZ::RPI::MaterialFunctorSourceData);

        static void Reflect(AZ::ReflectContext* context);

    private:
        FunctorResult CreateFunctor(const RuntimeContext& context) const override;
    };

} // namespace AtomSampleViewer
