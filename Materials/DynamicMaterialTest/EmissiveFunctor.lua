----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

function GetMaterialPropertyDependencies()
    return {"emissive.useTexture", "emissive.textureMap", "emissive.intensity", "emissive.unit"}
end

function GetShaderOptionDependencies()
    return {"o_emissive_useTexture"}
end

 
-- These values are based on the Intensity enum defined in the .materialtype file
LightUnitProperty_EV100 = 0
LightUnitProperty_Nit = 1

function Process(context)

    local useTexture = context:GetMaterialPropertyValue_bool("emissive.useTexture")
    local textureMap = context:GetMaterialPropertyValue_Image("emissive.textureMap")
    
    context:SetShaderOptionValue_bool("o_emissive_useTexture", useTexture and textureMap ~= nil)

    local lightUnit = context:GetMaterialPropertyValue_enum("emissive.unit")
    local intensity = context:GetMaterialPropertyValue_float("emissive.intensity")

    if(LightUnitProperty_EV100 == lightUnit) then
        local Ev100LightMeterConstantLuminance = 12.5
        local Ev100Iso = 100.0
        local Ev100ShutterSpeed = 1.0
        intensity = (Ev100LightMeterConstantLuminance * Math.Pow(2.0, intensity)) / (Ev100Iso * Ev100ShutterSpeed)
    end

    context:SetShaderConstant_float("m_emissiveIntensity", intensity)
end

function ProcessEditor(context)
    
    local textureMap = context:GetMaterialPropertyValue_Image("emissive.textureMap")

    if(nil == textureMap) then
        context:SetMaterialPropertyVisibility("emissive.useTexture", MaterialPropertyVisibility_Disabled)
    else
        context:SetMaterialPropertyVisibility("emissive.useTexture", MaterialPropertyVisibility_Enabled)
    end

    -- Update range values based on light units
    
    local lightUnit = context:GetMaterialPropertyValue_enum("emissive.unit")
    
    if(LightUnitProperty_EV100 == lightUnit) then
        context:SetMaterialPropertyMinValue_float("emissive.intensity", -10)
        context:SetMaterialPropertyMaxValue_float("emissive.intensity", 20)
        context:SetMaterialPropertySoftMinValue_float("emissive.intensity", -6)
        context:SetMaterialPropertySoftMaxValue_float("emissive.intensity", 16)
    elseif(LightUnitProperty_Nit == lightUnit) then
        context:SetMaterialPropertyMinValue_float("emissive.intensity", 0.0)
        context:SetMaterialPropertyMaxValue_float("emissive.intensity", 100000.0)
        context:SetMaterialPropertySoftMinValue_float("emissive.intensity", 0.001)
        context:SetMaterialPropertySoftMaxValue_float("emissive.intensity", 20.0)
    else
        Error("Unrecognized light unit.")
    end
    
end

