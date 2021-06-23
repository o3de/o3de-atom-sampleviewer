/*
 * Copyright (c) Contributors to the Open 3D Engine Project
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
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
