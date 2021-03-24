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

#include <MaterialFunctors/StacksShaderCollectionFunctorSourceData.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroupLayout.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    void StacksShaderCollectionFunctorSourceData::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StacksShaderCollectionFunctorSourceData>()
                ->Version(2)
                ;
        }
    }

    AZ::RPI::MaterialFunctorSourceData::FunctorResult StacksShaderCollectionFunctorSourceData::CreateFunctor(const RuntimeContext& context) const
    {
        using namespace AZ;
        using namespace AZ::RPI;

        Ptr<StacksShaderCollectionFunctor> functor = aznew StacksShaderCollectionFunctor;

        functor->m_stackCountProperty = context.FindMaterialPropertyIndex(Name("stacks.stackCount"));
        functor->m_highlightLastStackProperty = context.FindMaterialPropertyIndex(Name("stacks.highlightLastStack"));
        AddMaterialPropertyDependency(functor, functor->m_stackCountProperty);
        AddMaterialPropertyDependency(functor, functor->m_highlightLastStackProperty);

        // StacksShaderCollectionFunctorSourceData directly corresponds to Comprehensive.material, which uses the same ShaderAsset for all passes, 
        // so we can just use the ShaderOptionGroupLayout from the first ShaderAsset.
        functor->m_highlightLastStackOption = context.FindShaderOptionIndex(0, AZ::Name{"o_highlighted2"});

        return Success(Ptr<MaterialFunctor>(functor));
    }
}
