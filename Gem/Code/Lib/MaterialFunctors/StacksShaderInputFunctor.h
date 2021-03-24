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

#include <Atom/RPI.Reflect/Material/MaterialFunctor.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertyDescriptor.h>

namespace AtomSampleViewer
{
    //! This is an example of a custom hard-coded MaterialFunctor used to perform calculations on material 
    //! property values to produce shader input values.
    //! It is used by comprehensive.materialtype to transform angle values into a light direction vector.
    class StacksShaderInputFunctor final
        : public AZ::RPI::MaterialFunctor
    {
        friend class StacksShaderInputFunctorSourceData;
    public:
        AZ_RTTI(StacksShaderInputFunctor, "{7F607170-1BC2-4510-A252-8A665FC02052}", AZ::RPI::MaterialFunctor);

        static void Reflect(AZ::ReflectContext* context);

        void Process(RuntimeContext& context) override;

    private:

        // Indices used to look up material property values at runtime
        AZ::RPI::MaterialPropertyIndex m_azimuthDegreesIndex;
        AZ::RPI::MaterialPropertyIndex m_elevationDegreesIndex;

        // Indices used to look up ShaderResourceGroup inputs at runtime
        AZ::RHI::ShaderInputConstantIndex m_lightDirectionIndex;
    };

} // namespace AtomSampleViewer
