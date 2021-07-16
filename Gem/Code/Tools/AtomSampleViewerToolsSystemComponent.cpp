/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Tools/AtomSampleViewerToolsSystemComponent.h>
#include <MaterialFunctors/StacksShaderCollectionFunctorSourceData.h>
#include <MaterialFunctors/StacksShaderInputFunctorSourceData.h>

#include <Atom/RPI.Edit/Material/MaterialFunctorSourceDataRegistration.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AtomSampleViewer
{
    namespace Tools
    {
        void AtomSampleViewerToolsSystemComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serialize->Class<AtomSampleViewerToolsSystemComponent, AZ::Component>()
                    ->Version(0)
                ;
            }

            StacksShaderCollectionFunctorSourceData::Reflect(context);
            StacksShaderInputFunctorSourceData::Reflect(context);
        }

        void AtomSampleViewerToolsSystemComponent::Activate()
        {
            AZ::RPI::MaterialFunctorSourceDataRegistration::Get()->RegisterMaterialFunctor("StacksShaderCollection", azrtti_typeid<StacksShaderCollectionFunctorSourceData>());
            AZ::RPI::MaterialFunctorSourceDataRegistration::Get()->RegisterMaterialFunctor("StacksShaderInput",      azrtti_typeid<StacksShaderInputFunctorSourceData>());
        }

        void AtomSampleViewerToolsSystemComponent::Deactivate()
        {
        }
    } // namespace Tools
} // namespace AtomSampleViewer
