/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
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
        using AZ::RPI::MaterialFunctorSourceData::CreateFunctor;
        FunctorResult CreateFunctor(const RuntimeContext& context) const override;
    };

} // namespace AtomSampleViewer
