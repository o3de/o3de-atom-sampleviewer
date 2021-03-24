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

#include <MaterialFunctors/StacksShaderInputFunctor.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Math/Matrix4x4.h>

namespace AtomSampleViewer
{
    void StacksShaderInputFunctor::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StacksShaderInputFunctor, AZ::RPI::MaterialFunctor>()
                ->Version(1)
                ->Field("m_azimuthDegreesIndex", &StacksShaderInputFunctor::m_azimuthDegreesIndex)
                ->Field("m_elevationDegreesIndex", &StacksShaderInputFunctor::m_elevationDegreesIndex)
                ->Field("m_lightDirectionIndex", &StacksShaderInputFunctor::m_lightDirectionIndex)
                ;
        }
    }

    void StacksShaderInputFunctor::Process(RuntimeContext& context)
    {
        float azimuthDegrees = context.GetMaterialPropertyValue<float>(m_azimuthDegreesIndex);
        float elevationDegrees = context.GetMaterialPropertyValue<float>(m_elevationDegreesIndex);

        AZ::Vector3 lightDir = AZ::Vector3(1,0,0) * AZ::Matrix4x4::CreateRotationZ(AZ::DegToRad(elevationDegrees)) * AZ::Matrix4x4::CreateRotationY(AZ::DegToRad(azimuthDegrees));

        float floats[3];
        lightDir.StoreToFloat3(floats);
        context.GetShaderResourceGroup()->SetConstantRaw(m_lightDirectionIndex, floats, 3 * sizeof(float));
    }

} // namespace AtomSampleViewer
