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

#include <MaterialFunctors/StacksShaderInputFunctorSourceData.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayout.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void StacksShaderInputFunctorSourceData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StacksShaderInputFunctorSourceData>()
                ->Version(2)
                ;
        }
    }

    AZ::RPI::MaterialFunctorSourceData::FunctorResult StacksShaderInputFunctorSourceData::CreateFunctor(const RuntimeContext& context) const
    {
        AZ::RPI::Ptr<StacksShaderInputFunctor> functor = aznew StacksShaderInputFunctor;
        functor->m_azimuthDegreesIndex = context.FindMaterialPropertyIndex(AZ::Name{ "light.azimuthDegrees" });
        functor->m_elevationDegreesIndex = context.FindMaterialPropertyIndex(AZ::Name{ "light.elevationDegrees" });
        AddMaterialPropertyDependency(functor, functor->m_azimuthDegreesIndex);
        AddMaterialPropertyDependency(functor, functor->m_elevationDegreesIndex);
        functor->m_lightDirectionIndex = context.GetShaderResourceGroupLayout()->FindShaderInputConstantIndex(AZ::Name{ "m_lightDir" });
        return AZ::Success(AZ::RPI::Ptr<AZ::RPI::MaterialFunctor>(functor));
    }
}
